#include "CydKeyboardImages.h"
#include "CydKeyboardTheme.h"
#include "../TFT/Display.h"
#include "../Screens/images/keyboard/keyboard_images.h"
#include <cstring>

static constexpr size_t CYD_BOTTOM_KEY_INDEX = 0;
static constexpr size_t CYD_SPACE_LEFT_SLOT = 8;
static constexpr size_t CYD_SPACE_RIGHT_SLOT = 9;

static void setImage(CydKeyImage &out, uint16_t w, uint16_t h, const uint16_t *data)
{
  out.width = w;
  out.height = h;
  out.data = data;
}

#define CYD_IMG(sym) setImage(out, sym##Width, sym##Height, sym##Data)

static bool imageForDigit(char digit, CydKeyImage &out)
{
  switch (digit)
  {
  case '0':
    CYD_IMG(key_0Image);
    return true;
  case '1':
    CYD_IMG(key_1Image);
    return true;
  case '2':
    CYD_IMG(key_2Image);
    return true;
  case '3':
    CYD_IMG(key_3Image);
    return true;
  case '4':
    CYD_IMG(key_4Image);
    return true;
  case '5':
    CYD_IMG(key_5Image);
    return true;
  case '6':
    CYD_IMG(key_6Image);
    return true;
  case '7':
    CYD_IMG(key_7Image);
    return true;
  case '8':
    CYD_IMG(key_8Image);
    return true;
  case '9':
    CYD_IMG(key_9Image);
    return true;
  default:
    return false;
  }
}

static bool imageForLetter(char letter, CydKeyImage &out)
{
  switch (letter)
  {
  case 'A':
    CYD_IMG(aImage);
    return true;
  case 'B':
    CYD_IMG(bImage);
    return true;
  case 'C':
    CYD_IMG(cImage);
    return true;
  case 'D':
    CYD_IMG(dImage);
    return true;
  case 'E':
    CYD_IMG(eImage);
    return true;
  case 'F':
    CYD_IMG(fImage);
    return true;
  case 'G':
    CYD_IMG(gImage);
    return true;
  case 'H':
    CYD_IMG(hImage);
    return true;
  case 'I':
    CYD_IMG(iImage);
    return true;
  case 'J':
    CYD_IMG(jImage);
    return true;
  case 'K':
    CYD_IMG(kImage);
    return true;
  case 'L':
    CYD_IMG(lImage);
    return true;
  case 'M':
    CYD_IMG(mImage);
    return true;
  case 'N':
    CYD_IMG(nImage);
    return true;
  case 'O':
    CYD_IMG(oImage);
    return true;
  case 'P':
    CYD_IMG(pImage);
    return true;
  case 'Q':
    CYD_IMG(qImage);
    return true;
  case 'R':
    CYD_IMG(rImage);
    return true;
  case 'S':
    CYD_IMG(sImage);
    return true;
  case 'T':
    CYD_IMG(tImage);
    return true;
  case 'U':
    CYD_IMG(uImage);
    return true;
  case 'V':
    CYD_IMG(vImage);
    return true;
  case 'W':
    CYD_IMG(wImage);
    return true;
  case 'X':
    CYD_IMG(xImage);
    return true;
  case 'Y':
    CYD_IMG(yImage);
    return true;
  case 'Z':
    CYD_IMG(zImage);
    return true;
  default:
    return false;
  }
}

bool cydKeyboardImageForLabel(const char *label, size_t keyIndex, CydKeyImage &out)
{
  if (label == nullptr || label[0] == '\0')
  {
    return false;
  }

  if (cydKeyboardThemeLoadImage(label, keyIndex, out))
  {
    return true;
  }

  if (strcmp(label, "Ent") == 0)
  {
    CYD_IMG(enterImage);
    return true;
  }
  if (strcmp(label, "Del") == 0)
  {
    CYD_IMG(delImage);
    return true;
  }
  if (strcmp(label, "Spc") == 0)
  {
    const size_t bottomSlot = keyIndex - CYD_BOTTOM_KEY_INDEX;
    if (bottomSlot == CYD_SPACE_LEFT_SLOT)
    {
      CYD_IMG(space_leftImage);
    }
    else
    {
      CYD_IMG(space_rightImage);
    }
    return true;
  }
  if (strcmp(label, "Sym") == 0)
  {
    CYD_IMG(symImage);
    return true;
  }
  if (strcmp(label, "Caps") == 0)
  {
    CYD_IMG(capsImage);
    return true;
  }
  if (strcmp(label, "Menu") == 0)
  {
    CYD_IMG(menuImage);
    return true;
  }
  if (strcmp(label, "R1") == 0)
  {
    CYD_IMG(r1Image);
    return true;
  }
  if (strcmp(label, "R2") == 0)
  {
    CYD_IMG(r2Image);
    return true;
  }
  if (strcmp(label, "R3") == 0)
  {
    CYD_IMG(r3Image);
    return true;
  }
  if (strcmp(label, "R4") == 0)
  {
    CYD_IMG(r4Image);
    return true;
  }

  if (label[1] == '\0')
  {
    const char c = label[0];
    if (c >= '0' && c <= '9')
    {
      return imageForDigit(c, out);
    }
    if (c >= 'A' && c <= 'Z')
    {
      return imageForLetter(c, out);
    }
    if (c >= 'a' && c <= 'z')
    {
      return imageForLetter((char)(c - 'a' + 'A'), out);
    }
  }

  return false;
}

void cydKeyboardDrawImage(Display &tft, int16_t x, int16_t y, const CydKeyImage &img)
{
  tft.setWindow(x, y, x + img.width - 1, y + img.height - 1);
  tft.pushPixels((uint16_t *)img.data, (uint32_t)img.width * img.height);
}
