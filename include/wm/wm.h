#ifndef WINDOW_PLATFORM_H
#define WINDOW_PLATFORM_H

#include <stdbool.h>


// generic — dispatch to the right WM inside window.c
// ADD NEW FEATURES HERE: just add a new generic func here + implement it in window.c
bool Wm_GetActiveWindowTitle(char *buffer, int buffer_size);
bool Wm_FocusByTitle(const char *title);
char* Wm_GetClipboardLastEntry();
char* Wm_Paste(char* content);

#endif
