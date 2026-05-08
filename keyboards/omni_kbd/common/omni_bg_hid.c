#include <stdint.h>
#include <stdbool.h>

#include "quantum.h"
#include "via.h"
#include "omni_bg_image.h"
#include "omni_icon_image.h"
    
#define OMNI_BG_CMD_BEGIN   0x40
#define OMNI_BG_CMD_STATUS  0x41
#define OMNI_BG_CMD_CHUNK   0x42
#define OMNI_BG_CMD_END     0x43
#define OMNI_BG_CMD_DRAW    0x44
#define OMNI_BG_CMD_CLEAR   0x45

// add
#define OMNI_ICON_CMD_BEGIN   0x50
#define OMNI_ICON_CMD_STATUS  0x51
#define OMNI_ICON_CMD_CHUNK   0x52
#define OMNI_ICON_CMD_END     0x53
#define OMNI_ICON_CMD_RELOAD  0x54

static uint16_t read_u16_le(const uint8_t *p) {
    return ((uint16_t)p[0]) | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32_le(const uint8_t *p) {
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void write_status_response(uint8_t *data, bool ok) {
    // data[0] はコマンドIDを残す
    data[1] = ok ? 0 : 1; // 0 = OK, 1 = NG
    data[2] = omni_bg_get_state();
    data[3] = omni_bg_get_error();
    data[4] = omni_bg_get_progress_percent();
}

static void write_icon_status_response(uint8_t *data, bool ok) {
    data[1] = ok ? 0 : 1;
    data[2] = omni_icon_get_state();
    data[3] = omni_icon_get_error();
    data[4] = omni_icon_get_progress_percent();
}


void raw_hid_receive_kb(uint8_t *data, uint8_t length) {
    if (length != 32) {
        if (length > 0) {
            data[0] = id_unhandled;
        }
        return;
    }

    switch (data[0]) {
        case OMNI_BG_CMD_BEGIN: {
            uint16_t width  = read_u16_le(&data[1]);
            uint16_t height = read_u16_le(&data[3]);
            uint8_t  format = data[5];
            uint32_t total  = read_u32_le(&data[6]);
            uint32_t crc    = read_u32_le(&data[10]);

            bool ok = omni_bg_schedule_begin_upload(width, height, format, total, crc);
            write_status_response(data, ok);
            return;
        }

        case OMNI_BG_CMD_STATUS: {
            write_status_response(data, true);
            return;
        }

        case OMNI_BG_CMD_CHUNK: {
            uint32_t offset = read_u32_le(&data[1]);
            uint8_t len = data[5];

            // payloadは data[6] から。今回は安定版として最大24byte。
            bool ok = omni_bg_write_chunk(offset, &data[6], len);
            write_status_response(data, ok);
            return;
        }

        case OMNI_BG_CMD_END: {
            bool ok = omni_bg_finish_upload();
            write_status_response(data, ok);
            return;
        }

        case OMNI_BG_CMD_DRAW: {
            omni_bg_draw_now();
            write_status_response(data, true);
            return;
        }

        case OMNI_BG_CMD_CLEAR: {
            bool ok = omni_bg_invalidate();
            write_status_response(data, ok);
            return;
        }

        //add
        case OMNI_ICON_CMD_BEGIN: {
            uint8_t  slot  = data[1];
            uint32_t total = read_u32_le(&data[2]);
            uint32_t crc   = read_u32_le(&data[6]);

            bool ok = omni_icon_schedule_begin_upload(slot, total, crc);
            write_icon_status_response(data, ok);
            return;
        }

        case OMNI_ICON_CMD_STATUS: {
            write_icon_status_response(data, true);
            return;
        }

        case OMNI_ICON_CMD_CHUNK: {
            uint8_t  slot   = data[1];
            uint32_t offset = read_u32_le(&data[2]);
            uint8_t  len    = data[6];

            bool ok = omni_icon_write_chunk(slot, offset, &data[7], len);
            write_icon_status_response(data, ok);
            return;
        }

        case OMNI_ICON_CMD_END: {
            uint8_t slot = data[1];

            bool ok = omni_icon_finish_upload(slot);
            write_icon_status_response(data, ok);
            return;
        }

        case OMNI_ICON_CMD_RELOAD: {
            uint8_t slot = data[1];

            uprintf("[ICON HID] RELOAD slot=%u\n", slot);

            omni_icon_close_image(slot);
            write_icon_status_response(data, true);
            data[5] = slot;

            return;
        }

        // case OMNI_ICON_CMD_RELOAD: {
        //     uint8_t slot = data[1];

        //     uprintf("[ICON HID] RELOAD slot=%u\n", slot);

        //     data[1] = 0;     // OK
        //     data[2] = slot;  // echo

        //     return;
        // }

        default:
            data[0] = id_unhandled;
            return;
    }
}