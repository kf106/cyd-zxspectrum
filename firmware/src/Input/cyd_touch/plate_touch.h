/*
 * CYD XPT2046 plate-ADC touch + four-corner calibration (from cyd-lords-of-midnight touch.c).
 * Hardware read is bit-banged on dedicated GPIOs (SPI3 pins); mapping matches LoM.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint16_t py_at_scr_x_min;
  uint16_t py_at_scr_x_max;
  uint16_t px_at_scr_y_min;
  uint16_t px_at_scr_y_max;
  bool valid;
} plate_touch_cal_t;

typedef void (*plate_touch_cal_step_cb_t)(int step_index, const char *colour_name, uint16_t colour_rgb565);
typedef void (*plate_touch_idle_cb_t)(void);

esp_err_t plate_touch_init(uint16_t screen_w, uint16_t screen_h);
bool plate_touch_try_read(int *screen_x, int *screen_y);
void plate_touch_interactive_calibration(plate_touch_cal_step_cb_t on_step);
void plate_touch_get_cal(plate_touch_cal_t *out);
void plate_touch_set_cal(const plate_touch_cal_t *cal);
void plate_touch_set_idle_callback(plate_touch_idle_cb_t cb);

#ifdef __cplusplus
}
#endif
