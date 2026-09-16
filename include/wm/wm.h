#ifndef WINDOW_PLATFORM_H
#define WINDOW_PLATFORM_H

#include <stdbool.h>
#include "core/types.h"


char* Wm_GetActiveWindowTitle();
bool Wm_FocusByTitle(const char *title);
char* Wm_GetClipboardLastEntry();
char* Wm_Paste(char* content);
WindowList Wm_GetWindowTitles();
WindowList Wm_GetWindowNames();

#endif
