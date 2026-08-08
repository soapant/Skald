#ifndef FOCUS_SWITCHER_H
#define FOCUS_SWITCHER_H

#include <windows.h>
#include <stdbool.h>

typedef struct {
    const char* search_exe;
    HWND result;
} FindWindowData;

typedef struct {
    const char* search_title;
    HWND result;
} FindWindowSomething;

void FocusWindow(HWND hwnd);
HWND FindWindowByPartialTitle(const char* title);
bool HandleFocusSwitcher(int vk_code, int modifiers, const char* current_title);

#endif
