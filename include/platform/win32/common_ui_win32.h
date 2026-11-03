#ifndef COMMON_UI_WIN32_H
#define COMMON_UI_WIN32_H

#include <windows.h>
#include "core/types.h"

HFONT CreateCustomNerdFont(int height, const char *face_name);
void UpdateTextCaret(void);
void CalculateWindowMetrics(WindowLayout *layout, int height, int width,
                            int item_height);
void ForceWindowFocus(WindowHandle hwndTarget, WindowHandle hwndInput);
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
WindowHandle FindWindowByPartialTitle(const char *partialTitle);
COLORREF LerpColor(COLORREF a, COLORREF b, float t);
void win32_draw_window_border(HDC hdc, int ox, int oy, int ww, int wh,
                              const RECT *region);
void win32_apply_rounded_corners(HWND hwnd, int w, int h);
void win32_fill_rounded_rect(HDC hdc, RECT *rc, COLORREF col, int radius);
int UI_InsertChar(wchar_t wc);

#endif
