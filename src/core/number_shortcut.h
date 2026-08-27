#ifndef NUMBER_SHORTCUT_H
#define NUMBER_SHORTCUT_H

#include "core/types.h"

#define NUMBER_SHORTCUT_DELAY_MS 100 

bool HandleNumberShortcut(int wParam, WindowHandle hListBox, WindowHandle hEditControl, ListBoxInfo* lb);
void ResetNumberShortcutState(void);

#endif
