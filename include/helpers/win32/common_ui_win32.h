#ifndef COMMON_UI_WIN32_H
#define COMMON_UI_WIN32_H

#include <windows.h>
#include "core/types.h"

#define SKALD_CLASS "SkaldClass"
#define MESSAGE_CLASS "DisplayMessageModalClass"
#define WIDGET_CLASS "WidgetClass"

void win32_force_window_focus(WindowHandle hwndTarget);
typedef struct {
  HDC hdc;
  int ox;
  int oy;
  int ww;
  int wh;
  const RECT *region;
} Win32DrawWindowBorder;

void win32_draw_window_border(Win32DrawWindowBorder *argspec);
void win32_apply_rounded_corners(HWND hwnd, int w, int h);
typedef struct {
  HWND hwnd;
  int w;
  int h;
  bool rounded;
  int radius;
} Win32ApplyRounded;

void win32_apply_rounded(Win32ApplyRounded *argspec);
void win32_fill_rounded_rect(HDC hdc, RECT *rc, COLORREF col, int radius);

#endif

