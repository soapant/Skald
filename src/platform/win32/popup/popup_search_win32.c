#include <windows.h>
#include "platform/win32/win32_state.h"
#include "deps/log.h"
#include "core/context.h"
#include "core/modal.h"
#include <commctrl.h>
#include <shellapi.h>
#include "config.h"
#include "platform/ui.h"
#include "platform/win32/subclasses_win32.h"
#include "wingdi.h"
#include "helpers/helpers_ui.h"
#include "platform/draw.h"
#include "platform/win32/popup_helpers_win32.h"

#include "core/timer.h"
LRESULT CALLBACK SearchPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PROFILE_START(_pt);
ListBoxInfo* lb = NULL;
  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) lb = GetLBFromPopup(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
  HFONT hFont = CreateCustomNerdFont(FontSize, UsedFont);
    switch (msg) {
    case WM_NCCREATE: {
      CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
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
        if (EnableShaders && ShaderPreset != 0) win32_draw_background(hdc, &rect);
        else { HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG); FillRect(hdc, &rect, hBgBrush); DeleteObject(hBgBrush); }

        {
          Platform_SetDrawContextWin32(hdc);
          Platform_DrawPromptEditText(g_win32_prompt_state);
          Platform_DrawPromptEditCursor(g_win32_prompt_state);
          Platform_SetDrawContextWin32(NULL);
        }

        if (g_vim_mode != NULL) {
          wchar_t wmode[256];
            MultiByteToWideChar(CP_UTF8, 0, g_vim_mode, -1, wmode, 256);
            
            HFONT hOldFont = (HFONT)SelectObject(hdc, g_win32_hMainFont);
            SetBkMode(hdc, TRANSPARENT);
            
            COLORREF modeColor;
            if (g_current_mode == MODE_NORMAL) {
              modeColor = RGB(100, 255, 255);
            } else if (g_current_mode == MODE_INSERT) {
              modeColor = RGB(50, 255, 50);
            } else {
              modeColor = RGB(200, 3, 222);
            }
          SetTextColor(hdc, modeColor);
            
            RECT modeRect;
            modeRect.left = 8;
            modeRect.top = 4;
            modeRect.right = rect.right - 8;
            modeRect.bottom = 28;
            
            DrawTextW(hdc, wmode, -1, &modeRect, DT_LEFT | DT_TOP);
            
            SelectObject(hdc, hOldFont);
            
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
        UI_Redraw(lb, FlintContext_Get()->window.hListBox);
        return 0;
      }
      if (wParam == VK_RETURN) {
        if (GetKeyState(VK_SHIFT) & 0x8000) break;
        log_info("Enter pressed");
        DestroyWindow(hwnd);
        UI_Redraw(lb, FlintContext_Get()->window.hListBox);
        return 0;
      }
      if (wParam == VK_BACK) {
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        PromptState* ps = g_win32_prompt_state;
        if (ctrl) {
          UI_SetText("");
        } else if (ps) {
          size_t len = strlen(ps->filter);
          size_t cur = ps->cursor;
          if (cur > len) cur = len;
          if (cur >= 2 && ps->filter[cur-1] == '\n' && ps->filter[cur-2] == '\r') {
            memmove(ps->filter + cur - 2, ps->filter + cur, len - cur + 1);
            ps->cursor = cur - 2;
          } else {
            UI_Backspace();
          }
        }
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
      }
      if (wParam == VK_DELETE) {
        PromptState* ps = g_win32_prompt_state;
        if (ps) {
          size_t len = strlen(ps->filter);
          size_t cur = ps->cursor;
          if (cur > len) cur = len;
          if (cur + 1 < len && ps->filter[cur] == '\r' && ps->filter[cur+1] == '\n') {
            memmove(ps->filter + cur, ps->filter + cur + 2, len - cur - 1);
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
      if (wParam == 0x08 || wParam == 0x7F) return 0;
      if (wParam >= 0x20 && IsInsertMode() && UI_InsertChar((wchar_t)wParam) > 0) {
        if (lb) UI_Redraw(lb, FlintContext_Get()->window.hListBox);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
      }
      break;
    }

    case WM_DESTROY: {
        DeleteObject(hFont);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
        ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, WH_NULL);
        SetModeWithListBox(MODE_NORMAL, lb);
        UpdateTextCaret();
        break;
    }
    }
  { PROFILE_END_VAR(_pt, "SearchPopupProc"); return DefWindowProcA(hwnd, msg, wParam, lParam); }
}
