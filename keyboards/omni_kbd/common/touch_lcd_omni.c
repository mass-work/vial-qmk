// Copyright 2025 mass
// SPDX-License-Identifier: GPL-2.0-or-later

#include "qp.h"
#include "hardware/gpio.h"
#include <math.h>
#include "touch_lcd_omni.h"
#include "matrix.h"
#include "draw_custom.h"
#include "view_keymap.h"
#include "status_view.h"
#include "dynamic_keymap.h"
#include "../drivers/cst816t.h"
#include "../icon/omni_image_loader.h"
#include "../common/omni_icon_image.h"

#define SAMPLE_MS       1
#define WINDOW_SLOTS    85
#define CUSTOM_ICON_KEYCODE 0x77FE  // M254
#ifndef DISPLAY_MODE_DEFAULT
#    define DISPLAY_MODE_DEFAULT DISPLAY_MODE_TOUCH_KEY
#endif

painter_device_t display;
ImagePosition lcd_layer_app_images[MAX_LCD_LAYER + 1][7];
uint8_t current_layer = 0;
uint8_t current_lcd_layer = 0;
painter_font_handle_t noto9_font;
painter_font_handle_t noto11_font;
painter_font_handle_t roboto_mono16;
painter_font_handle_t st2_mono16;
uint16_t touch_x = 0xFFFF;
uint16_t touch_y = 0xFFFF;
uint16_t touch_start_timer = 0;
bool touch_signal = false;
bool touch_start_flag = false; 
bool initial_touch_flag = false;
bool touch_signal_view_update = false;
uint8_t gesture_id = GESTURE_NONE;
display_mode_t display_mode = DISPLAY_MODE_DEFAULT;
painter_image_handle_t qp_load_image_mem(const void *buffer);
painter_image_handle_t image_logo;
painter_image_handle_t image_save;
painter_image_handle_t layer_00, layer_01, layer_02, layer_03;
painter_image_handle_t image_000, image_001, image_002, image_003, image_004, image_005, image_006, image_007, image_008, image_009, image_010, image_011, image_012, image_013, image_014, image_015, image_016, image_017, image_018, image_019, image_020, image_021, image_022, image_023, image_024, image_025, image_026, image_027, image_028, image_029, image_030, image_031, image_032, image_033, image_034, image_035, image_036, image_037, image_038, image_039, image_040, image_041, image_042, image_043, image_044, image_045, image_046, image_047, image_048, image_049, image_050, image_051, image_052, image_053, image_054, image_055, image_056, image_057, image_058, image_059, image_060, image_061, image_062, image_063, image_064, image_065, image_066, image_067, image_068, image_069, image_070, image_071, image_072, image_073, image_074, image_075, image_076, image_077, image_078, image_079, image_080, image_081, image_082, image_083, image_084, image_085, image_086, image_087, image_088, image_089, image_090, image_091, image_092, image_093, image_094, image_095, image_096, image_097, image_098, image_099, image_100, image_101, image_102, image_103, image_104, image_105, image_106, image_107, image_108, image_109, image_110, image_111, image_112, image_113, image_114, image_115, image_116, image_117, image_118, image_119, image_120, image_121, image_122, image_123, image_124, image_125, image_126, image_127, image_128, image_129, image_130, image_131, image_132, image_133, image_134, image_135, image_136, image_137, image_138, image_139, image_140, image_141, image_142, image_143, image_144, image_145, image_146, image_147, image_148, image_149, image_150, image_151, image_152, image_153, image_154, image_155, image_156, image_157, image_158, image_159, image_160, image_161, image_162, image_163, image_164, image_165, image_166, image_167, image_168, image_169, image_170, image_171, image_172, image_173, image_174, image_175, image_176, image_177, image_178, image_179, image_180, image_181, image_182, image_183, image_184, image_185, image_186, image_187, image_188, image_189, image_190, image_191, image_192, image_193, image_194, image_195, image_196, image_197, image_198, image_199, image_200, image_201, image_202, image_203, image_204, image_205, image_206, image_207, image_208, image_209, image_210, image_211, image_212, image_213, image_214, image_215, image_216, image_217, image_218, image_219, image_220, image_221, image_222, image_223, image_224, image_225, image_226, image_227, image_228, image_229, image_230, image_231, image_232, image_233, image_234, image_235, image_236, image_237, image_238, image_239, image_240, image_241, image_242, image_243, image_244, image_245, image_246, image_247, image_248, image_249, image_250, image_251, image_252, image_253, image_254;
uint8_t touch_data[6];
ImagePosition images[7];
static uint16_t last_tap_time = 0;
static bool prev_pin = 1;
static uint16_t fall_time = 0;
float pre_speed_adjust1;
int pre_slope_factor1;
float pre_speed_adjust2;
int pre_slope_factor2;
static uint8_t hue_tbtune_r = 220;
static uint8_t sat_tbtune_r = 255;
static uint8_t val_tbtune_r = 255;
static uint8_t hue_tbtune_l = 135;
static uint8_t sat_tbtune_l = 255;
static uint8_t val_tbtune_l = 255;
static uint8_t x_pos1 = TOUCH_LCD_WIDTH / 2 + 50;
static uint8_t y_pos1 = TOUCH_LCD_HEIGHT / 2 - 10;
static uint8_t x_pos2 = TOUCH_LCD_WIDTH / 2 - 50;
static uint8_t y_pos2 = TOUCH_LCD_HEIGHT / 2 - 10;
static uint8_t y_pos3 = TOUCH_LCD_HEIGHT / 2 - 50;
static uint8_t y_pos4 = TOUCH_LCD_HEIGHT / 2 + 45;
static uint8_t x_pos_save = TOUCH_LCD_WIDTH / 2;
static uint8_t y_pos_save = TOUCH_LCD_HEIGHT / 2 + 90;
static int16_t text1_width, text3_width, text4_width, text5_width, text6_width = 0;
static const char *text1 = "TB TUNE";
static const char *text3 = "Right";
static const char *text4 = "Left";
static const char *text5 = "+      +";
static const char *text6 = "-      -";
static uint8_t  buf[WINDOW_SLOTS];
static uint8_t  idx = 0;
static uint8_t  count_low = 0;
static bool     slot_low_seen = false;
static uint16_t slot_t0 = 0;
uint8_t contact_off_hold_ms = 15;
static uint16_t off_hold_t0 = 0;
bool touch_buf_init_flag = false;
static uint16_t fast_touch_time = 0;
static uint16_t second_touch_time = 0;
static uint16_t touch_repeat_time = 0;
static uint16_t pre_touch_x;
static uint16_t pre_touch_y;
static uint16_t now_touch_x;
static uint16_t now_touch_y;
int32_t ax;
int32_t ay;
int32_t r_approx;
uint8_t press_deadzone = 6;

typedef enum {
    TD_TAP_LEFT = 0,
    TD_TAP_RIGHT,
    TD_TAP_UP,
    TD_TAP_DOWN,
    TD_TAP_SLOT_COUNT
} td_tap_slot_t;

typedef struct {
  bool in_contact;
} touch_fsm_t;
static touch_fsm_t T;

typedef enum {
    TOUCH_MODE_NONE = 0,
    TOUCH_MODE_PRESS,
    TOUCH_MODE_SWIPE
} touch_mode_t;
static touch_mode_t touch_mode = TOUCH_MODE_NONE;

typedef enum {
    TOUCH_STATE_NONE = 0,
    TOUCH_STATE_START,
    TOUCH_STATE_WAIT,
    TOUCH_STATE_REPEAT,
    TOUCH_STATE_SINGLE
} touch_state_t;
static touch_state_t touch_state = TOUCH_STATE_NONE;

void process_touch_trackball_tuning_mode(uint16_t touch_x, uint16_t touch_y);

painter_image_handle_t* get_layer_img_func(uint16_t layer_count) {
    static painter_image_handle_t* layer_image_ptrs[4] = {
        &layer_00, &layer_01, &layer_02, &layer_03
    };
    return layer_image_ptrs[layer_count];
}

painter_image_handle_t* get_img_func(uint16_t keycode) {
    static painter_image_handle_t* image_ptrs[MACRO_KEY_COUNT] = {
        &image_000, &image_001, &image_002, &image_003, &image_004, &image_005, &image_006, &image_007, &image_008, &image_009, &image_010, &image_011, &image_012, &image_013, &image_014, &image_015, &image_016, &image_017, &image_018, &image_019, &image_020, &image_021, &image_022, &image_023, &image_024, &image_025, &image_026, &image_027, &image_028, &image_029, &image_030, &image_031, &image_032, &image_033, &image_034, &image_035, &image_036, &image_037, &image_038, &image_039, &image_040, &image_041, &image_042, &image_043, &image_044, &image_045, &image_046, &image_047, &image_048, &image_049, &image_050, &image_051, &image_052, &image_053, &image_054, &image_055, &image_056, &image_057, &image_058, &image_059, &image_060, &image_061, &image_062, &image_063, &image_064, &image_065, &image_066, &image_067, &image_068, &image_069, &image_070, &image_071, &image_072, &image_073, &image_074, &image_075, &image_076, &image_077, &image_078, &image_079, &image_080, &image_081, &image_082, &image_083, &image_084, &image_085, &image_086, &image_087, &image_088, &image_089, &image_090, &image_091, &image_092, &image_093, &image_094, &image_095, &image_096, &image_097, &image_098, &image_099, &image_100, &image_101, &image_102, &image_103, &image_104, &image_105, &image_106, &image_107, &image_108, &image_109, &image_110, &image_111, &image_112, &image_113, &image_114, &image_115, &image_116, &image_117, &image_118, &image_119, &image_120, &image_121, &image_122, &image_123, &image_124, &image_125, &image_126, &image_127, &image_128, &image_129, &image_130, &image_131, &image_132, &image_133, &image_134, &image_135, &image_136, &image_137, &image_138, &image_139, &image_140, &image_141, &image_142, &image_143, &image_144, &image_145, &image_146, &image_147, &image_148, &image_149, &image_150, &image_151, &image_152, &image_153, &image_154, &image_155, &image_156, &image_157, &image_158, &image_159, &image_160, &image_161, &image_162, &image_163, &image_164, &image_165, &image_166, &image_167, &image_168, &image_169, &image_170, &image_171, &image_172, &image_173, &image_174, &image_175, &image_176, &image_177, &image_178, &image_179, &image_180, &image_181, &image_182, &image_183, &image_184, &image_185, &image_186, &image_187, &image_188, &image_189, &image_190, &image_191, &image_192, &image_193, &image_194, &image_195, &image_196, &image_197, &image_198, &image_199, &image_200, &image_201, &image_202, &image_203, &image_204, &image_205, &image_206, &image_207, &image_208, &image_209, &image_210, &image_211, &image_212, &image_213, &image_214, &image_215, &image_216, &image_217, &image_218, &image_219, &image_220, &image_221, &image_222, &image_223, &image_224, &image_225, &image_226, &image_227, &image_228, &image_229, &image_230, &image_231, &image_232, &image_233, &image_234, &image_235, &image_236, &image_237, &image_238, &image_239, &image_240, &image_241, &image_242, &image_243, &image_244, &image_245, &image_246, &image_247, &image_248, &image_249, &image_250, &image_251, &image_252, &image_253, &image_254
    };
    if (keycode >= MACRO_KEY_START && keycode <= MACRO_KEY_END) {
        return image_ptrs[keycode - MACRO_KEY_START];
    }
    return &image_001;
}

painter_image_handle_t* get_custom_img_func_for_slot(uint8_t slot) {
    painter_image_handle_t *custom_image = omni_icon_get_image_ptr(slot);

    if (custom_image != NULL && *custom_image != NULL) {
        return custom_image;
    }

    return &image_254;
}

painter_image_handle_t* get_img_func_for_slot(uint16_t keycode, uint8_t slot) {
    if (keycode == CUSTOM_ICON_KEYCODE) {
        return get_custom_img_func_for_slot(slot);
    }

    return get_img_func(keycode);
}

painter_image_handle_t* get_center_img_func_for_slot(uint16_t keycode, uint8_t slot, uint8_t page) {
    if (keycode == CUSTOM_ICON_KEYCODE) {
        return get_custom_img_func_for_slot(slot);
    }

    return get_layer_img_func(page);
}

void draw_background(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    qp_rect(display, x1, y1, x2, y2, hue_bg, sat_bg, val_bg, true);
}

void draw_background_black(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    qp_rect(display, x1, y1, x2, y2, 0, 0, 0, true);
}

void draw_background_all(void) {
    qp_rect(display, 0, 0, TOUCH_LCD_WIDTH, TOUCH_LCD_HEIGHT, hue_bg, sat_bg, val_bg, true);
}

void draw_background_all_black(void) {
    qp_rect(display, 0, 0, TOUCH_LCD_WIDTH, TOUCH_LCD_HEIGHT, 0, 0, 0, true);
}

void draw_lcd_layer_category_images(void) {
    ImagePosition *images = lcd_layer_app_images[current_lcd_layer];
    int image_count = sizeof(lcd_layer_app_images[current_lcd_layer]) / sizeof(lcd_layer_app_images[current_lcd_layer][0]);
    for (int i = 0; i < image_count; i++) {
        if (*images[i].image != NULL) {
            int draw_x = images[i].x - (*images[i].image)->width / 2;
            int draw_y = images[i].y - (*images[i].image)->height / 2;
            qp_drawimage(display, draw_x, draw_y, *images[i].image);
        }
    }
    qp_flush(display);
}
    
void swipe_gesture_swipe_omni_logo(void) {
    uint8_t case_depth = 160;
    uint8_t case_width = case_depth * 0.79f;
    float case_angle = 30.0f;
    float sball_angle = case_angle + 193.0f; //194
    uint8_t ball_size = case_depth * 0.25f;
    uint8_t ball_spase = ball_size * 0.93f;
    float ball_position_mr = case_depth * 0.23f;
    float ball_position_sr = case_depth * 0.21f; //0.215
    float rad_m = case_angle * (float)M_PI / 180.0f;
    float rad_s = sball_angle * (float)M_PI / 180.0f;
    uint8_t ball_position_mx = (uint8_t)lroundf(120.0f - ball_position_mr * sinf(rad_m));
    uint8_t ball_position_my = (uint8_t)lroundf(120.0f + ball_position_mr * cosf(rad_m));
    uint8_t ball_position_sx = (uint8_t)lroundf(120.0f - ball_position_sr * sinf(rad_s));
    uint8_t ball_position_sy = (uint8_t)lroundf(120.0f + ball_position_sr * cosf(rad_s));
    draw_background(0, 0, TOUCH_LCD_WIDTH, TOUCH_LCD_HEIGHT);
    qp_rotated_ellipse_filled(display, 120, 120, case_width, case_depth, case_angle, hue_main_color, sat_main_color, val_main_color);
    qp_circle(display, ball_position_mx, ball_position_my, ball_size, hue_bg, sat_bg, val_bg, true);
    qp_circle(display, ball_position_mx, ball_position_my, ball_spase, hue_sub_color, sat_sub_color, val_sub_color, true);
    qp_triangle_rotated(display, 120, 218, 20, 17,   0.0f, hue_main_color, sat_main_color, val_main_color, true, 1);
    qp_triangle_rotated(display,  22, 120, 20, 17,  90.0f, hue_main_color, sat_main_color, val_main_color, true, 1);   
    qp_triangle_rotated(display, 120,  22, 20, 17, 180.0f, hue_main_color, sat_main_color, val_main_color, true, 1);
    qp_triangle_rotated(display, 218, 120, 20, 17, 270.0f, hue_main_color, sat_main_color, val_main_color, true, 1);
    qp_circle(display, ball_position_sx, ball_position_sy, 8, hue_bg, sat_bg, val_bg, true);
    qp_circle(display, ball_position_sx, ball_position_sy, 6, hue_sub_color, sat_sub_color, val_sub_color, true);
    qp_draw_mni_rotated_solid(display, 160, 118, 270.0f, 1.4f, hue_bg, sat_bg, val_bg);
    qp_flush(display);
}

static const keypos_t td_tap_keypos[TD_TAP_SLOT_COUNT] = {
    [TD_TAP_LEFT]  = { .row = 4, .col = 4 },
    [TD_TAP_RIGHT] = { .row = 4, .col = 5 },
    [TD_TAP_UP]    = { .row = 5, .col = 4 },
    [TD_TAP_DOWN]  = { .row = 5, .col = 5 },
};

static void td_tap_virtual_key(td_tap_slot_t slot) {
    if (slot >= TD_TAP_SLOT_COUNT) {
        return;
    }

    keypos_t pos = td_tap_keypos[slot];
    uint8_t layer   = get_highest_layer(layer_state | default_layer_state);
    uint16_t keycode = dynamic_keymap_get_keycode(layer, pos.row, pos.col);

    if (keycode == KC_NO) {
        return;
    }

    if (keycode >= MACRO_KEY_START && keycode <= MACRO_KEY_END) {
        uint8_t macro_id = keycode - MACRO_KEY_START;
        dynamic_keymap_macro_send(macro_id);
        return;
    }

    tap_code16(keycode);
}

void swipe_gesture_vkey_process(void) {
    touch_signal_view_update = true;
    switch (gesture_id) {
        case CST816S_SLIDE_RIGHT:
            td_tap_virtual_key(TD_TAP_RIGHT);
            break;
        case CST816S_SLIDE_LEFT:
            td_tap_virtual_key(TD_TAP_LEFT);
            break;
        case CST816S_SLIDE_UP:
            td_tap_virtual_key(TD_TAP_UP);
            break;
        case CST816S_SLIDE_DOWN:
            td_tap_virtual_key(TD_TAP_DOWN);
            break;
        default:
            break;
    }
}

const char* get_layer_name(uint8_t layer) {
    switch (layer) {
        case 0:
            return "BASE";
        case 1:
            return "SUB ";
        case 2:
            return "NUM ";
        case 3:
            return "SYMB";
        case 4:
            return "CUST";
        default:
            return "UNKN";
    }
}

bool is_touch_in_circle(uint16_t touch_x, uint16_t touch_y, point_t circle, uint16_t radius) {
    uint32_t distance_squared = (circle.x - touch_x) * (circle.x - touch_x) + (circle.y - touch_y) * (circle.y - touch_y);
    return distance_squared <= (radius * radius);
}

void process_touch(void) {
    last_tap_time = timer_read();
    touch_signal = true;
}

void measure_pulse(uint8_t interrupt_pin) {
    bool curr_pin = interrupt_pin;
    if (prev_pin && !curr_pin) {
        uint16_t now = timer_read();
        uint16_t period = (fall_time) ? timer_elapsed(fall_time) : 0;
        fall_time = now;
        uprintf("period = %u ms\n", period);
    }
    if (!prev_pin && curr_pin) {
        uint16_t width = timer_elapsed(fall_time);
        uprintf("width  = %u ms\n", width);
    }
    prev_pin = curr_pin;
}

static inline void touch_irq_init(void) {
    setPinInputHigh(INT_PIN);
    gpio_set_irq_enabled(INT_PIN, GPIO_IRQ_EDGE_FALL, true);
}

void touch_buf_init(void) {
    for (int i = 0; i < WINDOW_SLOTS; i++) buf[i] = 0;
    idx = 0;
    count_low = 0;
    T.in_contact = false;
    slot_low_seen = false;
    slot_t0 = timer_read();
}

void touch_buf_tick(void) {
    if (gpio_get_irq_event_mask(INT_PIN) & GPIO_IRQ_EDGE_FALL) {
        slot_low_seen = true;
        gpio_acknowledge_irq(INT_PIN, GPIO_IRQ_EDGE_FALL);
    }

    while (timer_elapsed(slot_t0) >= SAMPLE_MS) {
        uint8_t v = slot_low_seen ? 1 : 0;
        count_low += v; count_low -= buf[idx];
        buf[idx] = v;
        if (++idx >= WINDOW_SLOTS) idx = 0;

        slot_t0 += SAMPLE_MS;
        slot_low_seen = false;

        if (!T.in_contact) {
            if (count_low >= 1) {
                T.in_contact = true;
                off_hold_t0 = 0;
                uprintf("[TOUCH] DOWN (count=%u)\n", count_low);
            }
        } else {
            if (count_low == 0) {
                if (off_hold_t0 == 0) {
                    off_hold_t0 = timer_read();
                } else if (timer_elapsed(off_hold_t0) >= contact_off_hold_ms) {
                    T.in_contact = false;
                    off_hold_t0 = 0;
                    uprintf("[TOUCH] UP   (count=%u)\n", count_low);
                }
            } else {
                off_hold_t0 = 0;
            }
        }
    }
}

void process_gesture(void) {
    switch (display_mode) {    
    case DISPLAY_MODE_TOUCH_KEY:
        if (touch_mode == TOUCH_MODE_PRESS) {
            touch_x = now_touch_x;
            touch_y = now_touch_y;
            process_touch();
        } else if (touch_mode == TOUCH_MODE_SWIPE) {
            swipe_gesture_vkey_process();
        }
        break;

    case DISPLAY_MODE_TRACKBALL_TUNING:
        process_touch_trackball_tuning_mode(now_touch_x, now_touch_y);
        break;

    case DISPLAY_MODE_SWIPE_GESTURE:
        swipe_gesture_vkey_process();
        break;   

    case DISPLAY_MODE_STATUS1:
        ui_handle_touch(display, noto9_font, now_touch_x, now_touch_y);
        break;                    

    default:
        break;
    }
}

void touch_mode_detected(int16_t x, int16_t y){
    if (abs(x) < press_deadzone && abs(y) < press_deadzone) {
        touch_mode = TOUCH_MODE_PRESS;
    } else {
        touch_mode = TOUCH_MODE_SWIPE;
    }
}

void swipe_gesture_id_detected(int16_t x, int16_t y){
    int16_t ax = x; 
    if (ax < 0) ax = -ax;
    int16_t ay = y; 
    if (ay < 0) ay = -ay;
    gesture_id = GESTURE_NONE;  
    if (ax > ay) {
        if (x > 0) {
            gesture_id = CST816S_SLIDE_RIGHT;
        } else if (x < 0) {
            gesture_id = CST816S_SLIDE_LEFT;
        }
    } else {
        if (y > 0) {
            gesture_id = CST816S_SLIDE_DOWN;
        } else if (y < 0) {
            gesture_id = CST816S_SLIDE_UP;
        }
    } 
}

void process_touch_interrupt(void) {

    if (!touch_buf_init_flag) {
        touch_buf_init_flag = true;
        touch_irq_init();
        touch_buf_init();
    }
    touch_buf_tick();

    if (T.in_contact) {
        if (touch_state == TOUCH_STATE_NONE) {
            cst816t_XY touch_data = cst816t_Get_Point();
            pre_touch_x = touch_data.x_point;
            pre_touch_y = touch_data.y_point;

            fast_touch_time = timer_read();
            touch_state = TOUCH_STATE_START;
            uprintf("------START------\n");
        } else if (touch_state == TOUCH_STATE_START) {
            if (timer_elapsed(fast_touch_time) > touch_repeat_interval) {
                cst816t_XY touch_data = cst816t_Get_Point();
                now_touch_x = touch_data.x_point;
                now_touch_y = touch_data.y_point;
                second_touch_time = timer_read();
                int16_t x = now_touch_x - pre_touch_x;
                int16_t y = now_touch_y - pre_touch_y;
                ax = abs(x);
                ay = abs(y);
                r_approx = ax + ay - (ax > ay ? ax : ay) / 2;
                uprintf("crood: %d %d %ld\n", x, y, r_approx);
                touch_mode_detected(x, y);
                swipe_gesture_id_detected(x, y);

                if (display_mode == DISPLAY_MODE_TOUCH_KEY && touch_mode == TOUCH_MODE_PRESS) {
                    touch_signal_view_update = true;
                    touch_state = TOUCH_STATE_REPEAT;
                    touch_repeat_time = timer_read() - touch_repeat_interval;
                    uprintf("-----PRESS -> REPEAT------\n");
                } else {
                    process_gesture();
                    touch_state = TOUCH_STATE_SINGLE;
                    uprintf("-----SINGLE------\n");
                }
            }

        } else if (touch_state == TOUCH_STATE_SINGLE) { 
            if (timer_elapsed(second_touch_time) > touch_repeat_interval / 4) {
                touch_signal = false;
                touch_state = TOUCH_STATE_WAIT;
                // uprintf("-----WAIT------\n");

            }
        } else if (touch_state == TOUCH_STATE_WAIT) {
            if (timer_elapsed(second_touch_time) > touch_single_interval) {
                touch_state = TOUCH_STATE_REPEAT;
                touch_repeat_time = timer_read();
            }
        } else if (touch_state == TOUCH_STATE_REPEAT) {

            uint16_t touch_repeat_interval_eff = touch_repeat_interval;
            if (r_approx != 0) {
                int32_t denom = r_approx / 5;
                if (denom < 1) {
                    denom = 1;
                } else if (denom > 10) {
                    denom = 10;
                }
                touch_repeat_interval_eff /= denom;
            }

            if (timer_elapsed(touch_repeat_time) > touch_repeat_interval_eff) {
                touch_repeat_time = timer_read();
                cst816t_XY touch_data = cst816t_Get_Point();
                now_touch_x = touch_data.x_point;
                now_touch_y = touch_data.y_point;
                process_gesture();
                // uprintf("-----REPEAT------\n");
            }
        }
            
    } else {
        if (timer_elapsed(fast_touch_time) > touch_repeat_interval + 30) {
            if (touch_state != TOUCH_STATE_NONE) {
                if (touch_mode == TOUCH_MODE_PRESS && touch_state == TOUCH_STATE_REPEAT) {
                    touch_signal_view_update = true;
                }
                touch_signal = false;
                touch_mode = TOUCH_MODE_NONE;
                touch_state = TOUCH_STATE_NONE;
                gesture_id = GESTURE_NONE;
                // uprintf("------reset------\n");
            }
        }
    }
}

void show_trackball_tuning_mode(void) {
    draw_background_all_black();
    if (text1_width == 0) {
        text1_width = qp_textwidth(noto11_font, text1);
    }
    if (text3_width == 0) {
        text3_width = qp_textwidth(noto11_font, text3);
    }
    if (text4_width == 0) {
        text4_width = qp_textwidth(noto11_font, text4);
    }
    if (text5_width == 0) {
        text5_width = qp_textwidth(noto11_font, text5);
    }
    if (text6_width == 0) {
        text6_width = qp_textwidth(noto11_font, text6);
    }
    qp_drawtext(display, TOUCH_LCD_WIDTH / 2 - text1_width / 2, TOUCH_LCD_HEIGHT / 2 - 70 - noto11_font->line_height, noto11_font, text1);
    qp_drawtext(display, x_pos1 - text3_width / 2, TOUCH_LCD_HEIGHT / 2 + 80 - noto11_font->line_height, noto11_font, text3);
    qp_drawtext(display, x_pos1 - text5_width / 2, y_pos3 - noto11_font->line_height / 2, noto11_font, text5);
    qp_drawtext(display, x_pos1 - text6_width / 2, y_pos4 - noto11_font->line_height / 2, noto11_font, text6);
    qp_drawtext(display, x_pos2 - text4_width / 2, TOUCH_LCD_HEIGHT / 2 + 80 - noto11_font->line_height, noto11_font, text4);
    qp_drawtext(display, x_pos2 - text5_width / 2, y_pos3 - noto11_font->line_height / 2, noto11_font, text5);
    qp_drawtext(display, x_pos2 - text6_width / 2, y_pos4 - noto11_font->line_height / 2, noto11_font, text6);
    qp_curve(display, speed_adjust1, slope_factor1, hue_tbtune_r, sat_tbtune_r, val_tbtune_r, x_pos1, y_pos1);
    qp_curve(display, speed_adjust2, slope_factor2, hue_tbtune_l, sat_tbtune_l, val_tbtune_l, x_pos2, y_pos2);
    qp_drawimage(display, x_pos_save - image_save->width / 2, y_pos_save - image_save->height / 2, image_save);
    qp_flush(display);
}

void display_redraw(void) {
    switch (display_mode) {
        case DISPLAY_MODE_TOUCH_KEY:
            draw_background_all_black();
            draw_lcd_layer_category_images();
            break;
        case DISPLAY_MODE_TRACKBALL_TUNING:
            show_trackball_tuning_mode();
            break;
        case DISPLAY_MODE_SWIPE_GESTURE:
            swipe_gesture_swipe_omni_logo();

            break;
        case DISPLAY_MODE_KEY_MATRIX:
            draw_key_matrix(display, roboto_mono16, st2_mono16, current_layer);
            break;
        case DISPLAY_MODE_STATUS1:
            status_view_init(display, noto9_font);
            break;
        default:
            break;
    }
}

void process_touch_trackball_tuning_mode(uint16_t touch_x, uint16_t touch_y) {
    const uint8_t radius = 22;
    const uint8_t radius_gap = 1;
    point_t plus1_center = {x_pos1 - (radius + radius_gap), y_pos4}; 
    point_t minus1_center = {x_pos1 - (radius + radius_gap), y_pos3};
    point_t plus2_center = {x_pos2 - (radius + radius_gap), y_pos4}; 
    point_t minus2_center = {x_pos2 - (radius + radius_gap), y_pos3};
    point_t plus3_center = {x_pos1 + (radius + radius_gap), y_pos3}; 
    point_t minus3_center = {x_pos1 + (radius + radius_gap), y_pos4};
    point_t plus4_center = {x_pos2 + (radius + radius_gap), y_pos3}; 
    point_t minus4_center = {x_pos2 + (radius + radius_gap), y_pos4};
    point_t save_center = {x_pos_save, y_pos_save + 5};
    if (is_touch_in_circle(touch_x, touch_y, plus1_center, radius)) {
        speed_adjust1 += 0.2;
        if (speed_adjust1 > 3.0f) speed_adjust1 = 3.0f;
    } else if (is_touch_in_circle(touch_x, touch_y, minus1_center, radius)) {
        speed_adjust1 -= 0.2;
        if (speed_adjust1 < 0.2f) speed_adjust1 =  0.2f;
    } else if (is_touch_in_circle(touch_x, touch_y, plus2_center, radius)) {
        speed_adjust2 += 0.2;
        if (speed_adjust2 > 3.0f) speed_adjust2 = 3.0f;
    } else if (is_touch_in_circle(touch_x, touch_y, minus2_center, radius)) {
        speed_adjust2 -= 0.2;
        if (speed_adjust2 < 0.2f) speed_adjust2 = 0.2f;
    } else if (is_touch_in_circle(touch_x, touch_y, plus3_center, radius)) {
        slope_factor1 += 5;
        if (slope_factor1 > 100) slope_factor1 = 100;
    } else if (is_touch_in_circle(touch_x, touch_y, minus3_center, radius)) {
        slope_factor1 -= 5;
        if (slope_factor1 < 10) slope_factor1 = 10;
    } else if (is_touch_in_circle(touch_x, touch_y, plus4_center, radius)) {
        slope_factor2 += 5;
        if (slope_factor2 > 100) slope_factor2 = 100;
    } else if (is_touch_in_circle(touch_x, touch_y, minus4_center, radius)) {
        slope_factor2 -= 5;
        if (slope_factor2 < 10) slope_factor2 = 10;
    } else if (is_touch_in_circle(touch_x, touch_y, save_center, radius)) {
        save_omni_tb_config();
        show_trackball_tuning_mode();
    }
    if (pre_speed_adjust1 != speed_adjust1 || pre_slope_factor1 != slope_factor1) {
        draw_background_black(TOUCH_LCD_WIDTH / 2 + 15, TOUCH_LCD_HEIGHT / 2 - 40, TOUCH_LCD_WIDTH / 2 + 85, TOUCH_LCD_HEIGHT / 2 + 25);

        wait_ms(10);
        qp_curve(display, speed_adjust1, slope_factor1, hue_tbtune_r, sat_tbtune_r, val_tbtune_r, x_pos1, y_pos1);
    } else if (pre_speed_adjust2 != speed_adjust2 || pre_slope_factor2 != slope_factor2) {
        draw_background_black(TOUCH_LCD_WIDTH / 2 - 85, TOUCH_LCD_HEIGHT / 2 - 40, TOUCH_LCD_WIDTH / 2 - 15, TOUCH_LCD_HEIGHT / 2 + 25);

        wait_ms(10);
        qp_curve(display, speed_adjust2, slope_factor2, hue_tbtune_l, sat_tbtune_l, val_tbtune_l, x_pos2, y_pos2);
    }
    pre_speed_adjust1 = speed_adjust1;
    pre_speed_adjust2 = speed_adjust2;
    pre_slope_factor1 = slope_factor1;
    pre_slope_factor2 = slope_factor2;
}
