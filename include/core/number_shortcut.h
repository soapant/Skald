#ifndef NUMBER_SHORTCUT_H
#define NUMBER_SHORTCUT_H

#include <windows.h>

#define NUMBER_SHORTCUT_DELAY_MS 100 

bool HandleNumberShortcut(int wParam, HWND hListBox, HWND hEditControl);
void ResetNumberShortcutState(void);

#endif
