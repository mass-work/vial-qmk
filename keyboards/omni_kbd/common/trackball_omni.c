// Copyright 2025 mass
// SPDX-License-Identifier: GPL-2.0-or-later

#include <math.h>
#include <stdint.h>
#include <print.h>
#include "haptic.h"
#include "dynamic_keymap.h"

#include "trackball_omni.h"
#include "timer.h"
#include "config_omni.h"
#include "status_view.h"

#define constrain_hid(amt) ((amt) < -127 ? -127 : ((amt) > 127 ? 127 : (amt)))
#define constrain_hid16(amt) ((amt) < -32767 ? -32767 : ((amt) > 32767 ? 32767 : (amt)))

static float accumulated_x = 0.0f;
static float accumulated_y = 0.0f;
static float accumulated_h = 0.0f;
static float accumulated_v = 0.0f;

static inline uint8_t clamp_1_100_x(int16_t x) {
    if (x < 1) x = 1;
    if (x > 100) x = 100;
    return (uint8_t)x;
}

void process_cursor_report(report_mouse_t *mouse_report, pmw33xx_report_t report, float speed_adjust, uint8_t slope_factor, int rx, int ry, uint8_t cpi_scale, uint8_t orientation) {
    if (!report.motion.b.is_lifted) {    
        


        float raw_x = (float)report.delta_x / cpi_scale;
        float raw_y = (float)report.delta_y / cpi_scale;

        float x, y;

        switch (orientation) {
            case 0:
            default:
                x = raw_x;
                y = raw_y;
                break;
            case 1: // 90 deg
                x =  raw_y;
                y = -raw_x;
                break;
            case 2: // 180 deg
                x = -raw_x;
                y = -raw_y;
                break;
            case 3: // 270 deg
                x = -raw_y;
                y =  raw_x;
                break;
        }

        int sign_x = ((x > 0) - (x < 0)) * rx;
        int sign_y = ((y > 0) - (y < 0)) * ry;
        float x_corr = pow(fabs(x), speed_adjust) / pow(127, speed_adjust) * 127 / 100 * slope_factor * sign_x;
        float y_corr = pow(fabs(y), speed_adjust) / pow(127, speed_adjust) * 127 / 100 * slope_factor * sign_y;
        accumulated_x += x_corr;
        accumulated_y += y_corr;

        if (fabs(accumulated_x) >= 1.0f) {
            mouse_report->x = constrain_hid(mouse_report->x + accumulated_x);
            accumulated_x = 0;
        }
        if (fabs(accumulated_y) >= 1.0f) {
            mouse_report->y = constrain_hid(mouse_report->y + accumulated_y);
            accumulated_y = 0;
        }
    }
}

void process_high_res_scroll_report(report_mouse_t *mouse_report, pmw33xx_report_t report, float speed_adjust, uint8_t slope_factor, int rx, int ry, uint8_t cpi_scale, uint8_t orientation, bool is_haptic, const keypos_t keypos[]) {
    (void)is_haptic;
    (void)keypos;
    if (!report.motion.b.is_lifted) {
        uint16_t corr_calc_rapport_max = 600;

        float raw_x = (float)report.delta_x * cpi_scale;
        float raw_y = (float)report.delta_y * cpi_scale;
        float x, y;

        switch (orientation) {
            case 0:
            default:
                x = raw_x;
                y = raw_y;
                break;
            case 1: // 90 deg
                x =  raw_y;
                y = -raw_x;
                break;
            case 2: // 180 deg
                x = -raw_x;
                y = -raw_y;
                break;
            case 3: // 270 deg
                x = -raw_y;
                y =  raw_x;
                break;
        }

        int sign_x = ((x > 0) - (x < 0)) * rx * lr_sc_mode_flag;
        int sign_y = ((y > 0) - (y < 0)) * ry * ud_sc_mode_flag;
        float x_corr = pow(fabs(x), speed_adjust) / pow(corr_calc_rapport_max, speed_adjust) * corr_calc_rapport_max / 100 * slope_factor * sign_x;
        float y_corr = pow(fabs(y), speed_adjust) / pow(corr_calc_rapport_max, speed_adjust) * corr_calc_rapport_max / 100 * slope_factor * sign_y;

        const float diagonal_limit = 0.5f;
        float ratio = fabs(y_corr) / fabs(x_corr);  
        if (ratio > diagonal_limit && ratio < (1.0f / diagonal_limit)) {
            return;
        } else if (ratio <= diagonal_limit) {
            accumulated_h += x_corr;
        } else {
            accumulated_v += y_corr;
        }

        if (fabs(accumulated_v) >= 1.0f * (clamp_1_100_x(hi_res_interval_v) * 12 / 10)) {
            mouse_report->v = constrain_hid16(mouse_report->v + accumulated_v) / (clamp_1_100_x(hi_res_value_v) * 12 / 10);
            accumulated_v = 0;
        }

        if (fabs(accumulated_h) >= 1.0f * (clamp_1_100_x(hi_res_interval_h) * 12 / 10)) {
            mouse_report->h = -constrain_hid16(mouse_report->h + accumulated_h) / (clamp_1_100_x(hi_res_value_h) * 12 / 10);
            accumulated_h = 0;
        }
    }
}

typedef enum {
    TB_TAP_LEFT = 0,
    TB_TAP_RIGHT,
    TB_TAP_UP,
    TB_TAP_DOWN,
    TB_TAP_SLOT_COUNT
} tb_tap_slot_t;

static const keypos_t tb_tap_keypos_main[TB_TAP_SLOT_COUNT] = {
    [TB_TAP_LEFT]  = { .row = 4, .col = 2 },
    [TB_TAP_RIGHT] = { .row = 4, .col = 3 },
    [TB_TAP_UP]    = { .row = 4, .col = 0 },
    [TB_TAP_DOWN]  = { .row = 4, .col = 1 },
};

static const keypos_t tb_tap_keypos_sub[TB_TAP_SLOT_COUNT] = {
    [TB_TAP_LEFT]  = { .row = 5, .col = 2 },
    [TB_TAP_RIGHT] = { .row = 5, .col = 3 },
    [TB_TAP_UP]    = { .row = 5, .col = 0 },
    [TB_TAP_DOWN]  = { .row = 5, .col = 1 },
};

static bool tb_main_gesture_enabled = false;
static bool tb_sub_gesture_enabled  = false;
static uint8_t tb_gesture_cache_layer = 255;

static uint16_t tb_tap_get_keycode_on_layer(
    const keypos_t keypos[],
    tb_tap_slot_t slot,
    uint8_t layer
) {
    if (slot >= TB_TAP_SLOT_COUNT) {
        return KC_NO;
    }

    keypos_t pos = keypos[slot];
    return dynamic_keymap_get_keycode(layer, pos.row, pos.col);
}

static bool tb_tap_has_any_key_on_layer(
    const keypos_t keypos[],
    uint8_t layer
) {
    for (uint8_t i = 0; i < TB_TAP_SLOT_COUNT; i++) {
        if (tb_tap_get_keycode_on_layer(keypos, i, layer) != KC_NO) {
            return true;
        }
    }

    return false;
}

static void tb_gesture_config_update_for_layer(uint8_t layer) {
    tb_main_gesture_enabled = tb_tap_has_any_key_on_layer(tb_tap_keypos_main, layer);
    tb_sub_gesture_enabled  = tb_tap_has_any_key_on_layer(tb_tap_keypos_sub, layer);
    tb_gesture_cache_layer  = layer;
}

void tb_gesture_config_force_update(void) {
    uint8_t layer = get_highest_layer(layer_state | default_layer_state);
    tb_gesture_config_update_for_layer(layer);
}

void tb_gesture_config_update_if_needed(void) {
    uint8_t layer = get_highest_layer(layer_state | default_layer_state);

    if (layer == tb_gesture_cache_layer) {
        return;
    }

    tb_gesture_config_update_for_layer(layer);
}

bool tb_main_gesture_is_enabled(void) {
    return tb_main_gesture_enabled;
}

bool tb_sub_gesture_is_enabled(void) {
    return tb_sub_gesture_enabled;
}

static uint16_t tb_tap_get_keycode(const keypos_t keypos[TB_TAP_SLOT_COUNT], tb_tap_slot_t slot) {
    if (slot >= TB_TAP_SLOT_COUNT) {
        return KC_NO;
    }

    uint8_t layer = get_highest_layer(layer_state | default_layer_state);
    keypos_t pos = keypos[slot];

    return dynamic_keymap_get_keycode(layer, pos.row, pos.col);
}

bool tb_main_tap_has_any_key(void) {
    uint8_t layer = get_highest_layer(layer_state | default_layer_state);
    return tb_tap_has_any_key_on_layer(tb_tap_keypos_main, layer);
}

bool tb_sub_tap_has_any_key(void) {
    uint8_t layer = get_highest_layer(layer_state | default_layer_state);
    return tb_tap_has_any_key_on_layer(tb_tap_keypos_sub, layer);
}

const keypos_t *tb_main_tap_keypos(void) {
    return tb_tap_keypos_main;
}

const keypos_t *tb_sub_tap_keypos(void) {
    return tb_tap_keypos_sub;
}

#define TB_TAP_RELEASE_DELAY_MS 1
#define TB_TAP_INTERVAL_MS 2
#define TB_TAP_QUEUE_MAX 20
#define TB_TAP_REQUEST_MAX 10

typedef enum {
    TB_TAP_IDLE = 0,
    TB_TAP_PENDING_PRESS,
    TB_TAP_HOLDING,
} tb_tap_state_t;

static tb_tap_state_t tb_tap_state = TB_TAP_IDLE;

static const keypos_t *tb_tap_pending_keypos = NULL;
static tb_tap_slot_t tb_tap_pending_slot = TB_TAP_LEFT;
static uint8_t tb_tap_pending_count = 0;

static uint16_t tb_tap_holding_keycode = KC_NO;
static uint32_t tb_tap_timer = 0;
static uint32_t tb_tap_last_time = 0;

static void tb_request_tap_count(const keypos_t keypos[TB_TAP_SLOT_COUNT], tb_tap_slot_t slot, uint8_t count) {
    if (count == 0) {
        return;
    }

    if (tb_tap_pending_count > 0 && tb_tap_pending_slot != slot) {
        tb_tap_pending_count = 0;
    }

    tb_tap_pending_keypos = keypos;
    tb_tap_pending_slot = slot;

    uint16_t next_count = tb_tap_pending_count + count;
    if (next_count > TB_TAP_QUEUE_MAX) {
        next_count = TB_TAP_QUEUE_MAX;
    }

    tb_tap_pending_count = (uint8_t)next_count;
}

void tb_tap_pending_task(void) {
    switch (tb_tap_state) {
        case TB_TAP_IDLE:
            if (tb_tap_pending_count == 0 || tb_tap_pending_keypos == NULL) {
                return;
            }

            if (timer_elapsed32(tb_tap_last_time) < TB_TAP_INTERVAL_MS) {
                return;
            }

            uint16_t keycode = tb_tap_get_keycode(tb_tap_pending_keypos, tb_tap_pending_slot);

            if (keycode == KC_NO) {
                tb_tap_pending_count = 0;
                return;
            }

            if (keycode >= MACRO_KEY_START && keycode <= MACRO_KEY_END) {
                tb_tap_pending_count = 0;
                return;
            }

            tb_tap_holding_keycode = keycode;
            register_code16(tb_tap_holding_keycode);

            tb_tap_timer = timer_read32();
            tb_tap_last_time = tb_tap_timer;
            tb_tap_state = TB_TAP_HOLDING;

            tb_tap_pending_count--;
            return;

        case TB_TAP_HOLDING:
            if (timer_elapsed32(tb_tap_timer) >= TB_TAP_RELEASE_DELAY_MS) {
                unregister_code16(tb_tap_holding_keycode);

                tb_tap_holding_keycode = KC_NO;
                tb_tap_state = TB_TAP_IDLE;
            }
            return;

        default:
            tb_tap_state = TB_TAP_IDLE;
            return;
    }
}

void process_tap_report(report_mouse_t *mouse_report, pmw33xx_report_t report, float speed_adjust, uint8_t slope_factor, int rx, int ry, uint8_t cpi_scale, uint8_t orientation, bool is_haptic, const keypos_t keypos[]) {
    if (!report.motion.b.is_lifted) {

        float raw_x = (float)report.delta_x / cpi_scale;
        float raw_y = (float)report.delta_y / cpi_scale;

        const float sens = 0.2f;
        raw_x *= sens;
        raw_y *= sens;

        float x, y;

        switch (orientation) {
            case 0:
            default:
                x = raw_x;
                y = raw_y;
                break;
            case 1: // 90 deg
                x =  raw_y;
                y = -raw_x;
                break;
            case 2: // 180 deg
                x = -raw_x;
                y = -raw_y;
                break;
            case 3: // 270 deg
                x = -raw_y;
                y =  raw_x;
                break;
        }

        int sign_x = ((x > 0) - (x < 0)) * rx * lr_sc_mode_flag;
        int sign_y = ((y > 0) - (y < 0)) * ry * ud_sc_mode_flag;

        float x_corr = powf(fabsf(x), speed_adjust) / powf(127.0f, speed_adjust) * 127.0f / 100.0f * slope_factor * sign_x;
        float y_corr = powf(fabsf(y), speed_adjust) / powf(127.0f, speed_adjust) * 127.0f / 100.0f * slope_factor * sign_y;

        const float diagonal_limit = 0.3f;
        float       ratio          = fabsf(y_corr) / fabsf(x_corr);

        if (ratio > diagonal_limit && ratio < (1.0f / diagonal_limit)) {
            return;
        } else if (ratio <= diagonal_limit) {
            accumulated_h += x_corr / 2.0f;
        } else {
            accumulated_v += y_corr / 1.0f;
        }


        #ifndef POINTING_DEVICE_HIRES_SCROLL_ENABLE
            bool did_scroll    = false;
        #endif

        if (fabsf(accumulated_h) >= 1.0f) {
            int tap_cycle_h = (int)roundf(fabsf(accumulated_h));
            tap_cycle_h = (tap_cycle_h > TB_TAP_REQUEST_MAX) ? TB_TAP_REQUEST_MAX : tap_cycle_h;

            uint8_t request_count = tap_cycle_h;
            uprintf("[TP Cycle] raw_dy=%d \n", request_count);

            if (accumulated_h > 0) {
                tb_request_tap_count(keypos, TB_TAP_LEFT, request_count);
            } else if (accumulated_h < 0) {
                tb_request_tap_count(keypos, TB_TAP_RIGHT, request_count);
            }

        #ifndef POINTING_DEVICE_HIRES_SCROLL_ENABLE
            did_scroll = true;
        #endif

            accumulated_h = 0.0f;
            accumulated_v = 0.0f;
        }

        if (fabsf(accumulated_v) >= 1.0f) {
            int tap_cycle_v = (int)roundf(fabsf(accumulated_v));
            tap_cycle_v = (tap_cycle_v > TB_TAP_REQUEST_MAX) ? TB_TAP_REQUEST_MAX : tap_cycle_v;

            uint8_t request_count = tap_cycle_v;
            uprintf("[TP Cycle] raw_dy=%d \n", request_count);

            if (accumulated_v > 0) {
                tb_request_tap_count(keypos, TB_TAP_UP, request_count);
            } else if (accumulated_v < 0) {
                tb_request_tap_count(keypos, TB_TAP_DOWN, request_count);
            }

        #ifndef POINTING_DEVICE_HIRES_SCROLL_ENABLE
            did_scroll = true;
        #endif

            accumulated_h = 0.0f;
            accumulated_v = 0.0f;
        }


#ifdef HAPTIC_ENABLE
    #ifndef POINTING_DEVICE_HIRES_SCROLL_ENABLE
    if (did_scroll && is_haptic) {
        haptic_play();
    }
    #endif

#endif
    }
}