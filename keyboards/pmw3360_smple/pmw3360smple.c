

#include QMK_KEYBOARD_H
#include "config.h"

#ifdef POINTING_DEVICE_ENABLE

static float accumulated_h = 0.0f;
static float accumulated_v = 0.0f;

    void pointing_device_init_kb(void) {
        pmw33xx_init(0);
        pmw33xx_init(1);         
        pmw33xx_set_cpi(0, 1000); // applies to first sensor
        pmw33xx_set_cpi(1, 1000); // applies to second sensor
        pointing_device_init_user();
    }

    report_mouse_t pointing_device_task_kb(report_mouse_t mouse_report) {
        pmw33xx_report_t report0 = pmw33xx_read_burst(0);
        pmw33xx_report_t report1 = pmw33xx_read_burst(1); // Sensor #2

        if (!report0.motion.b.is_lifted && report0.motion.b.is_motion) {
            // From quantum/pointing_device_drivers.c
            #define constrain_hid(amt) ((amt) < -127 ? -127 : ((amt) > 127 ? 127 : (amt)))
            mouse_report.x = constrain_hid(mouse_report.x + report0.delta_x);
            mouse_report.y = constrain_hid(mouse_report.y + report0.delta_y);
        }

        if (!report1.motion.b.is_lifted && report1.motion.b.is_motion) {
            int16_t x = report1.delta_x;
            int16_t y = report1.delta_y;
            const float diagonal_limit = 0.2f;
            float       ratio          = fabsf(y) / fabsf(x);
            if (ratio > diagonal_limit && ratio < (1.0f / diagonal_limit)) {
                return;
            } else if (ratio <= diagonal_limit) {
                accumulated_h += x / 2.0f;
            } else {
                accumulated_v += y / 1.0f;
            }
            if (fabsf(accumulated_h) >= 1.0f) {
                if (accumulated_h > 0) {
                    mouse_report.h = constrain_hid(mouse_report.h + 1);
                } else if (accumulated_h < 0) {
                    mouse_report.h = constrain_hid(mouse_report.h - 1);
                }
                accumulated_h = 0.0f;
            }
            if (fabsf(accumulated_v) >= 1.0f) {
                if (accumulated_v > 0) {
                    mouse_report.v = constrain_hid(mouse_report.v + 1);
                } else if (accumulated_v < 0) {
                    mouse_report.v = constrain_hid(mouse_report.v - 1);
                }
                accumulated_v = 0.0f;
            }
        }


        return pointing_device_task_user(mouse_report);
    }
#endif
