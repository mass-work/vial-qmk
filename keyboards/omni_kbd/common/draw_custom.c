// Copyright 2025 mass
// SPDX-License-Identifier: GPL-2.0-or-later
#include "draw_custom.h"
#include "config.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include "qp_internal.h"
#include "qp_comms.h"
#include "qp_draw.h"
#include "qgf.h"
#include "qp.h"

#define DEG_TO_RAD(angle) ((angle) * M_PI / 180.0)
#define ANGLE_STEP 1
#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif


static inline int16_t constrain_hid(int16_t value) {
    if (value > MAX_HID_VALUE) return MAX_HID_VALUE;
    if (value < MIN_HID_VALUE) return MIN_HID_VALUE;
    return value;
}

bool qp_curve(painter_device_t device, float speed_adjust, int slope_factor, uint8_t hue, uint8_t sat, uint8_t val, uint8_t x_pos, uint8_t y_pos) {
    painter_driver_t *driver = (painter_driver_t *)device;
    if (!driver || !driver->validate_ok) {
        return false;
    }
    if (!qp_comms_start(device)) {
        return false;
    }
    qp_internal_fill_pixdata(device, 1, hue, sat, val);
    int center_x = x_pos;
    int center_y = y_pos;
    int draw_size = 70;
    int draw_scale_y = 110;
    int hurf_value = draw_size / 2;
    int delta_values[draw_size + 1];
    for (int i = 0; i < draw_size + 1; i++) {
        delta_values[i] = i;
    }
    bool ret = true;
    for (int i = 0; i < draw_size; i++) {
        float delta_x0 = delta_values[i];
        float delta_x1 = delta_values[i+1];
        float delta_y0 = delta_values[i];
        float delta_y1 = delta_values[i+1];
        float scaled_y0 = pow(delta_y0, speed_adjust) / pow(draw_size, speed_adjust) * draw_size / draw_scale_y * slope_factor;
        float scaled_y1 = pow(delta_y1, speed_adjust) / pow(draw_size, speed_adjust) * draw_size / draw_scale_y * slope_factor;
        int draw_x0 = (center_x - hurf_value) + (int)delta_x0;
        int draw_x1 = (center_x - hurf_value) + (int)delta_x1;
        int draw_y0 = center_y + hurf_value - (int)scaled_y0;
        int draw_y1 = center_y + hurf_value - (int)scaled_y1;
        if (draw_x0 >= 0 && draw_x0 < 240 && draw_y0 >= 0 && draw_y0 < 240 &&
            draw_x1 >= 0 && draw_x1 < 240 && draw_y1 >= 0 && draw_y1 < 240) {
            if (!qp_internal_setpixel_impl(device, draw_x0, draw_y0) ||
                !qp_internal_setpixel_impl(device, draw_x1, draw_y1)) {
                ret = false;
                break;
            }
        }
    }
    qp_comms_stop(device);
    qp_dprintf("qp_curve: %s\n", ret ? "ok" : "fail");
    return ret;
}

bool qp_fill_arc(painter_device_t device, uint16_t centerx, uint16_t centery, uint16_t outer_radius, uint16_t inner_radius, uint16_t start_angle, uint16_t end_angle, uint8_t hue, uint8_t sat, uint8_t val) {
    if (outer_radius <= inner_radius) {
        return false; 
    }
    qp_internal_fill_pixdata(device, (outer_radius * 2) + 1, hue, sat, val);
    if (!qp_comms_start(device)) {
        return false;
    }
    bool ret = true;
    double cos_table[360 / ANGLE_STEP];
    double sin_table[360 / ANGLE_STEP];
    for (uint16_t angle = start_angle; angle <= end_angle; angle += ANGLE_STEP) {
        double rad = DEG_TO_RAD(angle);
        cos_table[(angle - start_angle) / ANGLE_STEP] = cos(rad);
        sin_table[(angle - start_angle) / ANGLE_STEP] = sin(rad);
    }
    for (uint16_t angle = start_angle; angle <= end_angle; angle += ANGLE_STEP) {
        for (uint16_t r = inner_radius; r <= outer_radius; r++) {
            uint16_t index = (angle - start_angle) / ANGLE_STEP;
            int16_t x = centerx + (int16_t)(r * cos_table[index]);
            int16_t y = centery + (int16_t)(r * sin_table[index]);
            if (!qp_internal_setpixel_impl(device, x, y)) {
                ret = false;
                break;
            }
        }
        if (!ret) break;
    }
    qp_comms_stop(device);
    return ret;
}

void qp_donut(painter_device_t device, uint16_t x, uint16_t y, uint16_t radius, uint16_t thickness, uint8_t hue, uint8_t sat, uint8_t val1, uint8_t val2) {
    int add_val = (val2 - val1);
    if (add_val != 0) {
        add_val = add_val / thickness;
    }
    for (size_t i = 0; i < thickness; i++) {
        qp_circle(device, x, y, radius - i, hue, sat, val1, false);
        val1 = (val1 + add_val < 0) ? 0 : val1 + add_val;
    }
}

static uint16_t isqrt_u32(uint32_t n) {
    uint32_t res = 0, bit = 1UL << 30;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= res + bit) { n -= res + bit; res = (res >> 1) + bit; }
        else                { res >>= 1; }
        bit >>= 2;
    }
    return (uint16_t)res;
}

static bool rr_filled(painter_device_t d, uint16_t l, uint16_t t, uint16_t r, uint16_t b, uint16_t rad) {
    uint16_t h = b - t + 1;

    for (uint16_t row = 0; row < h; row++) {
        uint16_t y = t + row;
        uint16_t dx;

        if (row < rad) {
            uint16_t dy = rad - row;
            dx = rad - isqrt_u32(rad*rad - dy*dy);
        } else if (row >= h - rad) {
            uint16_t dy = row - (h - rad) + 1;
            dx = rad - isqrt_u32(rad*rad - dy*dy);
        } else {
            dx = 0;
        }

        uint16_t xl = l + dx;
        uint16_t xr = r - dx;
        if (!qp_internal_fillrect_helper_impl(d, xl, y, xr, y))
            return false;
    }
    return true;
}

static bool rr_outline(painter_device_t d, uint16_t l, uint16_t t, uint16_t r, uint16_t b, uint16_t rad) {
    uint16_t h = b - t + 1;

    for (uint16_t row = 0; row < h; row++) {
        uint16_t y  = t + row;
        uint16_t dx;

        if (row < rad) {
            uint16_t dy = rad - row;
            dx = rad - isqrt_u32(rad*rad - dy*dy);
        } else if (row >= h - rad) {
            uint16_t dy = row - (h - rad) + 1;
            dx = rad - isqrt_u32(rad*rad - dy*dy);
        } else {
            dx = 0;
        }

        uint16_t xl = l + dx;
        uint16_t xr = r - dx;

        if (row == 0 || row == h - 1) {
            if (!qp_internal_fillrect_helper_impl(d, xl, y, xr, y))
                return false;
        } else {
            if (!qp_internal_fillrect_helper_impl(d, xl, y, xl, y) ||
                !qp_internal_fillrect_helper_impl(d, xr, y, xr, y))
                return false;
        }
    }
    return true;
}

static bool rr_outline_w(painter_device_t d, uint16_t l, uint16_t t, uint16_t r, uint16_t b, uint16_t rad, uint8_t  thick) {
    uint16_t h = b - t + 1;

    for (uint16_t row = 0; row < h; row++) {
        uint16_t y = t + row;
        uint16_t dx;

        if (row < rad) {
            uint16_t dy = rad - row;
            dx = rad - isqrt_u32(rad*rad - dy*dy);
        } else if (row >= h - rad) {
            uint16_t dy = row - (h - rad) + 1;
            dx = rad - isqrt_u32(rad*rad - dy*dy);
        } else {
            dx = 0;
        }

        for (uint8_t w = 0; w < thick; w++) {
            uint16_t xl = l + dx + w;
            uint16_t xr = r - dx - w;

            if (row <= thick || row >= h - 1 - thick) {
                if (!qp_internal_fillrect_helper_impl(d, xl, y, xr, y))
                    return false;
            } else {
                if (!qp_internal_fillrect_helper_impl(d, xl, y, xl, y) ||
                    !qp_internal_fillrect_helper_impl(d, xr, y, xr, y))
                    return false;
            }
        }
    }
    return true;
}

bool qp_round_rect(painter_device_t dev, uint16_t left,  uint16_t top, uint16_t right, uint16_t bottom, uint16_t radius, uint8_t hue, uint8_t sat, uint8_t val, bool filled, uint8_t stroke_w) {
    uint16_t l = QP_MIN(left,  right);
    uint16_t r = QP_MAX(left,  right);
    uint16_t t = QP_MIN(top,   bottom);
    uint16_t b = QP_MAX(top,   bottom);
    uint16_t w = r - l + 1, h = b - t + 1;
    radius = QP_MIN(radius, QP_MIN(w, h) / 2);

    qp_internal_fill_pixdata(dev, w, hue, sat, val);

    if (!qp_comms_start(dev)) return false;

    bool ok;
    if (filled) {
        ok = rr_filled(dev, l, t, r, b, radius);
    } else if (stroke_w <= 1) {
        ok = rr_outline(dev, l, t, r, b, radius);
    } else {
        ok = rr_outline_w(dev, l, t, r, b, radius, stroke_w);
    }

    qp_comms_stop(dev);
    return ok;
}

bool qp_rotated_ellipse_filled(painter_device_t device, uint16_t centerx, uint16_t centery, uint16_t width, uint16_t height, float angle_deg, uint8_t hue, uint8_t sat, uint8_t val) {
    qp_dprintf("qp_rotated_ellipse_filled: entry\n");

    painter_driver_t *driver = (painter_driver_t *)device;
    if (!driver || !driver->validate_ok) {
        qp_dprintf("qp_rotated_ellipse_filled: fail (validation_ok == false)\n");
        return false;
    }

    if (width == 0 || height == 0) {
        qp_dprintf("qp_rotated_ellipse_filled: fail (width/height == 0)\n");
        return false;
    }

    float a = (float)width  * 0.5f;
    float b = (float)height * 0.5f;
    float rad = angle_deg * (float)M_PI / 180.0f;
    float c   = cosf(rad);
    float s   = sinf(rad);
    float a2 = a * a;
    float b2 = b * b;
    float inv_a2 = 1.0f / a2;
    float inv_b2 = 1.0f / b2;

    int16_t x_extent = (int16_t)ceilf(sqrtf(a2 * c * c + b2 * s * s));
    int16_t y_extent = (int16_t)ceilf(sqrtf(a2 * s * s + b2 * c * c));

    uint16_t max_span = (uint16_t)(x_extent * 2 + 1);
    qp_internal_fill_pixdata(device, max_span, hue, sat, val);

    if (!qp_comms_start(device)) {
        qp_dprintf("qp_rotated_ellipse_filled: fail (could not start comms)\n");
        return false;
    }

    bool ret = true;
    float A = (c * c) * inv_a2 + (s * s) * inv_b2;
    float B_base = 2.0f * c * s * (inv_a2 - inv_b2);
    float C_base = (s * s) * inv_a2 + (c * c) * inv_b2;

    for (int16_t dy = -y_extent; dy <= y_extent; dy++) {
        float fy = (float)dy;

        float B = B_base * fy;
        float C = C_base * fy * fy - 1.0f;

        float D = B * B - 4.0f * A * C;   // 判別式
        if (D < 0.0f) {
            continue;
        }

        float sqrtD = sqrtf(D);

        float dx_min_f = (-B - sqrtD) / (2.0f * A);
        float dx_max_f = (-B + sqrtD) / (2.0f * A);

        int16_t dx_min = (int16_t)ceilf(dx_min_f);
        int16_t dx_max = (int16_t)floorf(dx_max_f);

        if (dx_min > dx_max) {
            continue;
        }

        int16_t y  = (int16_t)centery + dy;
        int16_t xl = (int16_t)centerx + dx_min;
        int16_t xr = (int16_t)centerx + dx_max;

        if (!qp_internal_fillrect_helper_impl(device, xl, y, xr, y)) {
            ret = false;
            break;
        }
    }

    qp_comms_stop(device);
    qp_dprintf("qp_rotated_ellipse_filled: %s\n", ret ? "ok" : "fail");
    return ret;
}

typedef struct {
    float x;
    float y;
} qp_ptf_t;

static inline qp_ptf_t qp_rotate_translate_pt(float x, float y, float c, float s, float tx, float ty) {
    qp_ptf_t p;
    p.x = tx + x * c - y * s;
    p.y = ty + x * s + y * c;
    return p;
}

static bool qp_plot_thick_pixel(painter_device_t device, int16_t x, int16_t y, uint8_t stroke_w) {
    if (stroke_w == 0) {
        stroke_w = 1;
    }

    int8_t r = (int8_t)(stroke_w / 2);

    for (int8_t oy = -r; oy <= r; oy++) {
        for (int8_t ox = -r; ox <= r; ox++) {
            int16_t px = x + ox;
            int16_t py = y + oy;

            if (px < 0 || px >= 240 || py < 0 || py >= 240) {
                continue;
            }

            if (!qp_internal_setpixel_impl(device, px, py)) {
                return false;
            }
        }
    }

    return true;
}

static bool qp_line_thick_internal(painter_device_t device, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t stroke_w) {
    int16_t dx = abs(x1 - x0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t dy = -abs(y1 - y0);
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx + dy;

    while (true) {
        if (!qp_plot_thick_pixel(device, x0, y0, stroke_w)) {
            return false;
        }

        if (x0 == x1 && y0 == y1) {
            break;
        }

        int16_t e2 = 2 * err;

        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }

        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }

    return true;
}

static inline float qp_edge_fn(qp_ptf_t a, qp_ptf_t b, float px, float py) {
    return (px - a.x) * (b.y - a.y) - (py - a.y) * (b.x - a.x);
}

bool qp_triangle_rotated(painter_device_t device, int16_t centerx, int16_t centery, uint16_t width, uint16_t height, float angle_deg, uint8_t hue, uint8_t sat, uint8_t val, bool filled, uint8_t stroke_w) {
    painter_driver_t *driver = (painter_driver_t *)device;
    if (!driver || !driver->validate_ok) {
        return false;
    }

    if (width == 0 || height == 0) {
        return false;
    }

    if (stroke_w == 0) {
        stroke_w = 1;
    }

    // 基準形は「▽」
    // angle_deg = 0 で下向き
    float hw = (float)width * 0.5f;
    float hh = (float)height * 0.5f;

    float rad = angle_deg * (float)M_PI / 180.0f;
    float c   = cosf(rad);
    float s   = sinf(rad);

    qp_ptf_t v0 = qp_rotate_translate_pt( 0.0f,  hh, c, s, centerx, centery); // 下頂点
    qp_ptf_t v1 = qp_rotate_translate_pt(-hw,   -hh, c, s, centerx, centery); // 左上
    qp_ptf_t v2 = qp_rotate_translate_pt( hw,   -hh, c, s, centerx, centery); // 右上

    qp_internal_fill_pixdata(device, 1, hue, sat, val);

    if (!qp_comms_start(device)) {
        return false;
    }

    bool ret = true;

    if (filled) {
        float min_x_f = fminf(v0.x, fminf(v1.x, v2.x));
        float max_x_f = fmaxf(v0.x, fmaxf(v1.x, v2.x));
        float min_y_f = fminf(v0.y, fminf(v1.y, v2.y));
        float max_y_f = fmaxf(v0.y, fmaxf(v1.y, v2.y));

        int16_t min_x = (int16_t)floorf(min_x_f);
        int16_t max_x = (int16_t)ceilf(max_x_f);
        int16_t min_y = (int16_t)floorf(min_y_f);
        int16_t max_y = (int16_t)ceilf(max_y_f);

        float area = qp_edge_fn(v0, v1, v2.x, v2.y);
        bool area_positive = (area >= 0.0f);

        for (int16_t y = min_y; y <= max_y; y++) {
            for (int16_t x = min_x; x <= max_x; x++) {
                if (x < 0 || x >= 240 || y < 0 || y >= 240) {
                    continue;
                }

                float px = (float)x + 0.5f;
                float py = (float)y + 0.5f;

                float e0 = qp_edge_fn(v0, v1, px, py);
                float e1 = qp_edge_fn(v1, v2, px, py);
                float e2 = qp_edge_fn(v2, v0, px, py);

                bool inside = area_positive
                    ? (e0 >= 0.0f && e1 >= 0.0f && e2 >= 0.0f)
                    : (e0 <= 0.0f && e1 <= 0.0f && e2 <= 0.0f);

                if (inside && !qp_internal_setpixel_impl(device, x, y)) {
                    ret = false;
                    goto out;
                }
            }
        }
    } else {
        if (!qp_line_thick_internal(device,
                                    (int16_t)lroundf(v0.x), (int16_t)lroundf(v0.y),
                                    (int16_t)lroundf(v1.x), (int16_t)lroundf(v1.y),
                                    stroke_w) ||
            !qp_line_thick_internal(device,
                                    (int16_t)lroundf(v1.x), (int16_t)lroundf(v1.y),
                                    (int16_t)lroundf(v2.x), (int16_t)lroundf(v2.y),
                                    stroke_w) ||
            !qp_line_thick_internal(device,
                                    (int16_t)lroundf(v2.x), (int16_t)lroundf(v2.y),
                                    (int16_t)lroundf(v0.x), (int16_t)lroundf(v0.y),
                                    stroke_w)) {
            ret = false;
        }
    }

out:
    qp_comms_stop(device);
    return ret;
}

typedef struct {
    uint8_t width;
    uint8_t height;
    const uint32_t *rows;
} mono_glyph_t;

static const uint32_t glyph_m_rows[28] = {
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b1110000111110000000111110000,
    0b1110001111111100001111111100,
    0b1110011111111100011111111110,
    0b1110111111111110111111111111,
    0b111111111111111111111111111,
    0b1111110000111111100000111111,
    0b1111110000011111100000011111,
    0b1111100000011111000000011111,
    0b1111100000011111000000011111,
    0b1111100000011111000000011111,
    0b1111100000011111000000011111,
    0b1111100000011111000000011111,
    0b1111100000011111000000011111,
    0b1111100000011111000000011111,
    0b1111100000011111000000011111,
    0b1111100000011111000000011111,
    0b1111100000011111000000011111,
    0b1111100000011111000000011111,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
};

static const uint32_t glyph_n_rows[28] = {
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000111000011111000000000000,
    0b0000111000111111110000000000,
    0b0000111001111111111000000000,
    0b0000111011111111111100000000,
    0b0000111111111111111100000000,
    0b0000111111000011111100000000,
    0b0000111111000001111100000000,
    0b0000111110000001111100000000,
    0b0000111110000001111100000000,
    0b0000111110000001111100000000,
    0b0000111110000001111100000000,
    0b0000111110000001111100000000,
    0b0000111110000001111100000000,
    0b0000111110000001111100000000,
    0b0000111110000001111100000000,
    0b0000111110000001111100000000,
    0b0000111110000001111100000000,
    0b0000111110000001111100000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
};

static const uint32_t glyph_i_rows[28] = {
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b1111000000000000000000000000,
    0b1111000000000000000000000000,
    0b1111000000000000000000000000,
    0b1111000000000000000000000000,
    0b1111000000000000000000000000,
    0b1111000000000000000000000000,
    0b1111000000000000000000000000,
    0b1111000000000000000000000000,
    0b1111000000000000000000000000,
    0b1111000000000000000000000000,
    0b1111000000000000000000000000,
    0b1111000000000000000000000000,
    0b1111000000000000000000000000,
    0b1111000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
    0b0000000000000000000000000000,
};

static const mono_glyph_t glyph_m = {28, 28, glyph_m_rows};
static const mono_glyph_t glyph_n = {28, 28, glyph_n_rows};
static const mono_glyph_t glyph_i = {28, 28, glyph_i_rows};

static bool glyph_get_pixel(const mono_glyph_t *g, uint8_t x, uint8_t y) {
    if (!g || x >= g->width || y >= g->height) {
        return false;
    }

    uint32_t row = g->rows[y];
    uint8_t bit = (g->width - 1) - x;
    return ((row >> bit) & 0x01U) != 0;
}

static bool mni_get_pixel(uint8_t x, uint8_t y) {
    if (y >= 28) {
        return false;
    }

    if (x <= 27) {
        return glyph_get_pixel(&glyph_m, x, y);
    }

    if (x >= 28 && x <= 51) {
        return glyph_get_pixel(&glyph_n, x - 28, y);
    }

    if (x >= 52 && x <= 75) {
        return glyph_get_pixel(&glyph_i, x - 52, y);
    }

    return false;
}

bool qp_draw_mni_rotated_solid(painter_device_t device, int16_t centerx, int16_t centery, float angle_deg, float scale, uint8_t hue, uint8_t sat, uint8_t val) {
    qp_dprintf("qp_draw_mni_rotated_solid: entry\n");

    painter_driver_t *driver = (painter_driver_t *)device;
    if (!driver || !driver->validate_ok) {
        qp_dprintf("qp_draw_mni_rotated_solid: fail (validation_ok == false)\n");
        return false;
    }

    if (scale <= 0.0f) {
        scale = 1.0f;
    }

    const int16_t src_w = 76;
    const int16_t src_h = 28;

    float scaled_w = src_w * scale;
    float scaled_h = src_h * scale;

    float rad = angle_deg * (float)M_PI / 180.0f;
    float c   = cosf(rad);
    float s   = sinf(rad);

    int16_t x_extent = (int16_t)ceilf((fabsf(scaled_w * c) + fabsf(scaled_h * s)) * 0.5f);
    int16_t y_extent = (int16_t)ceilf((fabsf(scaled_w * s) + fabsf(scaled_h * c)) * 0.5f);

    qp_internal_fill_pixdata(device, 1, hue, sat, val);

    if (!qp_comms_start(device)) {
        qp_dprintf("qp_draw_mni_rotated_solid: fail (could not start comms)\n");
        return false;
    }

    bool ret = true;

    for (int16_t dy = -y_extent; dy <= y_extent; dy++) {
        for (int16_t dx = -x_extent; dx <= x_extent; dx++) {
            float src_local_x =  (float)dx * c + (float)dy * s;
            float src_local_y = -(float)dx * s + (float)dy * c;

            float sx = src_local_x + scaled_w * 0.5f;
            float sy = src_local_y + scaled_h * 0.5f;

            if (sx < 0.0f || sy < 0.0f || sx >= scaled_w || sy >= scaled_h) {
                continue;
            }

            uint8_t src_px = (uint8_t)floorf(sx / scale);
            uint8_t src_py = (uint8_t)floorf(sy / scale);

            if (!mni_get_pixel(src_px, src_py)) {
                continue;
            }

            if (!qp_internal_setpixel_impl(device, centerx + dx, centery + dy)) {
                ret = false;
                goto out;
            }
        }
    }

out:
    qp_comms_stop(device);
    qp_dprintf("qp_draw_mni_rotated_solid: %s\n", ret ? "ok" : "fail");
    return ret;
}
