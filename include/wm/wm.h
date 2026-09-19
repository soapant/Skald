#ifndef WINDOW_PLATFORM_H
#define WINDOW_PLATFORM_H

#include <stdbool.h>
#include "core/types.h"


char* Wm_GetActiveWindowTitle();
bool Wm_FocusByTitle(const char *title);
char* Wm_Paste(char* content);
Fl_WindowList Wm_GetWindowTitles(void);
Fl_WindowList Wm_GetWindowNames(void);
Fl_Pos Wm_GetCursorPosition(void);

#endif
