#ifndef FOCUS_SWITCHER_H
#define FOCUS_SWITCHER_H

typedef struct {
    const char* search_title;
    WindowHandle result;
} FindWindowSomething;

void FocusWindow(WindowHandle hwnd);
WindowHandle FindWindowByPartialTitle(const char* title);
bool HandleFocusSwitcher(int vk_code, int modifiers, const char* current_title);

#endif
