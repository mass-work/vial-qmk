
#pragma once
#include "quantum.h"
#include "config.h"
#include <stdint.h>
#include "../drivers/pmw33xx_common.h"

void process_cursor_report(report_mouse_t *mouse_report, pmw33xx_report_t report, float speed_adjust, uint8_t slope_factor, int rx, int ry, uint8_t cpi_scale);
void process_high_res_scroll_report(report_mouse_t *mouse_report, pmw33xx_report_t report, float speed_adjust, uint8_t slope_factor, int rx, int ry, uint8_t cpi_scale, uint8_t orientation, bool is_haptic, const keypos_t keypos[]);
void process_tap_report(report_mouse_t *mouse_report, pmw33xx_report_t report, float speed_adjust, uint8_t slope_factor, int rx, int ry, uint8_t cpi_scale, uint8_t orientation, bool is_haptic, const keypos_t keypos[]);
bool tb_main_tap_has_any_key(void);
bool tb_sub_tap_has_any_key(void);
const keypos_t *tb_main_tap_keypos(void);
const keypos_t *tb_sub_tap_keypos(void);

void tb_gesture_config_force_update(void);
void tb_gesture_config_update_if_needed(void);

bool tb_main_gesture_is_enabled(void);
bool tb_sub_gesture_is_enabled(void);


