#include "omni_bg_image.h"

#include <string.h>
#include "quantum.h"

#if defined(RP2040)
#    include "hardware/flash.h"
#    include "hardware/sync.h"
#    include "hardware/regs/addressmap.h"
#endif

#ifndef OMNI_BG_FLASH_OFFSET
#    define OMNI_BG_FLASH_OFFSET 0x1C0000u
#endif

#ifndef OMNI_BG_SLOT_SIZE
#    define OMNI_BG_SLOT_SIZE 0x20000u
#endif

#ifndef OMNI_BG_WIDTH
#    define OMNI_BG_WIDTH 240
#endif

#ifndef OMNI_BG_HEIGHT
#    define OMNI_BG_HEIGHT 240
#endif

#define OMNI_BG_HEADER_SIZE       256u
#define OMNI_BG_FORMAT_RGB565     1u
#define OMNI_BG_BYTES_PER_PIXEL   2u
#define OMNI_BG_IMAGE_SIZE        ((uint32_t)OMNI_BG_WIDTH * OMNI_BG_HEIGHT * OMNI_BG_BYTES_PER_PIXEL)
#define OMNI_BG_PIXEL_OFFSET      (OMNI_BG_FLASH_OFFSET + OMNI_BG_HEADER_SIZE)

#define OMNI_BG_FLASH_SECTOR_SIZE 4096u
#define OMNI_BG_FLASH_PAGE_SIZE   256u

// 以前TX/RXが通っていた安定版として24byteに固定
#define OMNI_BG_HID_PAYLOAD_MAX   24u
#define OMNI_BG_ERASE_START_DELAY_MS 250u
#define OMNI_BG_BEGIN_START_DELAY_MS 50u

// 'O' 'M' 'B' 'G'
#define OMNI_BG_MAGIC             0x47424D4Fu
#define OMNI_BG_VERSION           1u


typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t width;
    uint16_t height;
    uint16_t format;
    uint32_t data_size;
    uint32_t crc32;
    uint8_t  reserved[236]; // total 256byte
} omni_bg_header_t;

static painter_device_t bg_display = NULL;

static omni_bg_state_t bg_state = OMNI_BG_STATE_IDLE;
static uint8_t bg_error = OMNI_BG_ERROR_NONE;

static uint32_t expected_size = 0;
static uint32_t expected_crc = 0;

static uint32_t received_size = 0;
static uint32_t programmed_size = 0;
static uint32_t erase_offset = 0;

static uint8_t page_buf[OMNI_BG_FLASH_PAGE_SIZE];
static uint16_t page_fill = 0;

static uint32_t crc_work = 0xFFFFFFFFu;

static bool draw_requested = false;

static bool begin_pending = false;
static uint16_t begin_pending_timer = 0;

static uint16_t pending_width = 0;
static uint16_t pending_height = 0;
static uint8_t pending_format = 0;
static uint32_t pending_total_size = 0;
static uint32_t pending_crc32 = 0;

static uint16_t erase_start_timer = 0;

static const uint8_t *flash_ptr(uint32_t flash_offset) {
    return (const uint8_t *)(XIP_BASE + flash_offset);
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
    bg_error = error;
    bg_state = OMNI_BG_STATE_ERROR;
}

#if defined(RP2040)
static void flash_erase_4k(uint32_t offset) {
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(offset, OMNI_BG_FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
}

static void flash_program_256(uint32_t offset, const uint8_t *data) {
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(offset, data, OMNI_BG_FLASH_PAGE_SIZE);
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

static bool flush_page(void) {
    if (page_fill == 0) {
        return true;
    }

    if (page_fill < OMNI_BG_FLASH_PAGE_SIZE) {
        memset(&page_buf[page_fill], 0xFF, OMNI_BG_FLASH_PAGE_SIZE - page_fill);
    }

    flash_program_256(OMNI_BG_PIXEL_OFFSET + programmed_size, page_buf);

    programmed_size += OMNI_BG_FLASH_PAGE_SIZE;
    page_fill = 0;
    memset(page_buf, 0xFF, sizeof(page_buf));

    return true;
}

static bool write_header_valid(void) {
    uint8_t page[OMNI_BG_FLASH_PAGE_SIZE];
    memset(page, 0xFF, sizeof(page));

    omni_bg_header_t header = {
        .magic     = OMNI_BG_MAGIC,
        .version   = OMNI_BG_VERSION,
        .width     = OMNI_BG_WIDTH,
        .height    = OMNI_BG_HEIGHT,
        .format    = OMNI_BG_FORMAT_RGB565,
        .data_size = OMNI_BG_IMAGE_SIZE,
        .crc32     = expected_crc,
    };

    memcpy(page, &header, sizeof(header));

    flash_program_256(OMNI_BG_FLASH_OFFSET, page);
    return true;
}

void omni_bg_set_display(painter_device_t display) {
    bg_display = display;
}

bool omni_bg_is_valid(void) {
    const omni_bg_header_t *header = (const omni_bg_header_t *)flash_ptr(OMNI_BG_FLASH_OFFSET);

    if (header->magic != OMNI_BG_MAGIC) {
        return false;
    }

    if (header->version != OMNI_BG_VERSION) {
        return false;
    }

    if (header->width != OMNI_BG_WIDTH || header->height != OMNI_BG_HEIGHT) {
        return false;
    }

    if (header->format != OMNI_BG_FORMAT_RGB565) {
        return false;
    }

    if (header->data_size != OMNI_BG_IMAGE_SIZE) {
        return false;
    }

    return true;
}

bool omni_bg_schedule_begin_upload(uint16_t width, uint16_t height, uint8_t format, uint32_t total_size, uint32_t crc32) {
    if (bg_state == OMNI_BG_STATE_ERASING || bg_state == OMNI_BG_STATE_WRITING) {
        set_error(OMNI_BG_ERROR_BUSY);
        return false;
    }

    if (width != OMNI_BG_WIDTH || height != OMNI_BG_HEIGHT || total_size != OMNI_BG_IMAGE_SIZE) {
        set_error(OMNI_BG_ERROR_BAD_SIZE);
        return false;
    }

    if (format != OMNI_BG_FORMAT_RGB565) {
        set_error(OMNI_BG_ERROR_BAD_FORMAT);
        return false;
    }

    pending_width = width;
    pending_height = height;
    pending_format = format;
    pending_total_size = total_size;
    pending_crc32 = crc32;

    begin_pending_timer = timer_read();
    begin_pending = true;

    bg_error = OMNI_BG_ERROR_NONE;
    bg_state = OMNI_BG_STATE_IDLE;

    return true;
}

bool omni_bg_begin_upload(uint16_t width, uint16_t height, uint8_t format, uint32_t total_size, uint32_t crc32) {
    if (bg_state == OMNI_BG_STATE_ERASING || bg_state == OMNI_BG_STATE_WRITING) {
        set_error(OMNI_BG_ERROR_BUSY);
        return false;
    }

    if (width != OMNI_BG_WIDTH || height != OMNI_BG_HEIGHT || total_size != OMNI_BG_IMAGE_SIZE) {
        set_error(OMNI_BG_ERROR_BAD_SIZE);
        return false;
    }

    if (format != OMNI_BG_FORMAT_RGB565) {
        set_error(OMNI_BG_ERROR_BAD_FORMAT);
        return false;
    }

    expected_size = total_size;
    expected_crc = crc32;

    received_size = 0;
    programmed_size = 0;
    erase_offset = 0;
    page_fill = 0;
    crc_work = 0xFFFFFFFFu;

    memset(page_buf, 0xFF, sizeof(page_buf));

    bg_error = OMNI_BG_ERROR_NONE;
    erase_start_timer = timer_read();
    bg_state = OMNI_BG_STATE_ERASING;

    return true;
}

bool omni_bg_write_chunk(uint32_t offset, const uint8_t *data, uint8_t len) {
    if (bg_state != OMNI_BG_STATE_READY && bg_state != OMNI_BG_STATE_WRITING) {
        set_error(OMNI_BG_ERROR_NOT_READY);
        return false;
    }

    if (len == 0 || len > OMNI_BG_HID_PAYLOAD_MAX) {
        set_error(OMNI_BG_ERROR_BAD_LENGTH);
        return false;
    }

    if (offset != received_size) {
        set_error(OMNI_BG_ERROR_BAD_OFFSET);
        return false;
    }

    if (received_size + len > expected_size) {
        set_error(OMNI_BG_ERROR_BAD_LENGTH);
        return false;
    }

    bg_state = OMNI_BG_STATE_WRITING;

    crc_work = crc32_update(crc_work, data, len);

    for (uint8_t i = 0; i < len; i++) {
        page_buf[page_fill++] = data[i];

        if (page_fill == OMNI_BG_FLASH_PAGE_SIZE) {
            flush_page();
        }
    }

    received_size += len;

    return true;
}

bool omni_bg_finish_upload(void) {
    if (bg_state != OMNI_BG_STATE_WRITING && bg_state != OMNI_BG_STATE_READY) {
        set_error(OMNI_BG_ERROR_NOT_READY);
        return false;
    }

    if (received_size != expected_size) {
        set_error(OMNI_BG_ERROR_INCOMPLETE);
        return false;
    }

    flush_page();

    uint32_t actual_crc = crc_work ^ 0xFFFFFFFFu;

    if (actual_crc != expected_crc) {
        set_error(OMNI_BG_ERROR_CRC);
        return false;
    }

    write_header_valid();

    bg_state = OMNI_BG_STATE_DONE;
    bg_error = OMNI_BG_ERROR_NONE;

    omni_bg_request_draw();

    return true;
}

bool omni_bg_invalidate(void) {
    uint8_t page[OMNI_BG_FLASH_PAGE_SIZE];
    memset(page, 0xFF, sizeof(page));

    // magicを0に潰す。eraseなしで無効化。
    page[0] = 0x00;
    page[1] = 0x00;
    page[2] = 0x00;
    page[3] = 0x00;

    flash_program_256(OMNI_BG_FLASH_OFFSET, page);

    bg_state = OMNI_BG_STATE_IDLE;
    bg_error = OMNI_BG_ERROR_NONE;

    return true;
}

void omni_bg_request_draw(void) {
    draw_requested = true;
}

void omni_bg_draw_now(void) {
    if (bg_display == NULL) {
        return;
    }

    if (!omni_bg_is_valid()) {
        return;
    }

    const uint8_t *img = flash_ptr(OMNI_BG_PIXEL_OFFSET);

    const uint16_t lines_per_chunk = 16;

    for (uint16_t y = 0; y < OMNI_BG_HEIGHT; y += lines_per_chunk) {
        uint16_t lines = lines_per_chunk;

        if (y + lines > OMNI_BG_HEIGHT) {
            lines = OMNI_BG_HEIGHT - y;
        }

        const uint8_t *src = img + ((uint32_t)y * OMNI_BG_WIDTH * OMNI_BG_BYTES_PER_PIXEL);

        qp_viewport(bg_display, 0, y, OMNI_BG_WIDTH - 1, y + lines - 1);
        qp_pixdata(bg_display, src, (uint32_t)OMNI_BG_WIDTH * lines);
    }

    qp_flush(bg_display);
}

void omni_bg_task(void) {
    if (begin_pending) {
        if (timer_elapsed(begin_pending_timer) < OMNI_BG_BEGIN_START_DELAY_MS) {
            return;
        }

        begin_pending = false;
        omni_bg_begin_upload(
            pending_width,
            pending_height,
            pending_format,
            pending_total_size,
            pending_crc32
        );
        return;
    }

    if (bg_state == OMNI_BG_STATE_ERASING) {
        if (timer_elapsed(erase_start_timer) < OMNI_BG_ERASE_START_DELAY_MS) {
            return;
        }

        flash_erase_4k(OMNI_BG_FLASH_OFFSET + erase_offset);

        erase_offset += OMNI_BG_FLASH_SECTOR_SIZE;

        if (erase_offset >= OMNI_BG_SLOT_SIZE) {
            bg_state = OMNI_BG_STATE_READY;
        }

        return;
    }

    if (draw_requested) {
        draw_requested = false;
        omni_bg_draw_now();
    }
}

omni_bg_state_t omni_bg_get_state(void) {
    return bg_state;
}

uint8_t omni_bg_get_error(void) {
    return bg_error;
}

uint8_t omni_bg_get_progress_percent(void) {
    if (bg_state == OMNI_BG_STATE_ERASING) {
        return (uint8_t)((erase_offset * 100u) / OMNI_BG_SLOT_SIZE);
    }

    if (expected_size == 0) {
        return 0;
    }

    return (uint8_t)((received_size * 100u) / expected_size);
}