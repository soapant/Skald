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
#include "platform/win32/subclasses_win32.h"
#include "wingdi.h"
#include "core/platform.h"
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
  case WM_DESTROY: {
      FlintContext_Get()->window.hSettings = WH_NULL;
      break;
  }
  }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK SettingsEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
  if (msg == WM_PAINT) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) { EndPaint(hwnd, &ps); return 0; }
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    if (!memDC || !memBmp) {
      if (memDC) DeleteDC(memDC);
      if (memBmp) DeleteObject(memBmp);
      EndPaint(hwnd, &ps);
      return DefSubclassProc(hwnd, msg, wParam, lParam);
    }
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
    HWND hPar = GetParent(hwnd);
    if (hPar && IsWindow(hPar)) {
      RECT winRc; GetClientRect(hPar, &winRc);
      int winW = winRc.right - winRc.left;
      int winH = winRc.bottom - winRc.top;
      POINT pt = {0,0};
      ClientToScreen(hwnd, &pt);
      ScreenToClient(hPar, &pt);
      if (EnableShaders && ShaderPreset != 0 && winW > 0 && winH > 0) {
        win32_draw_background_slice(memDC, &rc, COLOR_BG, winW, winH, pt.x, pt.y);
      } else {
        HBRUSH br = CreateSolidBrush(COLOR_BG);
        FillRect(memDC, &rc, br);
        DeleteObject(br);
      }
    } else {
      HBRUSH br = CreateSolidBrush(COLOR_BG);
      FillRect(memDC, &rc, br);
      DeleteObject(br);
    }
    SendMessage(hwnd, WM_PRINTCLIENT, (WPARAM)memDC, PRF_CLIENT);
    BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
    EndPaint(hwnd, &ps);
    return 0;
  }
  if (msg == WM_ERASEBKGND) return 1;
  ListBoxInfo* lb = GetLBFromChild(hwnd);
  (void)hwnd;
    
    if (msg == WM_KEYDOWN) {
      FlintContext_Get()->window.hActiveEditControl = FlintContext_Get()->window.hEditControl;
        bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
        if (wParam == VK_CONTROL || wParam == VK_SHIFT) {
          return DefSubclassProc(hwnd, msg, wParam, lParam);
        }
      
        if (wParam == VK_ESCAPE) {
          if (IsInsertMode()) {
            SetModeWithListBox(MODE_NORMAL, lb);
          }else {
            DestroyWindow(GetParent(hwnd));
              FlintContext_Get()->window.hSettings = WH_NULL;
              return 1;
          }
        }
      if (wParam == VK_BACK) {
        if (ctrl_pressed) {
          UI_SetText(FlintContext_Get()->window.hEditControl,"");
            ListBoxInfo* lbc = GetLBFromChild(hwnd);
            if (lbc) UI_Redraw(lbc, FlintContext_Get()->window.hListBox);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        } else {
          char buf[512]={0}; GetWindowTextA(hwnd, buf, sizeof(buf));
          DWORD s=0,e=0; SendMessageA(hwnd, EM_GETSEL, (WPARAM)&s, (WPARAM)&e);
          int len=(int)strlen(buf);
          if (s!=e) { memmove(buf+s, buf+e, len-e+1); SetWindowTextA(hwnd, buf); SendMessageA(hwnd, EM_SETSEL, s, s); }
          else if (s>0) { memmove(buf+s-1, buf+s, len-s+1); SetWindowTextA(hwnd, buf); SendMessageA(hwnd, EM_SETSEL, s-1, s-1); }
          InvalidateRect(hwnd, NULL, TRUE);
          return 0;
        }
      }
      if (wParam == VK_DELETE) {
        char buf[512]={0}; GetWindowTextA(hwnd, buf, sizeof(buf));
        DWORD s=0,e=0; SendMessageA(hwnd, EM_GETSEL, (WPARAM)&s, (WPARAM)&e);
        int len=(int)strlen(buf);
        if (s!=e) { memmove(buf+s, buf+e, len-e+1); SetWindowTextA(hwnd, buf); SendMessageA(hwnd, EM_SETSEL, s, s); }
        else if (s < (DWORD)len) { memmove(buf+s, buf+s+1, len-s); SetWindowTextA(hwnd, buf); SendMessageA(hwnd, EM_SETSEL, s, s); }
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
      }
      if (wParam == VK_DOWN) {
        if (ctrl_pressed) {
          char text[256];
          UI_GetCurrentTextEdit(FlintContext_Get()->window.hEditControl, text,sizeof(text));
          UI_SetCursorPosition(FlintContext_Get()->window.hEditControl,strlen(text));
        }
      }
      if (wParam == VK_UP) {
        if (ctrl_pressed) {
          UI_SetCursorPosition(FlintContext_Get()->window.hEditControl,1);
        }
      }
    } 
  if (msg == WM_CHAR) {
    if (wParam == 0x08 || wParam == 0x7F) return 0;
  }
  return DefSubclassProc(hwnd, msg, wParam, lParam);
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
  ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, FlintContext_Get()->window.hEditControl);
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

void Platform_SettingsPopUp() {
  if (!Platform_SettingsPopupShowWindow(NULL, 510, 210, 900, 700)) return;
  MSG msg;
  UI_PreparePopupPump();
  while (!FlintContext_Get()->window.selection_complete) {
    if (GetMessage(&msg, NULL, 0, 0)) {
      if (UI_ShouldSkipPopupKey(&msg)) continue;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      break;
    }
  }
  Platform_SettingsPopupHideWindow(NULL);
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
log_info("Platform_SettingsPopUp completed");
}

void SettingsCreateChildControls(WindowHandle hwndParent, HFONT hFont,const WindowLayout* layout) {
  HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(WH_TO_HWND(hwndParent), GWLP_HINSTANCE);

  FlintContext_Get()->window.hEditControl = WH_FROM_HWND(CreateWindowExA(
      0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      20, 50, layout->w - 40, 40, WH_TO_HWND(hwndParent), (HMENU)IDC_INPUT_BOX, hInstance, NULL
      ));

  if (WH_TO_HWND(FlintContext_Get()->window.hEditControl) == NULL) return;

  FlintContext_Get()->window.hActiveEditControl = FlintContext_Get()->window.hEditControl;
  UI_Edit_SetFont(FlintContext_Get()->window.hEditControl, hFont);
  SetWindowSubclass(WH_TO_HWND(FlintContext_Get()->window.hEditControl), SettingsEditSubclassProc, 0, 0);
}
