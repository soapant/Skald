#ifndef INPUT_CORE_WIN32_H
#define INPUT_CORE_WIN32_H
#include <windows.h>

LRESULT CALLBACK win32_handle_global_key(int nCode, WPARAM wParam,
                                         LPARAM lParam);

void win32_register_raw_mouse(HWND hwnd);
void win32_handle_raw_mouse_input(LPARAM lParam);

#endif
