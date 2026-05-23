#include "plate_touch_hw.h"
#include <Arduino.h>

#ifndef CYD_TOUCH_CS
#define CYD_TOUCH_CS 33
#endif
#ifndef CYD_TOUCH_IRQ
#define CYD_TOUCH_IRQ 36
#endif
#ifndef CYD_TOUCH_CLK
#define CYD_TOUCH_CLK 25
#endif
#ifndef CYD_TOUCH_MISO
#define CYD_TOUCH_MISO 39
#endif
#ifndef CYD_TOUCH_MOSI
#define CYD_TOUCH_MOSI 32
#endif
#ifndef CYD_TOUCH_Z_THRESHOLD
#define CYD_TOUCH_Z_THRESHOLD 400
#endif

static bool s_hwInit = false;

static uint16_t xpt2046Transfer(uint8_t command)
{
  digitalWrite(CYD_TOUCH_CS, LOW);
  uint16_t data = 0;
  for (int bit = 7; bit >= 0; bit--)
  {
    digitalWrite(CYD_TOUCH_MOSI, (command >> bit) & 1);
    digitalWrite(CYD_TOUCH_CLK, LOW);
    delayMicroseconds(1);
    digitalWrite(CYD_TOUCH_CLK, HIGH);
    delayMicroseconds(1);
  }
  for (int bit = 15; bit >= 0; bit--)
  {
    digitalWrite(CYD_TOUCH_CLK, LOW);
    delayMicroseconds(1);
    if (digitalRead(CYD_TOUCH_MISO))
    {
      data |= (1 << bit);
    }
    digitalWrite(CYD_TOUCH_CLK, HIGH);
    delayMicroseconds(1);
  }
  digitalWrite(CYD_TOUCH_CS, HIGH);
  return data >> 3;
}

bool plate_touch_hw_init(void)
{
  if (s_hwInit)
  {
    return true;
  }
  pinMode(CYD_TOUCH_CS, OUTPUT);
  pinMode(CYD_TOUCH_CLK, OUTPUT);
  pinMode(CYD_TOUCH_MOSI, OUTPUT);
  pinMode(CYD_TOUCH_MISO, INPUT);
  pinMode(CYD_TOUCH_IRQ, INPUT);
  digitalWrite(CYD_TOUCH_CS, HIGH);
  digitalWrite(CYD_TOUCH_CLK, LOW);
  digitalWrite(CYD_TOUCH_MOSI, LOW);
  s_hwInit = true;
  return true;
}

bool plate_touch_hw_pen_down(void)
{
  if (xpt2046Transfer(0xB1) > CYD_TOUCH_Z_THRESHOLD)
  {
    return true;
  }
  return digitalRead(CYD_TOUCH_IRQ) == LOW;
}

bool plate_touch_hw_read_plate(uint16_t *plate_x, uint16_t *plate_y)
{
  if (!plate_touch_hw_pen_down())
  {
    return false;
  }
  xpt2046Transfer(0xB1);
  xpt2046Transfer(0xC1);
  *plate_y = xpt2046Transfer(0x91);
  *plate_x = xpt2046Transfer(0xD1);
  return true;
}
