// Copyright 2025 mass
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdint.h>
#include <string.h>
#include <print.h>
#include "wait.h"
#include "timer.h"
#include "i2c_master.h"
#include "gpio.h"
#include "qp.h"
#include "matrix.h"
#include "config.h"
#include "dynamic_keymap.h"
#include "omni_cs.h"
#include "os_detection.h"

#include "../common/trackball_omni.h"
#include "../common/config_omni.h"
#include "../common/sleeping_view.h"
#include "../common/draw_custom.h"
#include "../common/view_keymap.h"
#include "../common/power_lcd.h"
#include "../common/status_view.h"
#include "../drivers/pmw33xx_common.h"
#include "../font/noto9.qff.h"
#include "../font/noto11.qff.h"
#include "../font/roboto_mono16.qff.h"
#include "../font/st2_mono16.qff.h"
#include "../icon/omni_image_loader.h"
#include "../common/omni_bg_image.h"
#include "../common/omni_icon_image.h"

bool matrix_changed = false;
static bool tb_state = false;
trackball_mode_t tb_mode_r = TRACKBALL_CURSOR;
trackball_mode_t tb_mode_l = TRACKBALL_TAP;
bool is_first_frame = true;  
bool is_second_frame = true;  
static deferred_token my_anim;
static uint8_t pre_layer = 0; 
uint32_t lcd_current_time = 0;
uint32_t lcd_fast_res_time = 0;
point_t home_point = {TOUCH_LCD_WIDTH / 2, TOUCH_LCD_HEIGHT / 2};
point_t circles[] = {{200, 120}, {160, 189}, {80, 189}, {40, 120}, {80, 51}, {160, 51}};
uint16_t sleeping_timer;
bool sleeping_state = false;
uint16_t draw_matrix_code_rain_timer = 0;
bool fast_draw_matrix_code_rain = false;
uint16_t virtual_keycode[KEYCODE_SIZE];
uint16_t import_keymaps[MATRIX_ROWS / 2][MATRIX_COLS];
static uint16_t blink_start_time = 0;
static bool is_backlight_off = false;
bool lcd_is_on = true;
static uint16_t display_angle = 0;

#define TOUCH_DRAW_LAYER      0
#define TOUCH_DRAW_ROW_START  10
#define TOUCH_DRAW_ROW_COUNT  4
#define TOUCH_DRAW_COL_COUNT  6

#define TOUCH_CENTER_DRAW_ROW         14
#define TOUCH_CENTER_DRAW_COL_COUNT   4
#define TOUCH_CENTER_ICON_SLOT_START  24
#define TOUCH_CUSTOM_ICON_SLOT_COUNT  28

void initialize_lcd_layer_app_images(void) {
    for (int page = 0; page < 4; page++) {
        uint8_t center_slot = TOUCH_CENTER_ICON_SLOT_START + page;

        uint16_t center_keycode = dynamic_keymap_get_keycode(
            TOUCH_DRAW_LAYER,
            TOUCH_CENTER_DRAW_ROW,
            page
        );

        lcd_layer_app_images[page][0] = (ImagePosition){
            get_center_img_func_for_slot(center_keycode, center_slot, page),
            home_point.x,
            home_point.y
        };

        for (int k = 0; k < 6; k++) {
            uint8_t slot = k + page * 6;
            uint16_t keycode = virtual_keycode[slot];

            lcd_layer_app_images[page][k + 1] = (ImagePosition){
                get_img_func_for_slot(keycode, slot),
                circles[k].x,
                circles[k].y
            };
        }
    }
}

void load_virtual_keys(void) {
    int i = 0;
    for (int row = TOUCH_DRAW_ROW_START; row < TOUCH_DRAW_ROW_START + TOUCH_DRAW_ROW_COUNT; row++) {
        for (int col = 0; col < TOUCH_DRAW_COL_COUNT; col++) {
            virtual_keycode[i] = dynamic_keymap_get_keycode(TOUCH_DRAW_LAYER, row, col);
            i++;
        }
    }
}

void update_lcd_view_data(void){
    load_virtual_keys();
    draw_background_all_black();
    initialize_lcd_layer_app_images();
    draw_lcd_layer_category_images();
}

void omni_icon_on_changed(uint8_t slot) {
    if (slot >= TOUCH_CUSTOM_ICON_SLOT_COUNT) {
        return;
    }

    if (display_mode == DISPLAY_MODE_TOUCH_KEY) {
        update_lcd_view_data();
    }
}

bool get_haptic_enabled_key(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

void pointing_device_init_kb(void) {
    pmw33xx_init(0);         
    pmw33xx_init(1);         
    pmw33xx_set_cpi(0, 3000);
    pmw33xx_set_cpi(1, 3000);
    pointing_device_init_user();
}

void matrix_init_user(void) {
    setPinOutput(BLK_PIN);
    writePinHigh(BLK_PIN);
    i2c_init();
    setPinInputHigh(INT_PIN); 
    initialize_lcd_layer_app_images();
    load_virtual_keys();
}

void keyboard_post_init_kb(void) {
    if (!eeconfig_is_enabled()) {
        eeconfig_init();
    }
    load_omni_tb_config(); 
    load_omni_color_config();
    persist_load_all();
    display = qp_gc9a01_make_spi_device(TOUCH_LCD_WIDTH, TOUCH_LCD_HEIGHT, CS_PIN, DC_PIN, RST_PIN, 4, 0); // パネル幅, パネル高さ,,,,SPIディバイザ,SPIモード
    qp_init(display, QP_ROTATION_0);
    omni_bg_set_display(display);
    power_lcd_init(display, BLK_PIN);
    noto9_font = qp_load_font_mem(font_noto9); 
    noto11_font = qp_load_font_mem(font_noto11); 
    roboto_mono16 = qp_load_font_mem(font_roboto_mono16); 
    st2_mono16 = qp_load_font_mem(font_st2_mono16); 
    uint8_t init_status = cst816t_init(2);
    if (init_status == 0) {
        uprintf("Touch initialization failed.\n");
    } else {
        uprintf("Touch initialization successful.\n");
    }
    draw_background_all_black();
    initialize_images();
    qp_flush(display);
    if (image_logo != NULL) {
        int x = (TOUCH_LCD_WIDTH - image_logo->width) / 2;
        int y = (TOUCH_LCD_HEIGHT - image_logo->height) / 2;
        my_anim = qp_animate(display, x, y, image_logo);
        lcd_fast_res_time = timer_read();
    }

    wait_ms(300); 
    sync_default_layer_to_os();
    tb_gesture_config_force_update();
}

report_mouse_t pointing_device_task_kb(report_mouse_t mouse_report) {
    current_layer = get_highest_layer(layer_state);
    
    if (display_mode ==  DISPLAY_MODE_KEY_MATRIX) {
        if (!get_auto_mouse_enable()) {
            if(current_layer != pre_layer){
                draw_key_matrix(display, roboto_mono16, st2_mono16, current_layer);
            }            
        } else {
            if (current_layer != 3){
                if(pre_layer !=  3){
                    if(current_layer != pre_layer){
                        draw_key_matrix(display, roboto_mono16, st2_mono16, current_layer);
                    }
                }
            }
        }
    }
    pre_layer = current_layer;

    pmw33xx_report_t report0 = pmw33xx_read_burst(0);
    pmw33xx_report_t report1 = pmw33xx_read_burst(1);

    if (report0.motion.b.is_motion || report1.motion.b.is_motion) {
        tb_state = true;
    }else {
        tb_state = false;
    }

    tb_gesture_config_update_if_needed();

    if (tb_main_gesture_is_enabled()) {
        process_tb_gesture_report( &mouse_report, report0, speed_adjust2, slope_factor2, 1, -1, 3, ORIENT_0, tb_main_tap_keypos());
    } else {
        process_cursor_report( &mouse_report, report0, speed_adjust1, slope_factor1, -1, 1, 2, ORIENT_0);
    }

    if (tb_sub_gesture_is_enabled()) {
        process_tb_gesture_report( &mouse_report, report1, speed_adjust2, slope_factor2, -1, 1, 3, ORIENT_0, tb_sub_tap_keypos());
    } else {
        process_cursor_report( &mouse_report, report1, speed_adjust1, slope_factor1, 1, -1, 2, ORIENT_0);
    }

    if (ENABLE_TOUCH_UPDATE == 1) {
        if (touch_signal_view_update) {
            if (!is_backlight_off) {
                writePinLow(BLK_PIN);
                blink_start_time = timer_read();
                is_backlight_off = true;
            }
            if (is_backlight_off && timer_elapsed(blink_start_time) >= 50) {
                writePinHigh(BLK_PIN);
                is_backlight_off = false;
                touch_signal_view_update = false;
            }
        }
    }
    if (!is_first_frame) {
        if (is_second_frame) {
            display_redraw(display_angle);
            qp_flush(display);
            is_second_frame = false;
        }
    }
    return pointing_device_task_user(mouse_report);
}

void matrix_scan_user(void) {
    process_touch_interrupt();
}

static uint16_t sleep_view_switch_timer = 0;
static bool sleep_view_show_picture = true;

void sleeping_kb(bool matrix_changed) {
    if (matrix_changed || tb_state || touch_start_flag){ 
        sleeping_timer = timer_read();
        if (sleeping_state) {
            if (!lcd_is_on){
                lcd_is_on = power_on_lcd();
            }
            // if (swipe_view_state == 1) {
            //     omni_bg_draw_now();
            // } else {
                display_redraw(display_angle);
            // }
            sleeping_state = false;
        }
    }
    if (!sleeping_state) {
        if (timer_elapsed(sleeping_timer) >= SLEEPING_KB_TIME) {
            lcd_is_on = power_off_lcd();
            sleeping_state = true;

            sleep_view_switch_timer = timer_read();
            sleep_view_show_picture = true;
            fast_draw_matrix_code_rain = false;
        }
    } else if (sleep_view_state == 0) {
        return;

    } else if(sleep_view_state == 1) {
        if (timer_elapsed(draw_matrix_code_rain_timer) > 50) {
            draw_matrix_code_rain_timer = timer_read();
            if (!lcd_is_on){
                lcd_is_on = power_on_lcd();
            }
            if (!fast_draw_matrix_code_rain) {
                init_matrix_code_rain();
                fast_draw_matrix_code_rain = true;
            }
            update_matrix_code_rain();
            draw_matrix_code_rain(display, noto11_font);
        }

    } else if (sleep_view_state == 2) {
        if (!lcd_is_on){
            lcd_is_on = power_on_lcd();
        }
        omni_bg_draw_now();
        
    } else if (sleep_view_state == 3) {
        if (timer_elapsed(sleep_view_switch_timer) >= SLEEP_VIEW_SWITCH_TIME) {
            sleep_view_switch_timer = timer_read();
            sleep_view_show_picture = !sleep_view_show_picture;

            if (!sleep_view_show_picture) {
                fast_draw_matrix_code_rain = false;
            }
        }

        if (sleep_view_show_picture) {
            if (!lcd_is_on){
                lcd_is_on = power_on_lcd();
            }
            omni_bg_draw_now();

        } else {
            if (timer_elapsed(draw_matrix_code_rain_timer) > 50) {
                draw_matrix_code_rain_timer = timer_read();

                if (!lcd_is_on) {
                    lcd_is_on = power_on_lcd();
                }

                if (!fast_draw_matrix_code_rain) {
                    init_matrix_code_rain();
                    fast_draw_matrix_code_rain = true;
                }

                update_matrix_code_rain();
                draw_matrix_code_rain(display, noto11_font);
            }
        }
    }
}

uint8_t draw_counter = 0;
uint16_t draw_stress_timer = 0;
uint16_t draw_test_time = 3000;

void draw_test(void) {
    if (timer_elapsed(draw_stress_timer) < draw_test_time) {
        return;
    }
    draw_stress_timer = timer_read();
    uprintf("DRAW TEST: mode=%u start\n", draw_counter);

    switch (draw_counter) {
        case 0:
            draw_background_all_black();
            init_matrix_code_rain();
            update_matrix_code_rain();
            draw_matrix_code_rain(display, noto11_font);
            qp_flush(display);
            break;

        case 1:
            omni_bg_draw_now();
            break;

        case 2:            
            draw_lcd_layer_category_images();
            display_redraw(display_angle);
            break;

        case 3:
            omni_bg_draw_now();
            break;
    }

    uprintf("DRAW TEST: mode=%u end\n", draw_counter);

    draw_counter++;

    if (draw_counter > 3) {
        draw_counter = 0;
    }
}

void housekeeping_task_user(void) {
    lcd_current_time = timer_read();
    if (lcd_current_time - lcd_fast_res_time > 3000) {
        if (is_first_frame) {
            is_first_frame = false;
            qp_stop_animation(my_anim);
            draw_background_all_black();
            initialize_lcd_layer_app_images();
        }
    }
    matrix_changed = get_last_matrix_state();
    sleeping_kb(matrix_changed);

    if (touch_start_flag) {
        if (timer_elapsed(touch_start_timer) >= TOUCH_DEBOUNCE_TIME) {
            touch_start_flag = false;
            initial_touch_flag = false;
            gesture_id = GESTURE_NONE;
        }   
    }
    tb_tap_pending_task();
    omni_icon_task();
    omni_bg_task();
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) {
        tb_mode_l = TRACKBALL_TAP;
        tb_mode_r = TRACKBALL_CURSOR;
        return true;
    }
    switch (keycode) {
        case KC_hue_bg_UP:
            hue_bg = (hue_bg + 16) % 256;
            break;
        case KC_hue_bg_DOWN:
            hue_bg = (hue_bg >= 16) ? (hue_bg - 16) : (hue_bg + 240); 
            break;
        case KC_sat_bg_UP:
            sat_bg = (sat_bg + 16 <= 254) ? sat_bg + 16 : 254;
            break;
        case KC_sat_bg_DOWN:
            sat_bg = (sat_bg > 16) ? sat_bg - 16 : 0;
            break;
        case KC_val_bg_UP:
            val_bg = (val_bg + 16 <= 254) ? val_bg + 16 : 254;
            break;
        case KC_val_bg_DOWN:
            val_bg = (val_bg > 16) ? val_bg - 16 : 0;
            break;
        case KC_hue_main_color_UP:
            hue_main_color = (hue_main_color + 16) % 256;
            break;
        case KC_hue_main_color_DOWN:
            hue_main_color = (hue_main_color >= 16) ? (hue_main_color - 16) : (hue_main_color + 240);
            break;
        case KC_sat_main_color_UP:
            sat_main_color = (sat_main_color + 16 <= 254) ? sat_main_color + 16 : 254;
            break;
        case KC_sat_main_color_DOWN:
            sat_main_color = (sat_main_color > 16) ? sat_main_color - 16 : 0;
            break;
        case KC_val_main_color_UP:
            val_main_color = (val_main_color + 16 <= 254) ? val_main_color + 16 : 254;
            break;
        case KC_val_main_color_DOWN:
            val_main_color = (val_main_color > 16) ? val_main_color - 16 : 0;
            break;
        case KC_hue_sub_color_UP:
            hue_sub_color = (hue_sub_color + 16) % 256;
            break;
        case KC_hue_sub_color_DOWN:
            hue_sub_color = (hue_sub_color >= 16) ? (hue_sub_color - 16) : (hue_sub_color + 240);
            break;
        case KC_sat_sub_color_UP:
            sat_sub_color = (sat_sub_color + 16 <= 254) ? sat_sub_color + 16 : 254;
            break;
        case KC_sat_sub_color_DOWN:
            sat_sub_color = (sat_sub_color > 16) ? sat_sub_color - 16 : 0;
            break;
        case KC_val_sub_color_UP:
            val_sub_color = (val_sub_color + 16 <= 254) ? val_sub_color + 16 : 254;
            break;
        case KC_val_sub_color_DOWN:
            val_sub_color = (val_sub_color > 16) ? val_sub_color - 16 : 0;
            break;
        case TB_R_MODE_TOGGLE:
            if (record->event.pressed) {
                tb_mode_r = TRACKBALL_TAP;
            } else {
                tb_mode_r = TRACKBALL_CURSOR;
            }
            return false;
        case TB_L_MODE_TOGGLE:
            tb_mode_l = TRACKBALL_CURSOR;
            return false;
        case KC_DP_TOUCH_KEY0:
            display_mode =  DISPLAY_MODE_TOUCH_KEY;
            current_lcd_layer = 0;
            draw_background_all_black();
            draw_lcd_layer_category_images();
            break;
        case KC_DP_TOUCH_KEY1:
            display_mode =  DISPLAY_MODE_TOUCH_KEY;
            current_lcd_layer = 1;
            draw_background_all_black();
            draw_lcd_layer_category_images();
            break;
        case KC_DP_TOUCH_KEY2:
            display_mode =  DISPLAY_MODE_TOUCH_KEY;
            current_lcd_layer = 2;
            draw_background_all_black();
            draw_lcd_layer_category_images();
            break;
        case KC_DP_TOUCH_KEY3:
            display_mode =  DISPLAY_MODE_TOUCH_KEY;
            current_lcd_layer = 3;
            draw_background_all_black();
            draw_lcd_layer_category_images();
            break;
        case KC_DP_TB_TUNE: 
            display_mode =  DISPLAY_MODE_TRACKBALL_TUNING;
            show_trackball_tuning_mode();
            break;
        case KC_DP_SWIPE_GESTURE:
            display_mode =  DISPLAY_MODE_SWIPE_GESTURE;
            if (swipe_view_state == 1) {
                omni_bg_draw_now();
            } else {
                display_redraw(display_angle);
            }
            break;
        case KC_DP_KEY_MAT:
            display_mode =  DISPLAY_MODE_KEY_MATRIX;
            draw_key_matrix(display, roboto_mono16, st2_mono16, current_layer);
            break;
        case KC_DP_STAT1:
            display_mode =  DISPLAY_MODE_STATUS1;
            status_view_init(display, noto9_font);
            break;

        default:
            return true;
    }
    if(display_mode == DISPLAY_MODE_SWIPE_GESTURE) {
        save_omni_color_config();
    }
    return false;
}

void suspend_power_down_user(void){
    lcd_is_on = power_off_lcd();
    sleeping_state = true;
};

void __real_dynamic_keymap_set_keycode(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode);

void __wrap_dynamic_keymap_set_keycode(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode) {
    __real_dynamic_keymap_set_keycode(layer, row, col, keycode);
    if (keymap_change_update_flag){
        load_virtual_keys();
        initialize_lcd_layer_app_images();
        load_omni_tb_config();
        load_omni_color_config();
        persist_load_all();
        display_mode = DISPLAY_MODE_TOUCH_KEY;
        display_redraw(display_angle);
    }
    tb_gesture_config_force_update();
}



