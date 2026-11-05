#include <windows.h>
#include "platform/win32/win32_state.h"
#include "deps/log.h"
#include "core/context.h"
#include "core/modal.h"
#include <commctrl.h>
#include <shellapi.h>
#include "config.h"
#include "platform/ui.h"
#include "platform/win32/common_ui_win32.h"
#include "platform/win32/win32_state.h"
#include "platform/win32/subclasses_win32.h"
#include "wingdi.h"
#include "helpers/helpers_ui.h"
#include "platform/draw.h"
#include "platform/win32/popup_helpers_win32.h"
#include "platform/win32/bg_image_win32.h"

#include "core/timer.h"
LRESULT CALLBACK SearchPopupProc(HWND hwnd, UINT msg, WPARAM wParam,
                                 LPARAM lParam) {

  ListBoxInfo *lb = NULL;
  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) &&
      IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd)))
    lb = GetLBFromPopup(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
  HFONT hFont = CreateCustomNerdFont(FontSize, UsedFont);
  switch (msg) {
  case WM_NCCREATE: {
    CREATESTRUCTA *cs = (CREATESTRUCTA *)lParam;
    SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    return DefWindowProcA(hwnd, msg, wParam, lParam);
  }

  case WM_CREATE: {
    SetFocus(hwnd);
    break;
  }

  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rect;
    GetClientRect(hwnd, &rect);
    if (win32_bg_image_enabled() &&
        win32_draw_bg_image_slice(hdc, &rect, rect.right, rect.bottom, 0, 0)) {
    } else {
      HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG);
      FillRect(hdc, &rect, hBgBrush);
      DeleteObject(hBgBrush);
    }

    {
      Platform_SetDrawContextWin32(hdc);
      Platform_DrawPromptEditText(NULL);
      Platform_DrawPromptEditCursor(NULL);
      Platform_SetDrawContextWin32(NULL);
    }

    if ("Insert" != NULL) {
      wchar_t wmode[256];
      MultiByteToWideChar(CP_UTF8, 0, "Insert", -1, wmode, 256);

      HFONT hMainFont = FH_TO_HFONT(Platform_CreateFont(FontSize, UsedFont));
      HFONT hOldFont = hMainFont ? (HFONT)SelectObject(hdc, hMainFont) : NULL;
      SetBkMode(hdc, TRANSPARENT);

      COLORREF modeColor = RGB(50, 255, 50);
      SetTextColor(hdc, modeColor);

      RECT modeRect;
      modeRect.left = 8;
      modeRect.top = 4;
      modeRect.right = rect.right - 8;
      modeRect.bottom = 28;

      DrawTextW(hdc, wmode, -1, &modeRect, DT_LEFT | DT_TOP);

      SelectObject(hdc, hOldFont);
      if (hMainFont)
        DeleteObject(hMainFont);

      HPEN hPen = CreatePen(PS_SOLID, 1, RGB(60, 65, 75));
      HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
      MoveToEx(hdc, 5, 30, NULL);
      LineTo(hdc, rect.right - 5, 30);
      SelectObject(hdc, hOldPen);
      DeleteObject(hPen);
    }

    EndPaint(hwnd, &ps);
    return 0;
  }

  case WM_KEYDOWN: {
    if (wParam == VK_ESCAPE) {
      DestroyWindow(hwnd);
      UI_Redraw(lb);
      return 0;
    }
    if (wParam == VK_RETURN) {
      if (GetKeyState(VK_SHIFT) & 0x8000)
        break;
      log_info("Enter pressed");
      DestroyWindow(hwnd);
      UI_Redraw(lb);
      return 0;
    }
    if (wParam == VK_BACK) {
      bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
      PromptState *ps = NULL;
      if (ctrl) {
        UI_SetText("");
      } else if (ps) {
        size_t len = strlen(ps->eb->filter);
        size_t cur = ps->eb->cursor;
        if (cur > len)
          cur = len;
        if (cur >= 2 && ps->eb->filter[cur - 1] == '\n' &&
            ps->eb->filter[cur - 2] == '\r') {
          memmove(ps->eb->filter + cur - 2, ps->eb->filter + cur, len - cur + 1);
          ps->eb->cursor = cur - 2;
        } else {
          UI_Backspace();
        }
      }
      InvalidateRect(hwnd, NULL, TRUE);
      return 0;
    }
    if (wParam == VK_DELETE) {
      PromptState *ps = NULL;
      if (ps) {
        size_t len = strlen(ps->eb->filter);
        size_t cur = ps->eb->cursor;
        if (cur > len)
          cur = len;
        if (cur + 1 < len && ps->eb->filter[cur] == '\r' &&
            ps->eb->filter[cur + 1] == '\n') {
          memmove(ps->eb->filter + cur, ps->eb->filter + cur + 2, len - cur - 1);
        } else {
          UI_DeleteForward();
        }
      }
      InvalidateRect(hwnd, NULL, TRUE);
      return 0;
    }
    break;
  }

  case WM_CHAR: {
    if (wParam == 0x08 || wParam == 0x7F)
      return 0;
    if (wParam >= 0x20 && true &&
        UI_InsertChar((wchar_t)wParam) > 0) {
      if (lb)
        UI_Redraw(lb);
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }
    break;
  }

  case WM_DESTROY: {
    DeleteObject(hFont);
    SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
    ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, WH_NULL);
    UpdateTextCaret();
    break;
  }
  }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}
