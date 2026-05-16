#include "omni_icon_image.h"

#include <string.h>
#include "quantum.h"
#include "qp_internal.h"
#include "qp_comms.h"

#if defined(RP2040)
#    include "hardware/flash.h"
#    include "hardware/sync.h"
#    include "hardware/regs/addressmap.h"
#endif

#ifndef OMNI_ICON_FLASH_OFFSET
#    define OMNI_ICON_FLASH_OFFSET 0x150000u
#endif

#ifndef OMNI_ICON_SLOT_SIZE
#    define OMNI_ICON_SLOT_SIZE 0x4000u
#endif

#define OMNI_ICON_SLOT_COUNT 28u
#define OMNI_ICON_HEADER_SIZE      256u
#define OMNI_ICON_DATA_OFFSET      (OMNI_ICON_FLASH_OFFSET + OMNI_ICON_HEADER_SIZE)
#define OMNI_ICON_MAX_DATA_SIZE    (OMNI_ICON_SLOT_SIZE - OMNI_ICON_HEADER_SIZE)

#define OMNI_ICON_FLASH_SECTOR_SIZE 4096u
#define OMNI_ICON_FLASH_PAGE_SIZE   256u
#define OMNI_ICON_HID_PAYLOAD_MAX   24u

#define OMNI_ICON_ERASE_START_DELAY_MS 50u
#define OMNI_ICON_BEGIN_START_DELAY_MS 50u

#define OMNI_ICON_FORMAT_QGF       2u
#define OMNI_ICON_MAGIC            0x43494D4Fu
#define OMNI_ICON_VERSION          1u

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t slot;
    uint16_t format;
    uint16_t reserved0;
    uint32_t data_size;
    uint32_t crc32;
    uint8_t  reserved[236];
} omni_icon_header_t;

_Static_assert(sizeof(omni_icon_header_t) == OMNI_ICON_HEADER_SIZE,
               "omni_icon_header_t must be 256 bytes");

typedef enum {
    OMNI_ICON_STATE_IDLE    = 0,
    OMNI_ICON_STATE_ERASING = 1,
    OMNI_ICON_STATE_READY   = 2,
    OMNI_ICON_STATE_WRITING = 3,
    OMNI_ICON_STATE_DONE    = 4,
    OMNI_ICON_STATE_ERROR   = 5,
} omni_icon_state_t;

typedef enum {
    OMNI_ICON_ERROR_NONE          = 0,
    OMNI_ICON_ERROR_BUSY          = 1,
    OMNI_ICON_ERROR_BAD_SIZE      = 2,
    OMNI_ICON_ERROR_BAD_SLOT      = 3,
    OMNI_ICON_ERROR_BAD_OFFSET    = 4,
    OMNI_ICON_ERROR_BAD_LENGTH    = 5,
    OMNI_ICON_ERROR_CRC           = 6,
    OMNI_ICON_ERROR_NOT_READY     = 7,
    OMNI_ICON_ERROR_INCOMPLETE    = 8,
} omni_icon_error_t;

static omni_icon_state_t icon_state = OMNI_ICON_STATE_IDLE;
static uint8_t icon_error = OMNI_ICON_ERROR_NONE;
static uint8_t current_slot = 0;
static uint8_t pending_slot = 0;
static uint32_t expected_size = 0;
static uint32_t expected_crc = 0;
static uint32_t received_size = 0;
static uint32_t programmed_size = 0;
static uint32_t erase_offset = 0;
static uint8_t page_buf[OMNI_ICON_FLASH_PAGE_SIZE];
static uint16_t page_fill = 0;
static uint32_t crc_work = 0xFFFFFFFFu;
static bool begin_pending = false;
static uint16_t begin_pending_timer = 0;
static uint32_t pending_total_size = 0;
static uint32_t pending_crc32 = 0;
static uint16_t erase_start_timer = 0;
static painter_image_handle_t icon_qgf_images[OMNI_ICON_SLOT_COUNT] = {0};
static bool icon_qgf_load_failed[OMNI_ICON_SLOT_COUNT] = {0};

static const uint8_t *flash_ptr(uint32_t flash_offset) {
    return (const uint8_t *)(XIP_BASE + flash_offset);
}

static uint32_t slot_base(uint8_t slot) {
    return OMNI_ICON_FLASH_OFFSET + ((uint32_t)slot * OMNI_ICON_SLOT_SIZE);
}

static uint32_t slot_data_offset(uint8_t slot) {
    return slot_base(slot) + OMNI_ICON_HEADER_SIZE;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len) {
    while (len--) {
        crc ^= *data++;

        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

static void set_error(uint8_t error) {
    icon_error = error;
    icon_state = OMNI_ICON_STATE_ERROR;
}

#if defined(RP2040)
static void flash_erase_4k(uint32_t offset) {
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(offset, OMNI_ICON_FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
}

static void flash_program_256(uint32_t offset, const uint8_t *data) {
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(offset, data, OMNI_ICON_FLASH_PAGE_SIZE);
    restore_interrupts(ints);
}
#else
static void flash_erase_4k(uint32_t offset) {
    (void)offset;
}

static void flash_program_256(uint32_t offset, const uint8_t *data) {
    (void)offset;
    (void)data;
}
#endif

static bool is_valid_slot(uint8_t slot) {
    return slot < OMNI_ICON_SLOT_COUNT;
}

static bool flush_page(void) {
    if (page_fill == 0) {
        return true;
    }

    if (page_fill < OMNI_ICON_FLASH_PAGE_SIZE) {
        memset(&page_buf[page_fill], 0xFF, OMNI_ICON_FLASH_PAGE_SIZE - page_fill);
    }

    flash_program_256(slot_data_offset(current_slot) + programmed_size, page_buf);

    programmed_size += OMNI_ICON_FLASH_PAGE_SIZE;
    page_fill = 0;
    memset(page_buf, 0xFF, sizeof(page_buf));

    return true;
}

static bool write_header_valid(void) {
    uint8_t page[OMNI_ICON_FLASH_PAGE_SIZE];
    memset(page, 0xFF, sizeof(page));

    omni_icon_header_t header = {
        .magic     = OMNI_ICON_MAGIC,
        .version   = OMNI_ICON_VERSION,
        .slot      = current_slot,
        .format    = OMNI_ICON_FORMAT_QGF,
        .reserved0 = 0,
        .data_size = expected_size,
        .crc32     = expected_crc,
    };

    memcpy(page, &header, sizeof(header));
    flash_program_256(slot_base(current_slot), page);

    return true;
}

void omni_icon_set_display(painter_device_t display) {
    (void)display;
}

bool omni_icon_is_valid(uint8_t slot) {
    if (!is_valid_slot(slot)) {
        return false;
    }

    const omni_icon_header_t *header =
        (const omni_icon_header_t *)flash_ptr(slot_base(slot));

    if (header->magic != OMNI_ICON_MAGIC) {
        return false;
    }

    if (header->version != OMNI_ICON_VERSION) {
        return false;
    }

    if (header->slot != slot) {
        return false;
    }

    if (header->format != OMNI_ICON_FORMAT_QGF) {
        return false;
    }

    if (header->data_size == 0 || header->data_size > OMNI_ICON_MAX_DATA_SIZE) {
        return false;
    }

    return true;
}

void omni_icon_close_image(uint8_t slot) {
    if (!is_valid_slot(slot)) {
        return;
    }

    if (icon_qgf_images[slot] != NULL) {
        qp_close_image(icon_qgf_images[slot]);
        icon_qgf_images[slot] = NULL;
    }

    icon_qgf_load_failed[slot] = false;
}

__attribute__((weak)) void omni_icon_on_changed(uint8_t slot) {
    (void)slot;
}

painter_image_handle_t omni_icon_get_image(uint8_t slot) {
    if (!is_valid_slot(slot)) {
        return NULL;
    }

    if (icon_qgf_images[slot] != NULL) {
        return icon_qgf_images[slot];
    }

    if (icon_qgf_load_failed[slot]) {
        return NULL;
    }

    if (!omni_icon_is_valid(slot)) {
        icon_qgf_load_failed[slot] = true;
        return NULL;
    }

    const void *qgf = flash_ptr(slot_data_offset(slot));
    icon_qgf_images[slot] = qp_load_image_mem(qgf);

    if (icon_qgf_images[slot] == NULL) {
        icon_qgf_load_failed[slot] = true;
        return NULL;
    }

    return icon_qgf_images[slot];
}

painter_image_handle_t* omni_icon_get_image_ptr(uint8_t slot) {
    if (!is_valid_slot(slot)) {
        return NULL;
    }

    (void)omni_icon_get_image(slot);

    return &icon_qgf_images[slot];
}


bool omni_icon_schedule_begin_upload(uint8_t slot, uint32_t total_size, uint32_t crc32) {
    if (!is_valid_slot(slot)) {
        set_error(OMNI_ICON_ERROR_BAD_SLOT);
        return false;
    }

    if (icon_state == OMNI_ICON_STATE_ERASING || icon_state == OMNI_ICON_STATE_WRITING) {
        set_error(OMNI_ICON_ERROR_BUSY);
        return false;
    }

    if (total_size == 0 || total_size > OMNI_ICON_MAX_DATA_SIZE) {
        set_error(OMNI_ICON_ERROR_BAD_SIZE);
        return false;
    }

    pending_slot = slot;
    pending_total_size = total_size;
    pending_crc32 = crc32;

    begin_pending_timer = timer_read();
    begin_pending = true;

    icon_error = OMNI_ICON_ERROR_NONE;
    icon_state = OMNI_ICON_STATE_IDLE;

    return true;
}

bool omni_icon_begin_upload(uint8_t slot, uint32_t total_size, uint32_t crc32) {
    if (!is_valid_slot(slot)) {
        set_error(OMNI_ICON_ERROR_BAD_SLOT);
        return false;
    }

    if (icon_state == OMNI_ICON_STATE_ERASING || icon_state == OMNI_ICON_STATE_WRITING) {
        set_error(OMNI_ICON_ERROR_BUSY);
        return false;
    }

    if (total_size == 0 || total_size > OMNI_ICON_MAX_DATA_SIZE) {
        set_error(OMNI_ICON_ERROR_BAD_SIZE);
        return false;
    }

    omni_icon_close_image(slot);

    current_slot = slot;
    expected_size = total_size;
    expected_crc = crc32;

    received_size = 0;
    programmed_size = 0;
    erase_offset = 0;
    page_fill = 0;
    crc_work = 0xFFFFFFFFu;

    memset(page_buf, 0xFF, sizeof(page_buf));

    icon_error = OMNI_ICON_ERROR_NONE;
    erase_start_timer = timer_read();
    icon_state = OMNI_ICON_STATE_ERASING;

    return true;
}

bool omni_icon_write_chunk(uint8_t slot, uint32_t offset, const uint8_t *data, uint8_t len) {
    if (!is_valid_slot(slot) || slot != current_slot) {
        set_error(OMNI_ICON_ERROR_BAD_SLOT);
        return false;
    }

    if (icon_state != OMNI_ICON_STATE_READY && icon_state != OMNI_ICON_STATE_WRITING) {
        set_error(OMNI_ICON_ERROR_NOT_READY);
        return false;
    }

    if (len == 0 || len > OMNI_ICON_HID_PAYLOAD_MAX) {
        set_error(OMNI_ICON_ERROR_BAD_LENGTH);
        return false;
    }

    if (offset != received_size) {
        set_error(OMNI_ICON_ERROR_BAD_OFFSET);
        return false;
    }

    if (received_size + len > expected_size) {
        set_error(OMNI_ICON_ERROR_BAD_LENGTH);
        return false;
    }

    icon_state = OMNI_ICON_STATE_WRITING;

    crc_work = crc32_update(crc_work, data, len);

    for (uint8_t i = 0; i < len; i++) {
        page_buf[page_fill++] = data[i];

        if (page_fill == OMNI_ICON_FLASH_PAGE_SIZE) {
            flush_page();
        }
    }

    received_size += len;

    return true;
}

bool omni_icon_finish_upload(uint8_t slot) {
    if (!is_valid_slot(slot) || slot != current_slot) {
        set_error(OMNI_ICON_ERROR_BAD_SLOT);
        return false;
    }

    if (icon_state != OMNI_ICON_STATE_WRITING && icon_state != OMNI_ICON_STATE_READY) {
        set_error(OMNI_ICON_ERROR_NOT_READY);
        return false;
    }

    if (received_size != expected_size) {
        set_error(OMNI_ICON_ERROR_INCOMPLETE);
        return false;
    }

    flush_page();

    uint32_t actual_crc = crc_work ^ 0xFFFFFFFFu;

    if (actual_crc != expected_crc) {
        set_error(OMNI_ICON_ERROR_CRC);
        return false;
    }

    write_header_valid();

    omni_icon_close_image(slot);

    icon_state = OMNI_ICON_STATE_DONE;
    icon_error = OMNI_ICON_ERROR_NONE;

    return true;
}

void omni_icon_task(void) {
    if (begin_pending) {
        if (timer_elapsed(begin_pending_timer) < OMNI_ICON_BEGIN_START_DELAY_MS) {
            return;
        }

        begin_pending = false;
        omni_icon_begin_upload(pending_slot, pending_total_size, pending_crc32);
        return;
    }

    if (icon_state == OMNI_ICON_STATE_ERASING) {
        if (timer_elapsed(erase_start_timer) < OMNI_ICON_ERASE_START_DELAY_MS) {
            return;
        }

        flash_erase_4k(slot_base(current_slot) + erase_offset);

        erase_offset += OMNI_ICON_FLASH_SECTOR_SIZE;

        if (erase_offset >= OMNI_ICON_SLOT_SIZE) {
            icon_state = OMNI_ICON_STATE_READY;
        }

        return;
    }
}

uint8_t omni_icon_get_state(void) {
    return icon_state;
}

uint8_t omni_icon_get_error(void) {
    return icon_error;
}

uint8_t omni_icon_get_progress_percent(void) {
    if (icon_state == OMNI_ICON_STATE_ERASING) {
        return (uint8_t)((erase_offset * 100u) / OMNI_ICON_SLOT_SIZE);
    }

    if (expected_size == 0) {
        return 0;
    }

    return (uint8_t)((received_size * 100u) / expected_size);
}