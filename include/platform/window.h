#ifndef WINDOW_PLATFORM_H
#define WINDOW_PLATFORM_H

#include <stdbool.h>

typedef enum {
    WM_UNKNOWN = 0,
    WM_X11,
    WM_SWAY,
    WM_HYPRLAND,
} WM_Type;

extern WM_Type g_current_wm;

WM_Type Window_DetectWM(void);

// generic — dispatch to the right WM inside window.c
// ADD NEW FEATURES HERE: just add a new generic func here + implement it in window.c
bool Window_GetActiveWindowTitle(char *buffer, int buffer_size);
bool Window_FocusByTitle(const char *title);
bool Window_SetFloating(const char *title, bool floating);

#endif
