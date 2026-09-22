#ifndef WINDOW_PLATFORM_H
#define WINDOW_PLATFORM_H

#include <stdbool.h>
#include "core/types.h"


char* Wm_GetActiveWindowTitle();
char* Wm_GetActiveWindowAppId();
bool Wm_FocusByTitle(const char *title);
char* Wm_Paste(char* content);
Fl_WindowList Wm_GetWindowTitles(void);
Fl_WindowList Wm_GetWindowNames(void);
Fl_Pos Wm_GetCursorPosition(void);
bool Wm_CloseActiveWindow(void);
bool Wm_ToggleFullscreenActive(void);
bool Wm_MoveActiveWindowToMonitor(bool right);
bool Wm_MinimizeActiveWindow(void);

#endif
