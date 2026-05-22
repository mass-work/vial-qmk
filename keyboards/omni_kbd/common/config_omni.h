#pragma once
#include "quantum.h"
#include <stdint.h>
#include <stdbool.h>
#include "os_detection.h"

#define DEFAULT_SPEED_ADJUST1 1.6f
#define DEFAULT_SLOPE_FACTOR1 50
#define DEFAULT_SPEED_ADJUST2 1.4f
#define DEFAULT_SLOPE_FACTOR2 70

extern bool keymap_change_update_flag;

extern uint8_t hue_bg;
extern uint8_t sat_bg;
extern uint8_t val_bg;
extern uint8_t hue_main_color;
extern uint8_t sat_main_color;
extern uint8_t val_main_color;
extern uint8_t hue_sub_color;
extern uint8_t sat_sub_color;
extern uint8_t val_sub_color;

extern float speed_adjust1;
extern int slope_factor1;
extern float speed_adjust2;
extern int slope_factor2; 

void save_omni_color_config(void);
void load_omni_color_config(void);
void save_omni_tb_config(void);
void load_omni_tb_config(void);

extern bool         is_on_aos;
extern os_variant_t manual_os;

void omni_status_save_global(void);
void omni_status_load_global(void);

#define STATUS_TOG_COUNT 6

extern uint8_t osbuf_tog_win[STATUS_TOG_COUNT];
extern uint8_t osbuf_up_win [STATUS_TOG_COUNT];
extern uint8_t osbuf_lo_win [STATUS_TOG_COUNT];

extern uint8_t osbuf_tog_mac[STATUS_TOG_COUNT];
extern uint8_t osbuf_up_mac [STATUS_TOG_COUNT];
extern uint8_t osbuf_lo_mac [STATUS_TOG_COUNT];

void omni_status_save_os_win(void);
void omni_status_save_os_mac(void);
void omni_status_load_os_win(void);
void omni_status_load_os_mac(void);

void omni_status_save_toggle_aos(bool state);
void omni_status_save_toggle_osc(uint8_t state);
void omni_status_save_toggle_normal(uint8_t profile, uint8_t index, bool state);
bool omni_status_load_toggle_aos(void);
bool omni_status_load_toggle_osc(void);
bool omni_status_load_toggle_normal(uint8_t profile, uint8_t index);

void omni_status_save_bar(uint8_t profile, bool upper, uint8_t index, uint8_t value);
uint8_t omni_status_load_bar(uint8_t profile, bool upper, uint8_t index);

extern uint8_t sleep_view_state;
extern uint8_t swipe_view_state;
