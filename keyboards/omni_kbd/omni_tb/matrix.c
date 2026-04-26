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
#include "omni_tb.h"
#include <math.h>
#include "config.h"
#include "timer.h"

static const pin_t row_pins[MATRIX_ROWS] = MATRIX_ROW_PINS;
static const pin_t col_pins[MATRIX_COLS] = MATRIX_COL_PINS;

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
    // initialize key pins
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
    uint8_t touched_index = check_touch_within_radius(touch_x, touch_y, circles, 6, 30);
    if (touched_index != 255) {
        if (touch_x <= TOUCH_LCD_WIDTH && touch_y <= TOUCH_LCD_HEIGHT) {
            *row = (uint8_t)current_lcd_category + current_lcd_layer * (MAX_LCD_CATEGORY + 1) + MATRIX_ROWS + 4 ;
            *col = (uint8_t)touched_index;
            return true;
        }
    }
    *row = 0xFF;
    *col = 0xFF;
    return false;
}

bool read_touch(matrix_row_t current_matrix[], bool call_to_row) {
    bool matrix_changed = false;
    uint8_t row_index = 0, col_index = 0;
    if (get_touch_coordinates(&row_index, &col_index, touch_x, touch_y)) {
        select_col(col_index);
        matrix_io_delay();
        matrix_row_t last_row_value = current_matrix[row_index];
        if (touch_signal) {
            current_matrix[row_index] |= (MATRIX_ROW_SHIFTER << col_index);
        } else {
            current_matrix[row_index] &= ~(MATRIX_ROW_SHIFTER << col_index);
        }
        if ((last_row_value != current_matrix[row_index]) && !(matrix_changed)) {
            matrix_changed = true;
        }
        unselect_col(col_index);
        }
    return matrix_changed;
}


extern matrix_row_t matrix[MATRIX_ROWS];

static uint16_t last_touch_time = 0;
static bool last_matrix_state = false;

bool matrix_scan_custom(matrix_row_t current_matrix[])
{
    bool changed = false;
    // Set row, read cols
    for (uint8_t current_row = 0; current_row < MATRIX_ROWS / 2; current_row++) {
    changed |= read_cols_on_row(current_matrix, current_row);
    }
    //else
    // Set col, read rows
    for (uint8_t current_col = 0; current_col < MATRIX_COLS; current_col++) {
    changed |= read_rows_on_col(current_matrix, current_col);
    }

    static bool touch_signal_latch = false;
    if (touch_signal) {
        touch_signal_latch = true;
    }
    

    switch (display_mode) {
        case DISPLAY_MODE_TOUCH_KEY:
            if (!touch_signal_latch) {
                touch_x = 0xFFFF;
                touch_y = 0xFFFF;
            }
            if (timer_elapsed(last_touch_time) > touch_repeat_interval) {
                if (read_touch(current_matrix, true)) {
                    changed = true;
                    last_touch_time = timer_read();  
                    touch_signal_latch = false;
                }
            }
            break;

        case DISPLAY_MODE_TRACKBALL_TUNING:
            break;

        case DISPLAY_MODE_SWIPE_GESTURE:
            break;
            
        default:
            break;
    }

  last_matrix_state = changed;
  return changed;
}

bool get_last_matrix_state(void) {
    return last_matrix_state;
}
