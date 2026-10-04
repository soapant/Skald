#include <windows.h>
#include "platform/win32/win32_state.h"
#include "deps/log.h"
#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include <commctrl.h>
#include <shellapi.h>
#include "config.h"
#include "platform/ui.h"
#include "platform/draw.h"
#include "platform/win32/subclasses_win32.h"
#include "wingdi.h"
#include "platform/platform.h"
#include "helpers/helpers_ui.h"
#include "platform/win32/popup_helpers_win32.h"
#include "platform/win32/common_ui_win32.h"

LRESULT CALLBACK SettingsPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
  case WM_NCCREATE: {
    CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
      SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
      return DefWindowProcA(hwnd, msg, wParam, lParam);
  }
    
  case WM_CREATE: {
    if (!g_win32_hMainFont) {
      g_win32_hMainFont = FH_TO_HFONT(Platform_CreateFont(FontSize, UsedFont));
    }
    break;
  }
      
  case WM_ERASEBKGND: {
    return TRUE;
  }
        
  case WM_CTLCOLOREDIT: {
    HDC hdc = (HDC)wParam;
      if (EnableShaders && ShaderPreset != 0) {
        SetTextColor(hdc, COLOR_TEXT);
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
      } else {
        SetTextColor(hdc, COLOR_TEXT);
        SetBkColor(hdc, COLOR_BG);
        static HBRUSH hBrush = NULL;
        if (!hBrush) hBrush = CreateSolidBrush(COLOR_BG);
          return (LRESULT)hBrush;
      }
  }
          
  case WM_PAINT: {
    PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      RECT rect;
      GetClientRect(hwnd, &rect);
      
      
      HFONT hOldFont = (HFONT)SelectObject(hdc, g_win32_hMainFont);
      
      HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG);
      FillRect(hdc, &rect, hBgBrush);
      DeleteObject(hBgBrush);
      
      if (g_vim_mode != NULL) {
        wchar_t wmode[256];
          MultiByteToWideChar(CP_UTF8, 0, g_vim_mode, -1, wmode, 256);
          
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
      }

    {
      Platform_SetDrawContextWin32(hdc);
      Platform_DrawPromptEditText(g_win32_prompt_state);
      Platform_DrawPromptEditCursor(g_win32_prompt_state);
      Platform_SetDrawContextWin32(NULL);
    }

    wchar_t wtitle[256];
    MultiByteToWideChar(CP_UTF8, 0, "Settings", -1, wtitle, 256);

    SetBkMode(hdc, TRANSPARENT);

    SIZE sz;
    GetTextExtentPoint32W(hdc, wtitle, (int)wcslen(wtitle), &sz);

    RECT titleRect;
    titleRect.left = rect.right - sz.cx - 20;
    titleRect.top = 10;
    titleRect.right = rect.right - 10;
    titleRect.bottom = rect.top + sz.cy + 15;

    COLORREF titleColor;
    titleColor = RGB(0, 255, 0);

    SetTextColor(hdc, titleColor);
    DrawTextW(hdc, wtitle, -1, &titleRect, DT_LEFT | DT_TOP);

    SelectObject(hdc, hOldFont);
    
      
      SelectObject(hdc, hOldFont);
      
      EndPaint(hwnd, &ps);
      return 0;
  }
  case WM_KEYDOWN: {
      bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
      if (wParam == VK_CONTROL || wParam == VK_SHIFT) break;
      if (wParam == VK_ESCAPE) {
        if (IsInsertMode()) {
          SetModeWithListBox(MODE_NORMAL, GetLBFromChild(hwnd));
        } else {
          DestroyWindow(hwnd);
          FlintContext_Get()->window.hSettings = WH_NULL;
          return 0;
        }
      }
      if (wParam == VK_BACK) {
        if (ctrl_pressed) {
          UI_SetText("");
          ListBoxInfo* lbc = GetLBFromChild(hwnd);
          if (lbc) UI_Redraw(lbc, FlintContext_Get()->window.hListBox);
        } else {
          PromptState* ps = g_win32_prompt_state;
          if (ps) {
            size_t len = strlen(ps->filter);
            size_t cur = ps->cursor;
            if (cur > len) cur = len;
            if (cur > 0) {
              memmove(ps->filter + cur - 1, ps->filter + cur, len - cur + 1);
              ps->cursor = cur - 1;
            }
          }
        }
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
      }
      if (wParam == VK_DELETE) {
        UI_DeleteForward();
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
      }
      if (wParam == VK_DOWN && ctrl_pressed) {
        char text[256];
        UI_GetCurrentTextEdit(text, sizeof(text));
        UI_SetCursorPosition((int)strlen(text));
      }
      if (wParam == VK_UP && ctrl_pressed) {
        UI_SetCursorPosition(1);
      }
      break;
  }

  case WM_CHAR: {
      if (wParam == 0x08 || wParam == 0x7F) return 0;
      if (wParam >= 0x20 && IsInsertMode() && UI_InsertChar((wchar_t)wParam) > 0) {
        ListBoxInfo* lbc = GetLBFromChild(hwnd);
        if (lbc) UI_Redraw(lbc, FlintContext_Get()->window.hListBox);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
      }
      break;
  }

  case WM_DESTROY: {
      FlintContext_Get()->window.hSettings = WH_NULL;
      break;
  }
  }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static HFONT s_settings_font = NULL;

bool Platform_SettingsPopupShowWindow(Popup* self, int x, int y, int w, int h) {
  (void)self;
  HINSTANCE hInstance = GetModuleHandle(NULL);
  SetMode(MODE_INSERT);
  WindowLayout layout;
  CalculateWindowMetrics(&layout, h, w, 32);
  s_settings_font = CreateCustomNerdFont(FontSize, UsedFont);
  FlintContext_Get()->window.hPopupWnd = WH_FROM_HWND(CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_COMPOSITED,
      "SettingsPopupClass", NULL, WS_POPUP | WS_CLIPCHILDREN,
      x, y, w, h, NULL, NULL, hInstance, NULL
      ));
  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) == NULL) {
    DWORD err = GetLastError();
    log_error("CreateWindowExA failed: %lu", err);
    DeleteObject(s_settings_font);
    s_settings_font = NULL;
    return false;
  }
  win32_apply_rounded_corners(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), w, h);
  FlintContext_Get()->window.hSettings = FlintContext_Get()->window.hPopupWnd;
  SetTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), 999, 50, NULL);
  SettingsCreateChildControls(FlintContext_Get()->window.hPopupWnd, s_settings_font, &layout);
  SetLayeredWindowAttributes(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), 0, (BYTE)WindowOpacity, LWA_ALPHA);
  ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, WH_NULL);
  return true;
}

void Platform_SettingsPopupHideWindow(Popup* self) {
  (void)self;
  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) {
    KillTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), 999);
    DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
    FlintContext_Get()->window.hPopupWnd = WH_NULL;
  }
  FlintContext_Get()->window.hSettings = WH_NULL;
  if (s_settings_font) { DeleteObject(s_settings_font); s_settings_font = NULL; }
}

void Platform_SettingsPopupRender(Popup* self) {
  (void)self;
}

void Platform_SettingsPopupDestroyWindow(Popup* self) {
  Platform_SettingsPopupHideWindow(self);
}

bool Platform_SettingsPopupOnKey(Popup* self, const PopupKeyEvent* key) {
  (void)self;
  (void)key;
  return false;
}

void SettingsCreateChildControls(WindowHandle hwndParent, HFONT hFont,const WindowLayout* layout) {
  (void)hwndParent;
  (void)layout;
  UI_Edit_SetFont(hFont);
}
