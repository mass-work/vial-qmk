// Copyright 2025 mass
// SPDX-License-Identifier: GPL-2.0-or-later
#include <math.h>
#include "config_omni.h"
#include "dynamic_keymap.h"

uint8_t hue_bg = 0;
uint8_t sat_bg = 0;
uint8_t val_bg = 0;
uint8_t hue_main_color = 255;
uint8_t sat_main_color = 0;
uint8_t val_main_color = 255;
uint8_t hue_sub_color = 255;
uint8_t sat_sub_color = 0;
uint8_t val_sub_color = 255;
uint8_t sleep_view_state;
uint8_t swipe_view_state;

float speed_adjust1;
int slope_factor1;
float speed_adjust2;
int slope_factor2; 

bool keymap_change_update_flag = true;

void save_omni_color_config(void) {
    keymap_change_update_flag = false;
    dynamic_keymap_set_keycode(3,10,0, 0x7700 + hue_bg);
    dynamic_keymap_set_keycode(3,10,1, 0x7700 + sat_bg);
    dynamic_keymap_set_keycode(3,10,2, 0x7700 + val_bg);
    dynamic_keymap_set_keycode(3,11,0, 0x7700 + hue_main_color);
    dynamic_keymap_set_keycode(3,11,1, 0x7700 + sat_main_color);
    dynamic_keymap_set_keycode(3,11,2, 0x7700 + val_main_color);
    dynamic_keymap_set_keycode(3,11,3, 0x7700 + hue_sub_color);
    dynamic_keymap_set_keycode(3,11,4, 0x7700 + sat_sub_color);
    dynamic_keymap_set_keycode(3,11,5, 0x7700 + val_sub_color);
    dynamic_keymap_set_keycode(3,10,4, 0x7700 + sleep_view_state);
    dynamic_keymap_set_keycode(3,10,5, 0x7700 + swipe_view_state); 
    keymap_change_update_flag = true;
}

void load_omni_color_config(void) {
    hue_bg         = dynamic_keymap_get_keycode(3,10,0) - 0x7700;
    sat_bg         = dynamic_keymap_get_keycode(3,10,1) - 0x7700;
    val_bg         = dynamic_keymap_get_keycode(3,10,2) - 0x7700;
    hue_main_color = dynamic_keymap_get_keycode(3,11,0) - 0x7700;
    sat_main_color = dynamic_keymap_get_keycode(3,11,1) - 0x7700;
    val_main_color = dynamic_keymap_get_keycode(3,11,2) - 0x7700;
    hue_sub_color  = dynamic_keymap_get_keycode(3,11,3) - 0x7700;
    sat_sub_color  = dynamic_keymap_get_keycode(3,11,4) - 0x7700;
    val_sub_color  = dynamic_keymap_get_keycode(3,11,5) - 0x7700;
    sleep_view_state  = dynamic_keymap_get_keycode(3,10,4) - 0x7700;
    swipe_view_state  = dynamic_keymap_get_keycode(3,10,5) - 0x7700;
}

void save_omni_tb_config(void) {
    keymap_change_update_flag = false;
    dynamic_keymap_set_keycode(3,12,0, 0x7700 + (int)round(speed_adjust1 * 10));
    dynamic_keymap_set_keycode(3,12,1, 0x7700 + slope_factor1);
    dynamic_keymap_set_keycode(3,12,2, 0x7700 + (int)round(speed_adjust2 * 10));
    dynamic_keymap_set_keycode(3,12,3, 0x7700 + slope_factor2);
    keymap_change_update_flag = true;
}

void load_omni_tb_config(void) {
    speed_adjust1 =  (dynamic_keymap_get_keycode(3,12,0) - 0x7700) / 10.0f;
    slope_factor1 =   dynamic_keymap_get_keycode(3,12,1) - 0x7700;
    speed_adjust2 =  (dynamic_keymap_get_keycode(3,12,2) - 0x7700) / 10.0f;
    slope_factor2 =   dynamic_keymap_get_keycode(3,12,3) - 0x7700;
    if (speed_adjust1 > 3.0f || speed_adjust1 < 0.1f) {
        speed_adjust1 = DEFAULT_SPEED_ADJUST1;
    }
    if (slope_factor1 > 100 || slope_factor1 < 10) {
        slope_factor1 = DEFAULT_SLOPE_FACTOR1;
    }
    if (speed_adjust2 > 3.0f || speed_adjust2 < 0.1f) {
        speed_adjust2 = DEFAULT_SPEED_ADJUST2;
    }
    if (slope_factor2 > 100 || slope_factor2 < 10) {
        slope_factor2= DEFAULT_SLOPE_FACTOR2;
    }
}

static inline uint16_t BASE_ENC(uint16_t v) { return 0x7700u + v; }
static inline uint16_t ENC_BOOL(bool v)     { return BASE_ENC(v ? 1u : 0u); }
static inline uint16_t ENC_U8(uint8_t v)    { return BASE_ENC((uint16_t)v); }
static inline uint8_t  DEC_U8(uint16_t kc)  { return (kc >= 0x7700u) ? (uint8_t)(kc - 0x7700u) : 0u; }

static inline uint8_t row_tog(os_variant_t os)  { return (os == OS_MACOS) ? 13 :10;  }
static inline uint8_t row_up (os_variant_t os)  { return (os == OS_MACOS) ? 14 :11;  }
static inline uint8_t row_low(os_variant_t os)  { return (os == OS_MACOS) ? 15 :12;  }

bool         is_on_aos  = false;
os_variant_t manual_os  = OS_WINDOWS;

void omni_status_save_global(void) {
    keymap_change_update_flag = false;
    dynamic_keymap_set_keycode(3, 12, 4, ENC_BOOL(is_on_aos));
    uint8_t mos = (manual_os == OS_MACOS) ? 1 : 0;
    dynamic_keymap_set_keycode(3, 12, 5, ENC_U8(mos));
    keymap_change_update_flag = true;
}

void omni_status_load_global(void) {
    uint16_t kc_aos = dynamic_keymap_get_keycode(3, 12, 4);
    uint16_t kc_mos = dynamic_keymap_get_keycode(3, 12, 5);
    is_on_aos = (DEC_U8(kc_aos) != 0);
    manual_os = (DEC_U8(kc_mos) == 1) ? OS_MACOS : OS_WINDOWS;
}

uint8_t osbuf_tog_win[STATUS_TOG_COUNT] = {0};
uint8_t osbuf_up_win [STATUS_TOG_COUNT] = {0};
uint8_t osbuf_lo_win [STATUS_TOG_COUNT] = {0};
uint8_t osbuf_tog_mac[STATUS_TOG_COUNT] = {0};
uint8_t osbuf_up_mac [STATUS_TOG_COUNT] = {0};
uint8_t osbuf_lo_mac [STATUS_TOG_COUNT] = {0};

static inline uint8_t  clamp100(uint8_t v) { return (v > 100) ? 100 : v; }

static void save_rows(os_variant_t os, const uint8_t *tog, const uint8_t *up, const uint8_t *lo){
    const uint8_t rt = row_tog(os), ru = row_up(os), rl = row_low(os);
    keymap_change_update_flag = false;
    for (uint8_t i=0; i<STATUS_TOG_COUNT; i++){
        dynamic_keymap_set_keycode(4, rt, i, ENC_U8(tog ? (tog[i]?1:0) : 0));
        dynamic_keymap_set_keycode(4, ru, i, ENC_U8(up  ? clamp100(up[i]) : 0));
        dynamic_keymap_set_keycode(4, rl, i, ENC_U8(lo  ? clamp100(lo[i]) : 0));
    }
    keymap_change_update_flag = true;
}

static void load_rows(os_variant_t os, uint8_t *tog, uint8_t *up, uint8_t *lo){
    const uint8_t rt = row_tog(os), ru = row_up(os), rl = row_low(os);
    for (uint8_t i=0; i<STATUS_TOG_COUNT; i++){
        if (tog) tog[i] = DEC_U8(dynamic_keymap_get_keycode(4, rt, i)) ? 1 : 0;
        if (up)  up[i]  = clamp100(DEC_U8(dynamic_keymap_get_keycode(4, ru, i)));
        if (lo)  lo[i]  = clamp100(DEC_U8(dynamic_keymap_get_keycode(4, rl, i)));
    }
}

void omni_status_save_os_win(void){ save_rows(OS_WINDOWS, osbuf_tog_win, osbuf_up_win, osbuf_lo_win); }
void omni_status_save_os_mac(void){ save_rows(OS_MACOS,  osbuf_tog_mac, osbuf_up_mac, osbuf_lo_mac); }
void omni_status_load_os_win(void){ load_rows(OS_WINDOWS, osbuf_tog_win, osbuf_up_win, osbuf_lo_win); }
void omni_status_load_os_mac(void){ load_rows(OS_MACOS,  osbuf_tog_mac, osbuf_up_mac, osbuf_lo_mac); }

static inline uint8_t clamp_1_100_int(int v) {
    if (v < 1)   return 1;
    if (v > 100) return 100;
    return (uint8_t)v;
}

static inline uint8_t clamp_0_100_int(int v) {
    if (v < 0)   return 0;
    if (v > 100) return 100;
    return (uint8_t)v;
}

void omni_status_save_toggle_aos(bool state) {
    keymap_change_update_flag = false;
    dynamic_keymap_set_keycode(3, 12, 4, 0x7700 + state);
    keymap_change_update_flag = true;
}

void omni_status_save_toggle_osc(uint8_t state) {
    keymap_change_update_flag = false;
    dynamic_keymap_set_keycode(3, 12, 5, 0x7700 + state);
    keymap_change_update_flag = true;
}

void omni_status_save_toggle_normal(uint8_t profile, uint8_t index, bool state) {
    keymap_change_update_flag = false;
    uint8_t val = (profile == 0) ? 10 : (profile == 1 ? 13 : 0);
    dynamic_keymap_set_keycode(4, val, index, 0x7700 + state);
    keymap_change_update_flag = true;
}

bool omni_status_load_toggle_aos(void) {
    bool state = dynamic_keymap_get_keycode(3, 12, 4) - 0x7700;
    return state;
}

bool omni_status_load_toggle_osc(void) {
    bool state = dynamic_keymap_get_keycode(3, 12, 5) - 0x7700;
    return state;
}

bool omni_status_load_toggle_normal(uint8_t profile, uint8_t index) {
    uint8_t val = (profile == 0) ? 10 : (profile == 1 ? 13 : 0);
    bool state = dynamic_keymap_get_keycode(4, val, index) - 0x7700;
    return state;
}

static inline uint8_t row_bar(uint8_t profile, bool upper) {
    if (upper) return profile ? 14 : 11;  // SUB:14 / BASE:11
    else       return profile ? 15 : 12;  // SUB:15 / BASE:12
}

uint8_t omni_status_load_bar(uint8_t profile, bool upper, uint8_t index) {
    int v = (int)dynamic_keymap_get_keycode(4, row_bar(profile, upper), index) - 0x7700;
    return clamp_0_100_int(v);
}

void omni_status_save_bar(uint8_t profile, bool upper, uint8_t index, uint8_t value) {
    uint8_t  v   = clamp_0_100_int(value);
    uint8_t  row = row_bar(profile, upper);
    uint16_t kc  = 0x7700u + v;

    if (dynamic_keymap_get_keycode(4, row, index) == kc) return;

    keymap_change_update_flag = false;
    dynamic_keymap_set_keycode(4, row, index, kc);
    keymap_change_update_flag = true;
}
