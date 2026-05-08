#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "qp.h"

typedef enum {
    OMNI_BG_STATE_IDLE    = 0,
    OMNI_BG_STATE_ERASING = 1,
    OMNI_BG_STATE_READY   = 2,
    OMNI_BG_STATE_WRITING = 3,
    OMNI_BG_STATE_DONE    = 4,
    OMNI_BG_STATE_ERROR   = 5,
} omni_bg_state_t;

typedef enum {
    OMNI_BG_ERROR_NONE          = 0,
    OMNI_BG_ERROR_BUSY          = 1,
    OMNI_BG_ERROR_BAD_SIZE      = 2,
    OMNI_BG_ERROR_BAD_FORMAT    = 3,
    OMNI_BG_ERROR_BAD_OFFSET    = 4,
    OMNI_BG_ERROR_BAD_LENGTH    = 5,
    OMNI_BG_ERROR_CRC           = 6,
    OMNI_BG_ERROR_NOT_READY     = 7,
    OMNI_BG_ERROR_INCOMPLETE    = 8,
} omni_bg_error_t;

void omni_bg_set_display(painter_device_t display);

bool omni_bg_schedule_begin_upload(uint16_t width, uint16_t height, uint8_t format, uint32_t total_size, uint32_t expected_crc);
bool omni_bg_begin_upload(uint16_t width, uint16_t height, uint8_t format, uint32_t total_size, uint32_t expected_crc);
bool omni_bg_write_chunk(uint32_t offset, const uint8_t *data, uint8_t len);
bool omni_bg_finish_upload(void);
bool omni_bg_invalidate(void);

void omni_bg_task(void);
void omni_bg_request_draw(void);
void omni_bg_draw_now(void);

bool omni_bg_is_valid(void);

omni_bg_state_t omni_bg_get_state(void);
uint8_t omni_bg_get_error(void);
uint8_t omni_bg_get_progress_percent(void);



void omni_bg_close_image(void);
void omni_bg_request_draw(void);
void omni_bg_render_task(void);
void omni_bg_close_image(void);