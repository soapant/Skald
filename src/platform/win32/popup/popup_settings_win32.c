#include <windows.h>
#include "deps/log.h"
#include "core/context.h"
#include "core/modal.h"
#include <commctrl.h>
#include <shellapi.h>
#include "config.h"
#include "platform/draw.h"
#include "platform/ui.h"
#include "core/feature_data.h"
#include "core/types.h"
#include "platform/subclasses_win32.h"
#include "wingdi.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include "platform/win32/popup_helpers.h"

LRESULT CALLBACK SettingsPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
  case WM_NCCREATE: {
    CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
      SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
      return DefWindowProcA(hwnd, msg, wParam, lParam);
  }
    
  case WM_CREATE: {
    if (!FlintContext_Get()->g_hMainFont) {
      FlintContext_Get()->g_hMainFont = CreateFontA(FontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UsedFont);
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
      
      
      HFONT hOldFont = (HFONT)SelectObject(hdc, FlintContext_Get()->g_hMainFont);
      
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
        Win32_DrawBackgroundSlice(memDC, &rc, COLOR_BG, winW, winH, pt.x, pt.y);
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
      FlintContext_Get()->window.g_ActiveEditControl = FlintContext_Get()->window.hEditControl;
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
          UI_GetCurrentText(FlintContext_Get()->window.hEditControl, text);
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
