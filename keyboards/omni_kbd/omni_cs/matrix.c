// Copyright 2025 mass
// Copyright 2012-2018 Jun Wako, Jack Humbert, Yiancar
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "util.h"
#include "matrix.h"
#include "debounce.h"
#include "atomic_util.h"
#include "omni_cs.h"
#include "../common/touch_lcd_omni.h"
#include <math.h>
#include "config.h"
#include "timer.h"

static const pin_t row_pins[MATRIX_ROWS] = MATRIX_ROW_PINS;
static const pin_t col_pins[MATRIX_COLS] = MATRIX_COL_PINS;

// add
#define TOUCH_MATRIX_HOLD_MS 20
static bool     touch_matrix_active = false;
static bool     touch_matrix_pressed = false;
static uint8_t  touch_matrix_row = 0xFF;
static uint8_t  touch_matrix_col = 0xFF;
static uint16_t touch_matrix_timer = 0;
// add fin


static void select_row(uint8_t row)
{
    if (row_pins[row] == NO_PIN) return;
    setPinOutput(row_pins[row]);
    writePinLow(row_pins[row]);
}

static void unselect_row(uint8_t row)
{
    if (row_pins[row] == NO_PIN) return;
    setPinInputHigh(row_pins[row]);
}

static void unselect_rows(void)
{
    for(uint8_t x = 0; x < MATRIX_ROWS; x++) {
        if (row_pins[x] == NO_PIN) continue;
        setPinInputHigh(row_pins[x]);
    }
}

static void select_col(uint8_t col)
{
    if (col_pins[col] == NO_PIN) return;
    setPinOutput(col_pins[col]);
    writePinLow(col_pins[col]);
}

static void unselect_col(uint8_t col)
{
    if (col_pins[col] == NO_PIN) return;
    setPinInputHigh(col_pins[col]);
}

static void unselect_cols(void)
{
    for(uint8_t x = 0; x < MATRIX_COLS; x++) {
        if (col_pins[x] == NO_PIN) continue;
        setPinInputHigh(col_pins[x]);
    }
}

static void init_pins(void) {
  unselect_rows();
  unselect_cols();
  for (uint8_t x = 0; x < MATRIX_COLS; x++) {
    if (col_pins[x] == NO_PIN) continue;
    setPinInputHigh(col_pins[x]);
  }
  for (uint8_t x = 0; x < MATRIX_ROWS; x++) {
    if (row_pins[x] == NO_PIN) continue;
    setPinInputHigh(row_pins[x]);
  }
}

static bool read_cols_on_row(matrix_row_t current_matrix[], uint8_t current_row) {

    if (row_pins[current_row] == NO_PIN) {
        matrix_row_t last_row_value = current_matrix[current_row];
        current_matrix[current_row] = 0;
        return (last_row_value != current_matrix[current_row]);
    }

    // Store last value of row prior to reading
    matrix_row_t last_row_value = current_matrix[current_row];

    // Clear data in matrix row
    current_matrix[current_row] = 0;

    // Select row and wait for row selecton to stabilize
    select_row(current_row);
    matrix_io_delay();

    // For each col...
    for(uint8_t col_index = 0; col_index < MATRIX_COLS; col_index++) {
        if (col_pins[col_index] == NO_PIN) continue; 

        // Select the col pin to read (active low)
        uint8_t pin_state = readPin(col_pins[col_index]);

        // Populate the matrix row with the state of the col pin
        current_matrix[current_row] |=  pin_state ? 0 : (MATRIX_ROW_SHIFTER << col_index);
    }

    // Unselect row
    unselect_row(current_row);

    return (last_row_value != current_matrix[current_row]);
}

static bool read_rows_on_col(matrix_row_t current_matrix[], uint8_t current_col)
{
    if (col_pins[current_col] == NO_PIN) {
        return false;    // この列自体スキャンしない
    }

    bool matrix_changed = false;

    // Select col and wait for col selecton to stabilize
    select_col(current_col);
    matrix_io_delay();

    // For each row...
    for(uint8_t row_index = 0; row_index < MATRIX_ROWS/2; row_index++) {
        if (row_pins[row_index] == NO_PIN) continue;

        uint8_t tmp = row_index + MATRIX_ROWS/2;
        // Store last value of row prior to reading
        matrix_row_t last_row_value = current_matrix[tmp];

        // Check row pin state
        if (readPin(row_pins[row_index]) == 0)
        {
            // Pin LO, set col bit
            current_matrix[tmp] |= (MATRIX_ROW_SHIFTER << current_col);
        }
        else
        {
            // Pin HI, clear col bit
            current_matrix[tmp] &= ~(MATRIX_ROW_SHIFTER << current_col);
        }

        // Determine if the matrix changed state
        if ((last_row_value != current_matrix[tmp]) && !(matrix_changed))
        {
            matrix_changed = true;
        }
    }

    // Unselect col
    unselect_col(current_col);

    return matrix_changed;
}

void matrix_init_custom(void) {
    init_pins();
}

int check_touch_within_radius(uint16_t touch_x, uint16_t touch_y, point_t circles[], size_t num_circles, int radius) {
    for (size_t i = 0; i < num_circles; i++) {
        int16_t dx = touch_x - circles[i].x;
        int16_t dy = touch_y - circles[i].y;
        int distance = sqrt(dx * dx + dy * dy);
        if (distance <= radius) {
            // uprintf("circle index %d\n", i);
            return i;
        }
    }
    return 255; 
}

bool get_touch_coordinates(uint8_t *row, uint8_t *col, uint16_t touch_x, uint16_t touch_y) {
    if (touch_x > TOUCH_LCD_WIDTH || touch_y > TOUCH_LCD_HEIGHT) {
        *row = 0xFF;
        *col = 0xFF;
        return false;
    }

    uint8_t touched_index = check_touch_within_radius(touch_x, touch_y, circles, 6, 30);

    if (touched_index != 255) {
        *row = (uint8_t)current_lcd_layer + 6;
        *col = (uint8_t)touched_index;

        uprintf("[TOUCH HIT] x=%u y=%u signal=%u layer=%u row=%u col=%u\n",
                touch_x, touch_y, touch_signal, current_lcd_layer, *row, *col);
        return true;
    }

    *row = 0xFF;
    *col = 0xFF;
    return false;
}

bool read_touch(matrix_row_t current_matrix[], bool touch_pressed) {
    bool matrix_changed = false;
    uint8_t row_index = 0, col_index = 0;

    if (get_touch_coordinates(&row_index, &col_index, touch_x, touch_y)) {
        matrix_row_t last_row_value = current_matrix[row_index];

        if (touch_pressed) {
            current_matrix[row_index] |= (MATRIX_ROW_SHIFTER << col_index);
        } else {
            current_matrix[row_index] &= ~(MATRIX_ROW_SHIFTER << col_index);
        }

        // uprintf("[READ_TOUCH] pressed=%u row=%u col=%u before=0x%04X after=0x%04X changed=%u\n",
        //         touch_pressed,
        //         row_index,
        //         col_index,
        //         last_row_value,
        //         current_matrix[row_index],
        //         last_row_value != current_matrix[row_index]);

        if (last_row_value != current_matrix[row_index]) {
            matrix_changed = true;
        }
    }

    return matrix_changed;
}


extern matrix_row_t matrix[MATRIX_ROWS];

static bool last_matrix_state = false;

static void update_touch_matrix_state(void) {
    uint8_t row = 0xFF;
    uint8_t col = 0xFF;

    if (touch_signal && get_touch_coordinates(&row, &col, touch_x, touch_y)) {
        touch_matrix_row = row;
        touch_matrix_col = col;
        touch_matrix_active = true;
        touch_matrix_pressed = true;
        touch_matrix_timer = timer_read();
        return;
    }

    if (touch_matrix_pressed && timer_elapsed(touch_matrix_timer) > TOUCH_MATRIX_HOLD_MS) {
        touch_matrix_pressed = false;
    }
}

static void apply_touch_matrix(matrix_row_t current_matrix[]) {
    if (!touch_matrix_active || touch_matrix_row == 0xFF || touch_matrix_col == 0xFF) {
        return;
    }

    if (touch_matrix_pressed) {
        current_matrix[touch_matrix_row] |= (MATRIX_ROW_SHIFTER << touch_matrix_col);
        // 暫定処理、PC側の1~2回目の入力遅延相当をあとでいれる
        touch_signal_view_update = true;
    } else {
        current_matrix[touch_matrix_row] &= ~(MATRIX_ROW_SHIFTER << touch_matrix_col);
    }

    uprintf("[TOUCH_APPLY] pressed=%u row=%u col=%u result=0x%04X\n",
            touch_matrix_pressed,
            touch_matrix_row,
            touch_matrix_col,
            current_matrix[touch_matrix_row]);

    if (!touch_matrix_pressed) {
        touch_matrix_active = false;
        touch_matrix_row = 0xFF;
        touch_matrix_col = 0xFF;
        touch_x = 0xFFFF;
        touch_y = 0xFFFF;
    }
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    matrix_row_t next_matrix[MATRIX_ROWS] = {0};

    for (uint8_t current_row = 0; current_row < MATRIX_ROWS / 2; current_row++) {
        read_cols_on_row(next_matrix, current_row);
    }

    for (uint8_t current_col = 0; current_col < MATRIX_COLS; current_col++) {
        read_rows_on_col(next_matrix, current_col);
    }

    switch (display_mode) {
        case DISPLAY_MODE_TOUCH_KEY:
            update_touch_matrix_state();
            apply_touch_matrix(next_matrix);
            break;

        case DISPLAY_MODE_TRACKBALL_TUNING:
            break;

        case DISPLAY_MODE_SWIPE_GESTURE:
            break;

        default:
            break;
    }

    bool changed = memcmp(current_matrix, next_matrix, sizeof(next_matrix)) != 0;

    if (changed) {
        memcpy(current_matrix, next_matrix, sizeof(next_matrix));
    }

    last_matrix_state = changed;
    return changed;
}



bool get_last_matrix_state(void) {
    return last_matrix_state;
}
