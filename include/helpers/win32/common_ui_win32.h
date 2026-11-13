#ifndef COMMON_UI_WIN32_H
#define COMMON_UI_WIN32_H

#include <windows.h>
#include "core/types.h"

#define QUICKSWITCHER_CLASS "QuickSwitcherModalClass"
#define MESSAGE_CLASS "DisplayMessageModalClass"
#define SEARCH_POPUP_CLASS "SearchPopupClass"
#define WIDGET_CLASS "WidgetClass"

void win32_force_window_focus(WindowHandle hwndTarget);
BOOL CALLBACK win32_enum_windows_proc(HWND hwnd, LPARAM lParam);
WindowHandle win32_find_window_by_partial_title(const char *partialTitle);
void win32_draw_window_border(HDC hdc, int ox, int oy, int ww, int wh,
                              const RECT *region);
void win32_apply_rounded_corners(HWND hwnd, int w, int h);
void win32_fill_rounded_rect(HDC hdc, RECT *rc, COLORREF col, int radius);

#endif
