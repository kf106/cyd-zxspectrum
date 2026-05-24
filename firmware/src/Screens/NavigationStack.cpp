#ifdef CYD_TOUCH_KEYBOARD
#include "NavigationStack.h"
#include "../Input/CydTouchKeyboard.h"

void NavigationStack::setCydKeyboardEnabled(bool enabled)
{
  if (m_cydTouchKeyboard != nullptr)
  {
    m_cydTouchKeyboard->setEnabled(enabled);
  }
}
#endif
