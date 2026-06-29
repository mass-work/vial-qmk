#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "qp.h"
#include "os_detection.h"

#define BTN_W   52
#define BTN_H   26
#define BTN_R    6

#define BAR_L   90
#define BAR_R   190
#define BAR_Y1  175
#define BAR_Y2  200

enum keyboard_layers {
    _BASE,
    _SUB,
    _NUM,
    _SYMBOL,
    _CUSTOM,
};

enum {
    COL0_X =  90,
    COL1_X = 150,
    COL2_X =  50,
    COL3_X = 120,
    COL4_X = 190,
    ROW0_Y =  35,
    ROW1_Y =  70,
    ROW2_Y = 105,
};

typedef enum { 
    PROF_BASE = 0, 
    PROF_SUB = 1, 
    PROF_COUNT = 2 
} prof_index_t;

const char *os_name_short(os_variant_t os);
void status_view_init(painter_device_t dev, painter_font_handle_t font);
void ui_handle_touch(painter_device_t dev, painter_font_handle_t font, int16_t tx, int16_t ty);
void persist_load_all(void);
void sync_default_layer_to_os(void);

extern uint8_t hi_res_interval_v;
extern uint8_t hi_res_value_v;
extern uint8_t hi_res_interval_h;
extern uint8_t hi_res_value_h;
// extern int8_t ud_sc_mode_flag;
// extern int8_t lr_sc_mode_flag;
extern uint16_t touch_repeat_interval;
extern uint16_t touch_single_interval;

extern bool is_tb_haptic;
extern bool is_td_haptic;

bool sv_hrv_enabled_current(void);
bool sv_hrh_enabled_current(void);