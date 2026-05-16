// Copyright 2025 mass
// SPDX-License-Identifier: GPL-2.0-or-later

#include <print.h>
#include "os_detection.h"
#include "status_view.h"
#include "draw_custom.h"
#include "config_omni.h"
#include "touch_lcd_omni.h"

uint8_t hi_res_interval_v = 100;
uint8_t hi_res_value_v    = 100;
uint8_t hi_res_interval_h = 100;
uint8_t hi_res_value_h    = 100;
int8_t ud_sc_mode_flag = 1;
int8_t lr_sc_mode_flag = 1;
uint16_t touch_repeat_interval = 70;
uint16_t touch_single_interval = 400;

static inline uint8_t clamp_u8(int16_t v, uint8_t lo, uint8_t hi) {
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (uint8_t)v;
}

static inline uint8_t clamp_bar_x(int16_t x) {
    if (x < BAR_L) x = BAR_L;
    if (x > BAR_R) x = BAR_R;
    return (uint8_t)x;
}

static inline uint8_t clamp_0_100_x(int16_t x) {
    if (x < 0) x = 0;
    if (x > 100) x = 100;
    return (uint8_t)x;
}

static inline os_variant_t current_os_variant(void) {
    return is_on_aos ? detected_host_os() : manual_os;
}

static inline uint8_t layer_for_os(os_variant_t os) {
    switch (os) {
        case OS_MACOS:
        case OS_IOS:
            return _SUB;

        case OS_WINDOWS:
        case OS_LINUX:
        case OS_UNSURE:
        default:
            return _BASE;
    }
}

static bool point_in_rect(int16_t x, int16_t y, uint16_t l, uint16_t t, uint16_t r, uint16_t b) {
    return (x >= l && x <= r && y >= t && y <= b);
}

static const char *const os_name_tbl[] = {
    [OS_UNSURE]  = "???",
    [OS_LINUX]   = "Lnx",
    [OS_WINDOWS] = "Win",
    [OS_MACOS]   = "Mac",
    [OS_IOS]     = "Mac", // 誤判定があるからiOSはMac扱い
};

const char *os_name_short(os_variant_t os) {
    if (!is_on_aos) {
        return (get_highest_layer(default_layer_state) == _SUB) ? "Sub" : "Bas";
    }
    if (os < 0 || os >= 5) return "???";
    return os_name_tbl[os];
}

typedef struct {
    const char *label;
    uint16_t    x, y;
    uint8_t     bar_mode;
    const char *bar_upper;
    const char *bar_lower;
    const char *summary;
} toggle_cfg_t;

static const toggle_cfg_t toggles[] = {
    { "HRV", COL2_X, ROW1_Y, 2, "RES", "VAL", "Vertical hi-res scroll" },
    { "HRH", COL3_X, ROW1_Y, 2, "RES", "VAL", "Horizontal hi-res scroll" },
    { "SCI", COL4_X, ROW1_Y, 0, "---", "---", "Invert scroll" },
    { "AML", COL2_X, ROW2_Y, 0, "---", "---", "Auto mouse layer" },
    { "TRP", COL3_X, ROW2_Y, 2, "HLD", "RPT", "Touch repeat interval" },
};

#define NUM_TOG (sizeof(toggles)/sizeof(toggles[0]))

static uint8_t os_tog_state[PROF_COUNT][NUM_TOG];
static uint8_t os_bar_upper[PROF_COUNT][NUM_TOG];
static uint8_t os_bar_lower[PROF_COUNT][NUM_TOG];
static int8_t  sel_idx = -1;

#define IDX_HRV 0
#define IDX_HRH 1
#define IDX_SCD 2
#define IDX_AML 3
#define IDX_TOC 4

static uint8_t g_default_layer = _BASE;
static inline bool is_sub_layer(void) { return g_default_layer == _SUB; }

static inline prof_index_t current_profile_index(void) {
    return is_sub_layer() ? PROF_SUB : PROF_BASE;
}

bool sv_hrv_enabled_current(void) {
    return os_tog_state[current_profile_index()][IDX_HRV] != 0;
}

bool sv_hrh_enabled_current(void) {
    return os_tog_state[current_profile_index()][IDX_HRH] != 0;
}

static inline void apply_to_param(uint8_t toggle_idx, bool is_upper_bar, uint8_t bar_x) {

    switch (toggle_idx) {
        case IDX_HRV:
            if (is_upper_bar) {
                if (sv_hrv_enabled_current()) {
                    hi_res_interval_v = clamp_0_100_x(bar_x);
                } else {
                    hi_res_interval_v = 100;
                }
            } else {
                if (sv_hrv_enabled_current()) {
                    hi_res_value_v = clamp_0_100_x(bar_x); //hi-res off
                } else {
                    if (g_default_layer == 0) {
                        hi_res_value_v = 1; // win
                    } else if (g_default_layer == 1) {
                        hi_res_value_v = 100; // mac
                    }
                }
            }
            break;

        case IDX_HRH:
            if (is_upper_bar) {
                if (sv_hrh_enabled_current()) {
                    hi_res_interval_h = clamp_0_100_x(bar_x);
                } else {
                    hi_res_interval_h = 100;
                }
            } else {
                if (sv_hrh_enabled_current()) {
                    hi_res_value_h    = clamp_0_100_x(bar_x);
                } else {
                    if (g_default_layer == 0) {
                        hi_res_value_h = 1; // win
                    } else if (g_default_layer == 1) {
                        hi_res_value_h = 100; // mac
                    }
                }
            }
            break;

        case IDX_SCD:
            if (os_tog_state[current_profile_index()][IDX_SCD]) {
                ud_sc_mode_flag = -1;
                lr_sc_mode_flag = -1;
            } else {
                ud_sc_mode_flag = 1;
                lr_sc_mode_flag = 1;
            }
            break;

        case IDX_AML:
            if (os_tog_state[current_profile_index()][IDX_AML]) {
                set_auto_mouse_enable(true);
            } else {
                set_auto_mouse_enable(false);
            }
            break;
        case IDX_TOC:
            if (is_upper_bar) {
                touch_single_interval = 150 + (bar_x * (1500 - 150) / 100);
            } else {
                touch_repeat_interval = 40 + (bar_x * (150 - 40) / 100);
            }

        default:
            break;
    }
}

static void draw_top_aos(painter_device_t dev, painter_font_handle_t font, uint8_t hue, uint8_t sat, uint8_t val) {
    const char *label = "OS";
    const bool  state = is_on_aos;
    omni_status_save_toggle_aos(state);

    const uint16_t x = COL0_X, y = ROW0_Y;
    const uint16_t l = x - BTN_W/2, t = y - BTN_H/2;
    const uint16_t r = x + BTN_W/2, b = y + BTN_H/2;

    const uint8_t main_v_on  = clamp_u8(val,     160, 255);
    const uint8_t main_v_off = clamp_u8(val - 80,  80, 255);
    const uint8_t bg_v_on    = clamp_u8(val -110,  50, 255);
    const uint8_t bg_v_off   = clamp_u8(val -140,  30, 255);

    const uint8_t txt_x = (uint8_t)(x - qp_textwidth(font, label)/2);
    const uint8_t txt_y = (uint8_t)(y - font->line_height/2 + 2);

    if (state) {
        qp_round_rect(dev, l, t, r, b, BTN_R, hue, sat, bg_v_on,  true, 0);
        qp_round_rect(dev, l, t, r, b, BTN_R, hue, sat, main_v_on,false, 1);
        qp_drawtext_recolor(dev, txt_x, txt_y, font, label, hue, sat, main_v_on, hue, sat, bg_v_on);
    } else {
        qp_round_rect(dev, l, t, r, b, BTN_R, hue, sat, bg_v_off, true, 0);
        qp_round_rect(dev, l, t, r, b, BTN_R, hue, sat, main_v_off,false, 1);
        qp_drawtext_recolor(dev, txt_x, txt_y, font, label, hue, sat, main_v_off, hue, sat, bg_v_off);
    }
}

static void draw_top_osc(painter_device_t dev, painter_font_handle_t font, uint8_t hue, uint8_t sat, uint8_t val) {
    os_variant_t cos = current_os_variant();
    const char  *label = os_name_short(cos);

    omni_status_save_toggle_osc(g_default_layer);

    const uint16_t x = COL1_X, y = ROW0_Y;
    const uint16_t l = x - BTN_W/2, t = y - BTN_H/2;
    const uint16_t r = x + BTN_W/2, b = y + BTN_H/2;

    const uint8_t main_v = clamp_u8(val, 160, 255);
    const uint8_t bg_v   = clamp_u8(val -110, 50, 255);

    const uint8_t txt_x = (uint8_t)(x - qp_textwidth(font, label)/2);
    const uint8_t txt_y = (uint8_t)(y - font->line_height/2 + 2);

    qp_round_rect(dev, l, t, r, b, BTN_R, hue, sat, bg_v,  true, 0);
    qp_round_rect(dev, l, t, r, b, BTN_R, hue, sat, main_v,false, 1);
    qp_drawtext_recolor(dev, txt_x, txt_y, font, label, hue, sat, main_v, hue, sat, bg_v);
}

static void draw_toggle_normal(uint8_t i, painter_device_t dev, painter_font_handle_t font, uint8_t hue, uint8_t sat, uint8_t val) {
    const toggle_cfg_t *cfg = &toggles[i];
    prof_index_t pi = current_profile_index();
    bool state = (os_tog_state[pi][i] != 0);
    
    omni_status_save_toggle_normal(pi, i, state);

    const uint16_t x = cfg->x, y = cfg->y;
    const uint16_t l = x - BTN_W/2, t = y - BTN_H/2;
    const uint16_t r = x + BTN_W/2, b = y + BTN_H/2;

    const uint8_t main_v_on  = clamp_u8(val,     160, 255);
    const uint8_t main_v_off = clamp_u8(val - 80,  80, 255);
    const uint8_t bg_v_on    = clamp_u8(val -110,  50, 255);
    const uint8_t bg_v_off   = clamp_u8(val -140,  30, 255);

    const uint8_t txt_x = (uint8_t)(x - qp_textwidth(font, cfg->label)/2);
    const uint8_t txt_y = (uint8_t)(y - font->line_height/2 + 2);

    if (state) {
        qp_round_rect(dev, l, t, r, b, BTN_R, hue, sat, bg_v_on,  true, 0);
        qp_round_rect(dev, l, t, r, b, BTN_R, hue, sat, main_v_on,false, 1);
        qp_drawtext_recolor(dev, txt_x, txt_y, font, cfg->label, hue, sat, main_v_on, hue, sat, bg_v_on);
    } else {
        qp_round_rect(dev, l, t, r, b, BTN_R, hue, sat, bg_v_off, true, 0);
        qp_round_rect(dev, l, t, r, b, BTN_R, hue, sat, main_v_off,false, 1);
        qp_drawtext_recolor(dev, txt_x, txt_y, font, cfg->label, hue, sat, main_v_off, hue, sat, bg_v_off);
    }
}

static void draw_bar(painter_device_t dev, uint16_t y, uint8_t hue, uint8_t sat, uint8_t val_cur, painter_font_handle_t font, uint8_t line_height, const char *label) {
    qp_round_rect(dev, BAR_L, y-2, BAR_R, y+2, 2, hue, sat, 90,  true, 0);
    qp_round_rect(dev, BAR_L, y-2, val_cur, y+2, 2, hue, sat, 180, true, 0);
    qp_drawtext_recolor(dev, BAR_L-45, y-line_height, font, label, hue, sat, 180, 0, 0, 0);
}

static void redraw_all(painter_device_t dev, painter_font_handle_t font) {
    qp_line(dev, 0, 155, 240, 155, hue_main_color, sat_main_color, 190);
    draw_top_aos(dev, font, hue_main_color, sat_main_color, 190);
    draw_top_osc(dev, font, hue_main_color, sat_main_color, 190);
    for (uint8_t i = 0; i < NUM_TOG; i++) {
        draw_toggle_normal(i, dev, font, hue_main_color, sat_main_color, 190);
    }
}

void load_params_from_prms(void){
    uint8_t row1 = (g_default_layer == 0) ? 11 : 14;
    uint8_t row2 = (g_default_layer == 0) ? 12 : 15;
    for (uint8_t i = 0; i < NUM_TOG; i++) {
        apply_to_param(i, true, dynamic_keymap_get_keycode(4, row1, i) - 0x7700);
        apply_to_param(i, false, dynamic_keymap_get_keycode(4, row2, i) - 0x7700);
    }
}

void persist_load_all(void) {
    is_on_aos = omni_status_load_toggle_aos();
    manual_os = omni_status_load_toggle_osc();

    if (!is_on_aos) {
        g_default_layer = manual_os;
        default_layer_set(1UL << g_default_layer);
    } else {
        os_variant_t cur = detected_host_os();
        g_default_layer = layer_for_os(cur);
        default_layer_set(1UL << g_default_layer);
    }
    for (uint8_t i = 0; i < NUM_TOG; i++) {
        os_tog_state[g_default_layer][i] = omni_status_load_toggle_normal(g_default_layer, i);
        os_bar_upper[g_default_layer][i] = omni_status_load_bar(g_default_layer, true, i);
        os_bar_lower[g_default_layer][i] = omni_status_load_bar(g_default_layer, false, i);
    }
    load_params_from_prms();
}

void ui_handle_touch(painter_device_t dev, painter_font_handle_t font, int16_t tx, int16_t ty) {
    const uint8_t line_height = (uint8_t)(font->line_height / 2);

    {
        const uint16_t x = COL0_X, y = ROW0_Y;
        const uint16_t l = x - BTN_W/2, t = y - BTN_H/2;
        const uint16_t r = x + BTN_W/2, b = y + BTN_H/2;
        if (point_in_rect(tx, ty, l, t, r, b)) {
            is_on_aos = !is_on_aos;

            if (is_on_aos) {
                os_variant_t cur = detected_host_os();
                default_layer_set(1UL << layer_for_os(cur)); 
            } 
            g_default_layer = get_highest_layer(default_layer_state);
            load_params_from_prms();

            sel_idx = -1;
            qp_rect(dev, 0, 130, 240, 150, 0, 0, 0, true);
            qp_rect(dev, 0, 160, 240, 240, 0, 0, 0, true);
            qp_drawtext_recolor(dev, (240/2) - qp_textwidth(font, "Automatic OS detection")/2, 130, font, "Automatic OS detection", hue_main_color, sat_main_color, 190, 0, 0, 0);
            omni_status_save_toggle_aos(is_on_aos);
            redraw_all(dev, font);
            return;
        }
    }

    {
        const uint16_t x = COL1_X, y = ROW0_Y;
        const uint16_t l = x - BTN_W/2, t = y - BTN_H/2;
        const uint16_t r = x + BTN_W/2, b = y + BTN_H/2;
        if (point_in_rect(tx, ty, l, t, r, b)) {
            if (!is_on_aos) {
                uint8_t curL = get_highest_layer(default_layer_state);
                uint8_t nxtL = (curL == _BASE) ? _SUB : _BASE;
                default_layer_set(1UL << nxtL);
                g_default_layer = nxtL;
                load_params_from_prms();

                sel_idx = -1;
                qp_rect(dev, 0, 130, 240, 150, 0, 0, 0, true);
                qp_rect(dev, 0, 160, 240, 240, 0, 0, 0, true);
                const char *txt = (nxtL == _BASE) ? "Keymap Change Base" : "Keymap Change Sub";
                qp_drawtext_recolor(dev, (240/2) - qp_textwidth(font, txt)/2, 130, font, txt, hue_main_color, sat_main_color, 190, 0, 0, 0);
                // persist_save_all();
                omni_status_save_toggle_osc(g_default_layer);
                redraw_all(dev, font);
            } else {
                redraw_all(dev, font);
            }
            return;
        }
    }

    for (uint8_t i = 0; i < NUM_TOG; i++) {
        const toggle_cfg_t *cfg = &toggles[i];
        const uint16_t x = cfg->x, y = cfg->y;
        const uint16_t l = x - BTN_W/2, t = y - BTN_H/2;
        const uint16_t r = x + BTN_W/2, b = y + BTN_H/2;



        if (!point_in_rect(tx, ty, l, t, r, b)) continue;
        prof_index_t pi = current_profile_index();
        os_tog_state[pi][i] ^= 1;
        const bool new_state = (os_tog_state[pi][i] != 0);
        qp_rect(dev, 0, 130, 240, 150, 0, 0, 0, true);
        qp_rect(dev, 0, 160, 240, 240, 0, 0, 0, true);
        uint8_t row1 = (g_default_layer == 0) ? 11 : 14;
        uint8_t row2 = (g_default_layer == 0) ? 12 : 15;

        if (!new_state) {
            sel_idx = -1;
            draw_toggle_normal(i, dev, font, hue_main_color, sat_main_color, 190);

            apply_to_param(i, true, dynamic_keymap_get_keycode(4, row1, i) - 0x7700);
            apply_to_param(i, false, dynamic_keymap_get_keycode(4, row2, i) - 0x7700);

            return;
        }
        {
            const char *txt = cfg->summary;
            qp_drawtext_recolor(dev, (240/2) - qp_textwidth(font, txt)/2, 130, font, txt, hue_main_color, sat_main_color, 190, 0, 0, 0);
        }

        const uint8_t bar_mode = cfg->bar_mode;
        if (bar_mode == 0) {
            sel_idx = -1;
        } else {
            sel_idx = i;
            const uint8_t upper = BAR_L + omni_status_load_bar(pi, true,  i);
            const uint8_t lower = BAR_L + omni_status_load_bar(pi, false, i);
            if (bar_mode >= 1) {
                draw_bar(dev, BAR_Y1, hue_main_color, sat_main_color, upper, font, line_height, cfg->bar_upper);
            }
            if (bar_mode == 2) {
                draw_bar(dev, BAR_Y2, hue_main_color, sat_main_color, lower, font, line_height, cfg->bar_lower);
            }
        }

        draw_toggle_normal(i, dev, font, hue_main_color, sat_main_color, 190);



        apply_to_param(i, true, dynamic_keymap_get_keycode(4, row1, i) - 0x7700);
        apply_to_param(i, false, dynamic_keymap_get_keycode(4, row2, i) - 0x7700);
        
        return;
    }

    if (sel_idx >= 0) {
        prof_index_t pi = current_profile_index();
        const toggle_cfg_t *cfg = &toggles[sel_idx];
        const uint8_t bar_mode = cfg->bar_mode;
        if (bar_mode >= 1 && point_in_rect(tx, ty, BAR_L-10, BAR_Y1-25, BAR_R+10, BAR_Y1+7)) {
            os_bar_upper[pi][sel_idx] = clamp_bar_x(tx);
            draw_bar(dev, BAR_Y1, hue_main_color, sat_main_color, os_bar_upper[pi][sel_idx], font, line_height, cfg->bar_upper);
            apply_to_param(sel_idx, true,  os_bar_upper[pi][sel_idx] - BAR_L);
            omni_status_save_bar(pi, true,  sel_idx, os_bar_upper[pi][sel_idx] - BAR_L);
        }
        if (bar_mode == 2 && point_in_rect(tx, ty, BAR_L-10, BAR_Y2-7, BAR_R+10, BAR_Y2+25)) {
            os_bar_lower[pi][sel_idx] = clamp_bar_x(tx);
            draw_bar(dev, BAR_Y2, hue_main_color, sat_main_color, os_bar_lower[pi][sel_idx], font, line_height, cfg->bar_lower);
            apply_to_param(sel_idx, false,  os_bar_lower[pi][sel_idx] - BAR_L);
            omni_status_save_bar(pi, false, sel_idx, os_bar_lower[pi][sel_idx] - BAR_L);
        }
    }
}

void status_view_init(painter_device_t dev, painter_font_handle_t font) {
    draw_background_all_black();
    persist_load_all();
    redraw_all(dev, font);
}

void load_toggle_data(void){
    is_on_aos = omni_status_load_toggle_aos();
    if (!is_on_aos) {
        g_default_layer = omni_status_load_toggle_osc();
        default_layer_set(1UL << g_default_layer);
    }
    for (uint8_t j = 0; j < PROF_COUNT; j++) {
        for (uint8_t i = 0; i < NUM_TOG; i++) {
            os_tog_state[j][i] = omni_status_load_toggle_normal(j, i);
        }
    }
}

void sync_default_layer_to_os(void) {
    os_variant_t cur = current_os_variant();
    if (is_on_aos) {
        uint8_t target = layer_for_os(cur);
        default_layer_set(1UL << target);
    }
    g_default_layer = get_highest_layer(default_layer_state);
    load_toggle_data();
    load_params_from_prms();
    
}



