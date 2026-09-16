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
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_file.h"
#include "helpers/helpers_ui.h"
#include "platform/win32/popup_helpers.h"

LRESULT CALLBACK SimpleEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
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
  HWND hParent = GetParent(hwnd);
    (void)GetDlgItem(hParent, IDC_LIST_BOX);
    (void)hwnd;
    bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
    
    if (msg == WM_KEYDOWN) {
      FlintContext_Get()->window.g_ActiveEditControl = FlintContext_Get()->window.hEditControl;
        if (wParam == VK_CONTROL || wParam == VK_SHIFT || wParam == VK_MENU) {
          return DefSubclassProc(hwnd, msg, wParam, lParam);
        }
      if (wParam == VK_RETURN) {
        GetWindowTextA(WH_TO_HWND(FlintContext_Get()->window.hEditControl), FlintContext_Get()->window.output_buffer, sizeof(FlintContext_Get()->window.output_buffer));
          
          FlintContext_Get()->window.selection_complete = true; 
          
          return 0; 
      }
      
        if (wParam == VK_ESCAPE) {
          FlintContext_Get()->window.output_buffer[0] = '\0'; 
            FlintContext_Get()->window.selection_complete = true;  
            if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
              KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
                DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
            }
          return 0;
        }
      
        if (wParam == VK_BACK) {
          if (ctrl_pressed) {
            UI_SetText(FlintContext_Get()->window.hEditControl,"");
              ListBoxInfo* lb2 = GetLBFromChild(hwnd);
              if (lb2) UI_Redraw(lb2, FlintContext_Get()->window.hListBox);
              InvalidateRect(hwnd, NULL, TRUE);
              return 0;
          } else {
            char buf[512] = {0};
            GetWindowTextA(hwnd, buf, sizeof(buf));
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
    } 
  if (msg == WM_CHAR) {
    if (wParam == 0x08 || wParam == 0x7F) return 0;
  }
  return DefSubclassProc(hwnd, msg, wParam, lParam);
} 

LRESULT CALLBACK SimpleListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
  static HBRUSH s_bg_brush = NULL;
    static HBRUSH s_gradient_brushes[32] = {0};
    static bool s_gradient_ready = false;
    (void)s_gradient_brushes; (void)s_gradient_ready; (void)dwRefData;
    
    switch (uMsg) {
    case WM_NCDESTROY:
                      RemoveWindowSubclass(hWnd, SimpleListBoxSubclassProc, uIdSubclass);
                        if (s_bg_brush) { DeleteObject(s_bg_brush); s_bg_brush = NULL; }
                          for (int i = 0; i < 32; i++) { if (s_gradient_brushes[i]) { DeleteObject(s_gradient_brushes[i]); s_gradient_brushes[i] = NULL; } }
                            s_gradient_ready = false;
                              break;
                              
    case WM_ERASEBKGND:
                       return TRUE;
                         
    case WM_PAINT: {
      PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rect;
        GetClientRect(hWnd, &rect);
        if (EnableShaders && ShaderPreset != 0) {
          HWND hParent = GetParent(hWnd);
          RECT winRc; GetClientRect(hParent ? hParent : hWnd, &winRc);
          int winW = winRc.right - winRc.left;
          int winH = winRc.bottom - winRc.top;
          POINT lbOrigin = {0,0};
          if (hParent) { ClientToScreen(hWnd, &lbOrigin); ScreenToClient(hParent, &lbOrigin); }
          if (winW > 0 && winH > 0) Win32_DrawBackgroundSlice(hdc, &rect, COLOR_BG, winW, winH, lbOrigin.x, lbOrigin.y);
          else Win32_DrawBackground(hdc, &rect);
        } else {
          if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
            FillRect(hdc, &rect, s_bg_brush);
        }
        int textLen = UI_LB_GetTextLen(WH_FROM_HWND(hWnd));
        if (textLen > 0 && textLen < 512) {
          char textBuffer[512] = {0};
            UI_LB_GetText(WH_FROM_HWND(hWnd), 0, textBuffer);
            HFONT hFont = UI_GetFont(WH_FROM_HWND(hWnd));
            HFONT hOldFont = NULL;
            if (hFont) hOldFont = (HFONT)SelectObject(hdc, hFont);
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, COLOR_TEXT);
                int itemHeight = UI_LB_GetItemHeight(WH_FROM_HWND(hWnd));
                if (itemHeight <= 0) itemHeight = 40;
                    RECT textRect = rect;
                    textRect.left = 0;
                    textRect.right = rect.right;
                    textRect.bottom = textRect.top + itemHeight;
                    DrawTextA(hdc, textBuffer, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
                    if (hOldFont) SelectObject(hdc, hOldFont);
        }
        EndPaint(hWnd, &ps);
        return 0;
    }
    }
  return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
