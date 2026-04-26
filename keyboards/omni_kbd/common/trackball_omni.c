// Copyright 2025 mass
// SPDX-License-Identifier: GPL-2.0-or-later

#include <math.h>
#include <stdint.h>
#include <print.h>
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

void process_cursor_report(report_mouse_t *mouse_report, pmw33xx_report_t report, float speed_adjust, uint8_t slope_factor, int rx, int ry, uint8_t cpi_scale) {
    if (!report.motion.b.is_lifted) {
        float x = (report.delta_x / cpi_scale);
        float y = (report.delta_y / cpi_scale);
        int sign_x = ((x > 0) - (x < 0)) * rx;
        int sign_y = ((y > 0) - (y < 0)) * ry;
        float x_corr = pow(fabs(x), speed_adjust) / pow(127, speed_adjust) * 127 / 100 * slope_factor * sign_x;
        float y_corr = pow(fabs(y), speed_adjust) / pow(127, speed_adjust) * 127 / 100 * slope_factor * sign_y;
        accumulated_x += x_corr;
        accumulated_y += y_corr;
        if (fabs(accumulated_x) >= 1.0f) {
            mouse_report->x = -constrain_hid(mouse_report->x + accumulated_x);
            accumulated_x = 0;
        }
        if (fabs(accumulated_y) >= 1.0f) {
            mouse_report->y = constrain_hid(mouse_report->y + accumulated_y);
            accumulated_y = 0;
        }
    }
}

void process_high_res_scroll_report(report_mouse_t *mouse_report, pmw33xx_report_t report, float speed_adjust, uint8_t slope_factor, int rx, int ry, uint8_t cpi_scale, uint8_t orientation, bool is_haptic) {
    (void)is_haptic;
    if (!report.motion.b.is_lifted) {
        uint16_t corr_calc_rapport_max = 600;

        float raw_x = report.delta_x * cpi_scale;
        float raw_y = report.delta_y * cpi_scale;
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



#include "haptic.h"
#include "dynamic_keymap.h"

typedef enum {
    TB_TAP_LEFT = 0,
    TB_TAP_RIGHT,
    TB_TAP_UP,
    TB_TAP_DOWN,
    TB_TAP_SLOT_COUNT
} tb_tap_slot_t;

static const keypos_t tb_tap_keypos[TB_TAP_SLOT_COUNT] = {
    [TB_TAP_LEFT]  = { .row = 3, .col = 2 },
    [TB_TAP_RIGHT] = { .row = 3, .col = 3 },
    [TB_TAP_UP]    = { .row = 2, .col = 2 },
    [TB_TAP_DOWN]  = { .row = 2, .col = 3 },
};

static void tb_tap_virtual_key(tb_tap_slot_t slot) {
    uprintf("tb_tap_virtual_key: slot=%d\n", slot);
    if (slot >= TB_TAP_SLOT_COUNT) {
        return;
    }

    keypos_t pos = tb_tap_keypos[slot];
    uint8_t layer   = get_highest_layer(layer_state | default_layer_state);
    uint16_t keycode = dynamic_keymap_get_keycode(layer, pos.row, pos.col);

    if (keycode == KC_NO) {
        return;
    }

    tap_code16(keycode);
}


void process_tap_report(report_mouse_t *mouse_report, pmw33xx_report_t report, float speed_adjust, uint8_t slope_factor, int rx, int ry, uint8_t cpi_scale, uint8_t orientation, bool is_haptic) {

    if (!report.motion.b.is_lifted) {

        float raw_x = (float)report.delta_x / cpi_scale;
        float raw_y = (float)report.delta_y / cpi_scale;

        const float sens = 0.1f;
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
        // --------------------------------------------

        int sign_x = ((x > 0) - (x < 0)) * rx * lr_sc_mode_flag;
        int sign_y = ((y > 0) - (y < 0)) * ry * ud_sc_mode_flag;

        float x_corr = powf(fabsf(x), speed_adjust) / powf(127.0f, speed_adjust) * 127.0f / 100.0f * slope_factor * sign_x;
        float y_corr = powf(fabsf(y), speed_adjust) / powf(127.0f, speed_adjust) * 127.0f / 100.0f * slope_factor * sign_y;

        const float diagonal_limit = 0.2f;
        float       ratio          = fabsf(y_corr) / fabsf(x_corr);

        if (ratio > diagonal_limit && ratio < (1.0f / diagonal_limit)) {
            return;
        } else if (ratio <= diagonal_limit) {
            accumulated_h += x_corr / 2.0f;
        } else {
            accumulated_v += y_corr / 1.0f;
        }

        int  tap_cycle_max = 20;
        #ifndef POINTING_DEVICE_HIRES_SCROLL_ENABLE
            bool did_scroll    = false;
        #endif

        if (fabsf(accumulated_h) >= 1.0f) {
            int tap_cycle_h = (int)roundf(fabsf(accumulated_h));
            tap_cycle_h     = (tap_cycle_h > tap_cycle_max) ? tap_cycle_max : tap_cycle_h;

            for (int i = 0; i < tap_cycle_h; i += 2) {
                if (accumulated_h > 0) {
                    tb_tap_virtual_key(TB_TAP_LEFT);
                } else if (accumulated_h < 0) {
                    tb_tap_virtual_key(TB_TAP_RIGHT);
                }
                #ifndef POINTING_DEVICE_HIRES_SCROLL_ENABLE
                    did_scroll = true;
                #endif
            }
            accumulated_h = 0.0f;
        }

        if (fabsf(accumulated_v) >= 1.0f) {
            int tap_cycle_v = (int)roundf(fabsf(accumulated_v));
            tap_cycle_v     = (tap_cycle_v > tap_cycle_max) ? tap_cycle_max : tap_cycle_v;

            for (int i = 0; i < tap_cycle_v; i += 2) {
                if (accumulated_v > 0) {
                    tb_tap_virtual_key(TB_TAP_UP);
                } else if (accumulated_v < 0) {
                    tb_tap_virtual_key(TB_TAP_DOWN);
                }
                #ifndef POINTING_DEVICE_HIRES_SCROLL_ENABLE
                    did_scroll = true;
                #endif
            }
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
