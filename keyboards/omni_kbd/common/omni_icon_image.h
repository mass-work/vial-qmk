#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "qp.h"

void omni_icon_set_display(painter_device_t display);

bool omni_icon_is_valid(uint8_t slot);
painter_image_handle_t omni_icon_get_image(uint8_t slot);
painter_image_handle_t* omni_icon_get_image_ptr(uint8_t slot);
void omni_icon_close_image(uint8_t slot);
void omni_icon_on_changed(uint8_t slot);

bool omni_icon_schedule_begin_upload(uint8_t slot, uint32_t total_size, uint32_t expected_crc);
bool omni_icon_begin_upload(uint8_t slot, uint32_t total_size, uint32_t expected_crc);
bool omni_icon_write_chunk(uint8_t slot, uint32_t offset, const uint8_t *data, uint8_t len);
bool omni_icon_finish_upload(uint8_t slot);

void omni_icon_task(void);

uint8_t omni_icon_get_state(void);
uint8_t omni_icon_get_error(void);
uint8_t omni_icon_get_progress_percent(void);