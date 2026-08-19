#ifndef NUMBER_SHORTCUT_H
#define NUMBER_SHORTCUT_H

#define NUMBER_SHORTCUT_DELAY_MS 100 

bool HandleNumberShortcut(int wParam, WindowHandle hListBox, WindowHandle hEditControl);
void ResetNumberShortcutState(void);

#endif
