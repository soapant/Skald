#ifndef INPUT_CORE_WIN32_H
#define INPUT_CORE_WIN32_H
#include <windows.h>

LRESULT CALLBACK win32_handle_global_key(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK win32_handle_global_mouse(int nCode, WPARAM wParam, LPARAM lParam);

#endif
