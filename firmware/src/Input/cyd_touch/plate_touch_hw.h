#pragma once

#include <stdbool.h>
#include <stdint.h>

bool plate_touch_hw_init(void);
bool plate_touch_hw_pen_down(void);
uint16_t plate_touch_hw_read_z(void);
bool plate_touch_hw_read_plate(uint16_t *plate_x, uint16_t *plate_y);
