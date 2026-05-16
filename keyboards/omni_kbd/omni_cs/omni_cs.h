#pragma once

#include "quantum.h"
#include <stdint.h>
#include "../drivers/cst816t.h"
#include "../common/touch_lcd_omni.h"

#define ENABLE_TOUCH_UPDATE   1 
#define SLEEPING_KB_TIME      60000
#define SLEEP_VIEW            2 // 0: none 1: code rain 2: picture 3:picture and code rain
#define SLEEP_VIEW_SWITCH_TIME  30000 // 30 sec

typedef enum {
    TRACKBALL_CURSOR = 0,
    TRACKBALL_TAP    = 1,
} trackball_mode_t;

enum custom_keycodes {
    KC_hue_bg_UP = QK_KB_0,
    KC_hue_bg_DOWN,
    KC_sat_bg_UP,
    KC_sat_bg_DOWN,
    KC_val_bg_UP,
    KC_val_bg_DOWN,
    KC_hue_main_color_UP,
    KC_hue_main_color_DOWN,
    KC_sat_main_color_UP,
    KC_sat_main_color_DOWN,
    KC_val_main_color_UP,
    KC_val_main_color_DOWN,
    KC_hue_sub_color_UP,
    KC_hue_sub_color_DOWN,
    KC_sat_sub_color_UP,
    KC_sat_sub_color_DOWN,
    KC_val_sub_color_UP,
    KC_val_sub_color_DOWN,
    TB_R_MODE_TOGGLE,
    TB_L_MODE_TOGGLE,
    KC_DP_TOUCH_KEY0,
    KC_DP_TOUCH_KEY1,
    KC_DP_TOUCH_KEY2,
    KC_DP_TOUCH_KEY3,
    KC_DP_TB_TUNE,
    KC_DP_SWIPE_GESTURE,
    KC_DP_KEY_MAT,
    KC_DP_STAT1,
};

extern point_t circles[6];
extern uint16_t virtual_keycode[72];
extern bool is_haptic;

typedef enum {
    ORIENT_0,
    ORIENT_90,
    ORIENT_180,
    ORIENT_270
} orientation_t;