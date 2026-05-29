#pragma once

#include "CydKeyboardImages.h"

class IFiles;

// Game keyboard packs (.kbd) on SD (sibling of the .tap/.tzx) are read directly from the card.
void cydKeyboardThemeInit(IFiles *files);
void cydKeyboardThemeResetToBuiltin();
bool cydKeyboardThemeHasGamePack();
bool cydKeyboardThemeUseCustom();
bool cydKeyboardThemeTryLoadForTape(const char *tapePath);
bool cydKeyboardThemeOnPlayfieldTap();
void cydKeyboardThemeClearPlayfieldLatch();
bool cydKeyboardThemeLoadImage(const char *label, size_t keyIndex, CydKeyImage &out);
