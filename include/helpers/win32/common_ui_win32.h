#ifndef COMMON_UI_WIN32_H
#define COMMON_UI_WIN32_H

#include <windows.h>
#include "core/types.h"

#define SKALD_CLASS "SkaldClass"
#define MESSAGE_CLASS "DisplayMessageModalClass"
#define WIDGET_CLASS "WidgetClass"

void win32_force_window_focus(WindowHandle hwndTarget);
typedef struct {
  HDC f_hdc;
  int f_ox;
  int f_oy;
  int f_ww;
  int f_wh;
  const RECT *f_region;
} Win32DrawWindowBorder;

void win32_draw_window_border(Win32DrawWindowBorder *argspec);
void win32_apply_rounded_corners(HWND hwnd, int w, int h);
typedef struct {
  HWND f_hwnd;
  int f_w;
  int f_h;
  bool f_rounded;
  int f_radius;
} Win32ApplyRounded;

void win32_apply_rounded(Win32ApplyRounded *argspec);
void win32_fill_rounded_rect(HDC hdc, RECT *rc, COLORREF col, int radius);

#endif

