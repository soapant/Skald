#ifndef INPUT_CORE_WIN32_H
#define INPUT_CORE_WIN32_H
#include <windows.h>

LRESULT CALLBACK win32_handle_global_key(int nCode, WPARAM wParam, LPARAM lParam);

void Platform_RegisterRawMouse(HWND hwnd);
void Platform_HandleRawMouseInput(LPARAM lParam);

#endif
