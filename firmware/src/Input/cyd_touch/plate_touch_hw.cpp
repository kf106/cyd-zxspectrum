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

#define PLATE_ADC_MAX 4095U

static bool s_hw_init = false;

static uint16_t plate_adc_sanitize(uint16_t raw)
{
  return (uint16_t)(raw & 0xFFFU);
}

static uint16_t xpt2046_transfer(uint8_t command)
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
  return plate_adc_sanitize(data >> 3);
}

bool plate_touch_hw_init(void)
{
  if (s_hw_init)
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
  s_hw_init = true;
  return true;
}

uint16_t plate_touch_hw_read_z(void)
{
  const uint16_t z1 = xpt2046_transfer(0xB1);
  const uint16_t z2 = xpt2046_transfer(0xC1);
  const uint32_t z = (uint32_t)z1 + (PLATE_ADC_MAX - z2);
  return (uint16_t)(z > PLATE_ADC_MAX ? PLATE_ADC_MAX : z);
}

bool plate_touch_hw_pen_down(void)
{
  if (plate_touch_hw_read_z() > CYD_TOUCH_Z_THRESHOLD)
  {
    return true;
  }
  return digitalRead(CYD_TOUCH_IRQ) == LOW;
}

static bool plate_hw_read_once(uint16_t *plate_x, uint16_t *plate_y)
{
  (void)xpt2046_transfer(0xD1);
  *plate_x = xpt2046_transfer(0xD1);
  *plate_y = xpt2046_transfer(0x91);
  return true;
}

static bool plate_adc_pair_valid(uint16_t px, uint16_t py)
{
  if (px == 0 && py == 0)
  {
    return false;
  }
  return true;
}

bool plate_touch_hw_read_plate(uint16_t *plate_x, uint16_t *plate_y)
{
  if (!plate_touch_hw_pen_down())
  {
    return false;
  }

  uint32_t sum_x = 0;
  uint32_t sum_y = 0;
  int count = 0;
  for (int i = 0; i < 5; i++)
  {
    uint16_t px = 0;
    uint16_t py = 0;
    if (!plate_hw_read_once(&px, &py))
    {
      continue;
    }
    if (!plate_adc_pair_valid(px, py))
    {
      continue;
    }
    sum_x += px;
    sum_y += py;
    count++;
  }
  if (count == 0)
  {
    return false;
  }
  *plate_x = (uint16_t)(sum_x / (uint32_t)count);
  *plate_y = (uint16_t)(sum_y / (uint32_t)count);
  return true;
}
