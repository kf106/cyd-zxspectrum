#pragma once

#include <cstdint>
#include <cstddef>

struct CydKeyImage
{
  uint16_t width;
  uint16_t height;
  const uint16_t *data;
};

// Bottom-row index 8/9 are both labelled "Spc" — space-left / space-right.
bool cydKeyboardImageForLabel(const char *label, size_t keyIndex, CydKeyImage &out);

void cydKeyboardDrawImage(class Display &tft, int16_t x, int16_t y, const CydKeyImage &img);
