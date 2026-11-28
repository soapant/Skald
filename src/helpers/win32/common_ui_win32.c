#include "config_values.h"
#include "core/context.h"
#include "core/types/color.h"
#include "core/types/window.h"
#include "deps/log.h"
#include "core/prompt_geometry.h"
#include "platform/platform.h"
#include "helpers/helpers_ui.h"
#include "helpers/win32/common_ui_win32.h"
#include <windows.h>
#include <commctrl.h>
#include <string.h>

void win32_apply_rounded(Win32ApplyRounded *argspec) {
  if (!argspec->hwnd || !IsWindow(argspec->hwnd))
    return;
  if (argspec->w <= 0 || argspec->h <= 0) {
    RECT rc;
    GetWindowRect(argspec->hwnd, &rc);
    argspec->w = rc.right - rc.left;
    argspec->h = rc.bottom - rc.top;
    if (argspec->w <= 0 || argspec->h <= 0)
      return;
  }

  HMODULE dwm = LoadLibraryA("dwmapi.dll");
  if (dwm) {
    HRESULT(WINAPI * pDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD) =
        (void *)GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (pDwmSetWindowAttribute) {
      const DWORD DWMWA_BORDER_COLOR = 34;
      COLORREF border = 0xFFFFFFFE;
      pDwmSetWindowAttribute(argspec->hwnd, DWMWA_BORDER_COLOR, &border,
                             sizeof(border));
      if (!argspec->rounded) {
        FreeLibrary(dwm);
        return;
      }
      const int DWMWCP_DONOTROUND = 0;
      const DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;
      pDwmSetWindowAttribute(argspec->hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                             &DWMWCP_DONOTROUND, sizeof(DWMWCP_DONOTROUND));
    }
    FreeLibrary(dwm);
  }

  if (!argspec->rounded)
    return;

  if (argspec->radius < 0)
    argspec->radius = 0;
  if (argspec->radius > 32)
    argspec->radius = 32;
  if (argspec->radius * 2 > argspec->w)
    argspec->radius = argspec->w / 2;
  if (argspec->radius * 2 > argspec->h)
    argspec->radius = argspec->h / 2;
  if (argspec->radius <= 0)
    return;
  HRGN rgn = CreateRoundRectRgn(0, 0, argspec->w + 1, argspec->h + 1,
                                argspec->radius * 2, argspec->radius * 2);
  if (rgn)
    SetWindowRgn(argspec->hwnd, rgn, TRUE);
}

void win32_apply_rounded_corners(HWND hwnd, int w, int h) {
  win32_apply_rounded(&(Win32ApplyRounded){.hwnd = hwnd,
                                           .w = w,
                                           .h = h,
                                           .rounded = RoundedCorners,
                                           .radius = CornerRadius});
}

static void win32_border_edge(HDC hdc, HBRUSH brush, int ex0, int ey0, int ex1,
                              int ey1, int ox, int oy, int rx0, int ry0,
                              int rx1, int ry1) {
  int ix0 = ex0 > rx0 ? ex0 : rx0;
  int iy0 = ey0 > ry0 ? ey0 : ry0;
  int ix1 = ex1 < rx1 ? ex1 : rx1;
  int iy1 = ey1 < ry1 ? ey1 : ry1;
  if (ix1 <= ix0 || iy1 <= iy0)
    return;
  RECT r;
  r.left = ix0 + ox;
  r.top = iy0 + oy;
  r.right = ix1 + ox;
  r.bottom = iy1 + oy;
  FillRect(hdc, &r, brush);
}

void win32_draw_window_border(Win32DrawWindowBorder *argspec) {
  if (!argspec->hdc || !EnableBorders || argspec->ww <= 0 || argspec->wh <= 0)
    return;
  int t = PROMPT_BORDER_W;
  if (t <= 0)
    return;
  int rx0 = 0, ry0 = 0, rx1 = argspec->ww, ry1 = argspec->wh;
  if (argspec->region) {
    rx0 = argspec->region->left;
    ry0 = argspec->region->top;
    rx1 = argspec->region->right;
    ry1 = argspec->region->bottom;
  }
  if (rx1 <= rx0 || ry1 <= ry0)
    return;
  int r = (RoundedCorners && CornerRadius > 0) ? CornerRadius : 0;
  if (r > argspec->ww / 2)
    r = argspec->ww / 2;
  if (r > argspec->wh / 2)
    r = argspec->wh / 2;
  if (r <= t)
    r = 0;
  HBRUSH brush = CreateSolidBrush(COLOR_BORDER);
  if (!brush)
    return;
  win32_border_edge(argspec->hdc, brush, r, 0, argspec->ww - r, t, argspec->ox,
                    argspec->oy, rx0, ry0, rx1, ry1);
  win32_border_edge(argspec->hdc, brush, r, argspec->wh - t, argspec->ww - r,
                    argspec->wh, argspec->ox, argspec->oy, rx0, ry0, rx1, ry1);
  win32_border_edge(argspec->hdc, brush, 0, r, t, argspec->wh - r, argspec->ox,
                    argspec->oy, rx0, ry0, rx1, ry1);
  win32_border_edge(argspec->hdc, brush, argspec->ww - t, r, argspec->ww,
                    argspec->wh - r, argspec->ox, argspec->oy, rx0, ry0, rx1,
                    ry1);
  if (r > t) {
    HRGN shape = CreateRoundRectRgn(0, 0, argspec->ww + 1, argspec->wh + 1,
                                    r * 2, r * 2);
    if (shape) {
      for (int y = 0; y < argspec->wh; y++) {
        if (y >= r && y < argspec->wh - r)
          continue;
        for (int x = 0; x < argspec->ww; x++) {
          if (x >= r && x < argspec->ww - r)
            continue;
          if (x < rx0 || x >= rx1 || y < ry0 || y >= ry1)
            continue;
          if (!PtInRegion(shape, x, y))
            continue;
          int edge = 0;
          if (!PtInRegion(shape, x - 1, y))
            edge = 1;
          if (!PtInRegion(shape, x + 1, y))
            edge = 1;
          if (!PtInRegion(shape, x, y - 1))
            edge = 1;
          if (!PtInRegion(shape, x, y + 1))
            edge = 1;
          if (!edge)
            continue;
          RECT px;
          px.left = x + argspec->ox;
          px.top = y + argspec->oy;
          px.right = x + argspec->ox + 1;
          px.bottom = y + argspec->oy + 1;
          FillRect(argspec->hdc, &px, brush);
        }
      }
      DeleteObject(shape);
    }
  }
  DeleteObject(brush);
}

void win32_fill_rounded_rect(HDC hdc, RECT *rc, COLORREF col, int radius) {
  if (!RoundedCorners || radius <= 0) {
    HBRUSH br = CreateSolidBrush(col);
    FillRect(hdc, rc, br);
    DeleteObject(br);
    return;
  }
  if (radius > (rc->right - rc->left) / 2)
    radius = (rc->right - rc->left) / 2;
  if (radius > (rc->bottom - rc->top) / 2)
    radius = (rc->bottom - rc->top) / 2;
  int save = SaveDC(hdc);
  HRGN rgn = CreateRoundRectRgn(rc->left, rc->top, rc->right + 1,
                                rc->bottom + 1, radius * 2, radius * 2);
  SelectClipRgn(hdc, rgn);
  HBRUSH br = CreateSolidBrush(col);
  FillRect(hdc, rc, br);
  DeleteObject(br);
  SelectClipRgn(hdc, NULL);
  RestoreDC(hdc, save);
  DeleteObject(rgn);
}

static bool win32_is_skald_window(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd))
    return false;
  char cls[64] = {0};
  GetClassNameA(hwnd, cls, sizeof(cls));
  return strcmp(cls, SKALD_CLASS) == 0 || strcmp(cls, MESSAGE_CLASS) == 0 ||
         strcmp(cls, WIDGET_CLASS) == 0;
}

void win32_force_window_focus(WindowHandle hwndTarget) {
  ShowWindow(WH_TO_HWND(hwndTarget), SW_SHOW);

  SetWindowPos(WH_TO_HWND(hwndTarget),
               win32_is_skald_window(WH_TO_HWND(hwndTarget)) ? HWND_TOPMOST
                                                             : HWND_TOP,
               0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW | SWP_ASYNCWINDOWPOS);

  HWND hForeground = GetForegroundWindow();
  DWORD foregroundThreadID = GetWindowThreadProcessId(hForeground, NULL);
  DWORD targetThreadID = GetWindowThreadProcessId(WH_TO_HWND(hwndTarget), NULL);
  DWORD currentThreadID = GetCurrentThreadId();

  if (foregroundThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, foregroundThreadID, TRUE);
  if (targetThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, targetThreadID, TRUE);

  SetForegroundWindow(WH_TO_HWND(hwndTarget));
  SetActiveWindow(WH_TO_HWND(hwndTarget));
  BringWindowToTop(WH_TO_HWND(hwndTarget));

  if (WH_TO_HWND(hwndTarget) != NULL &&
      IsWindowVisible(WH_TO_HWND(hwndTarget))) {
    SetFocus(WH_TO_HWND(hwndTarget));
  }
  update_text_caret();

  if (foregroundThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, foregroundThreadID, FALSE);
  if (targetThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, targetThreadID, FALSE);
}
