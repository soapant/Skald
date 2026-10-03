#include <windows.h>
#include <string.h>
#include "platform/win32/win32_state.h"
#include "deps/log.h"
#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include <commctrl.h>
#include <shellapi.h>
#include "config.h"
#include "core/prompt_geometry.h"
#include "platform/ui.h"
#include "platform/win32/subclasses_win32.h"
#include "wingdi.h"
#include "helpers/helpers_ui.h"
#include "platform/win32/popup_helpers_win32.h"
#include "core/prompt_controller.h"

static bool EditPopupWindow_OnKey(HWND hWnd, EditPopupData* data, ListBoxInfo* lb, const PopupKeyEvent* key) {
    int wParam = key->vk;
    if (PromptController_TryVimMotion(wParam, key->ctrl, key->shift, key->alt)) {
        WindowManager* wm = WindowManager_Get();
        Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
        if (top && top->type == POPUP_EDIT) {
            WMEditPopupData* e = (WMEditPopupData*)top;
            if (e->text) SetWindowTextA(hWnd, e->text);
            SendMessageA(hWnd, EM_SETSEL, (WPARAM)e->cursor, (LPARAM)e->cursor);
        }
        InvalidateRect(hWnd, NULL, TRUE);
        return true;
    }
    if (wParam == VK_ESCAPE) {
        data->result = NULL;
        DestroyWindow(GetParent(hWnd));
        UI_Redraw(lb, FlintContext_Get()->window.hListBox);
        return true;
    }
    if (wParam == VK_RETURN) {
      if (key->shift) {
        return false;
      }
        char buffer[2048] = {0};
        GetWindowTextA(WH_TO_HWND(data->hEdit), buffer, sizeof(buffer) - 1);
        data->result = _strdup(buffer);
        DestroyWindow(GetParent(hWnd));
        UI_Redraw(lb, FlintContext_Get()->window.hListBox);
        return true;
    }
    if (wParam == VK_BACK) {
      if (key->ctrl) { SetWindowTextA(hWnd, ""); InvalidateRect(hWnd, NULL, TRUE); return true; }
      char buf[2048]={0}; GetWindowTextA(hWnd, buf, sizeof(buf));
      DWORD s=0,e=0; SendMessageA(hWnd, EM_GETSEL, (WPARAM)&s, (WPARAM)&e);
      int len=(int)strlen(buf);
      if (s!=e) { memmove(buf+s, buf+e, len-e+1); SetWindowTextA(hWnd, buf); SendMessageA(hWnd, EM_SETSEL, s, s); }
      else if (s>0) {
        if (s>=2 && buf[s-1]=='\n' && buf[s-2]=='\r') { memmove(buf+s-2, buf+s, len-s+1); SetWindowTextA(hWnd, buf); SendMessageA(hWnd, EM_SETSEL, s-2, s-2); }
        else { memmove(buf+s-1, buf+s, len-s+1); SetWindowTextA(hWnd, buf); SendMessageA(hWnd, EM_SETSEL, s-1, s-1); }
      }
      InvalidateRect(hWnd, NULL, TRUE);
      return true;
    }
    if (wParam == VK_DELETE) {
      char buf[2048]={0}; GetWindowTextA(hWnd, buf, sizeof(buf));
      DWORD s=0,e=0; SendMessageA(hWnd, EM_GETSEL, (WPARAM)&s, (WPARAM)&e);
      int len=(int)strlen(buf);
      if (s!=e) { memmove(buf+s, buf+e, len-e+1); SetWindowTextA(hWnd, buf); SendMessageA(hWnd, EM_SETSEL, s, s); }
      else if (s < (DWORD)len) {
        if (s+1 < (DWORD)len && buf[s]=='\r' && buf[s+1]=='\n') { memmove(buf+s, buf+s+2, len-s-1); SetWindowTextA(hWnd, buf); SendMessageA(hWnd, EM_SETSEL, s, s); }
        else { memmove(buf+s, buf+s+1, len-s); SetWindowTextA(hWnd, buf); SendMessageA(hWnd, EM_SETSEL, s, s); }
      }
      InvalidateRect(hWnd, NULL, TRUE);
      return true;
    }
    return false;
}

LRESULT CALLBACK EditPopupSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
  if (uMsg == WM_PAINT) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    HideCaret(hWnd);
    RECT rc; GetClientRect(hWnd, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) { ShowCaret(hWnd); EndPaint(hWnd, &ps); return 0; }
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    if (!memDC || !memBmp) {
      if (memDC) DeleteDC(memDC);
      if (memBmp) DeleteObject(memBmp);
      ShowCaret(hWnd);
      EndPaint(hWnd, &ps);
      return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
    HWND hPar = GetParent(hWnd);
    if (hPar && IsWindow(hPar)) {
      RECT winRc; GetClientRect(hPar, &winRc);
      int winW = winRc.right - winRc.left;
      int winH = winRc.bottom - winRc.top;
      POINT pt = {0,0};
      ClientToScreen(hWnd, &pt);
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
    SendMessage(hWnd, WM_PRINTCLIENT, (WPARAM)memDC, PRF_CLIENT);
    BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
    ShowCaret(hWnd);
    EndPaint(hWnd, &ps);
    return 0;
  }
  if (uMsg == WM_ERASEBKGND) return 1;
  ListBoxInfo* lb = GetLBFromChild(hWnd);
  
  if (!lb && WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) lb = GetLBFromPopup(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
  EditPopupData* data = (EditPopupData*)dwRefData;
    if (!data || !WH_TO_HWND(data->hEdit)) {
      data = (EditPopupData*)GetWindowLongPtrA(hWnd, GWLP_USERDATA);
    }
  if (uMsg == WM_KEYDOWN) {
    if (!data || !WH_TO_HWND(data->hEdit)) {
      return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }
    PopupKeyEvent key;
    memset(&key, 0, sizeof(key));
    key.vk = (int)wParam;
    key.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    key.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    key.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    if (EditPopupWindow_OnKey(hWnd, data, lb, &key)) return 0;
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
  }
  if (uMsg == WM_CHAR) {
    if (IsNormalMode()) return 0;
    if (wParam == 0x08 || wParam == 0x7F) return 0;
  }
  return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK EditPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  ListBoxInfo* lb = NULL;
  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) lb = GetLBFromPopup(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
  EditPopupData* data = (EditPopupData*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    
    switch (msg) {
    case WM_NCCREATE: {
      CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
      
    case WM_CREATE: {
      if (!data) break;
        HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE);
          RECT rect;
          GetClientRect(hwnd, &rect);

          int lineHeight = FontSize + FLINT_EDIT_H_BONUS;
          {
            HDC probe = CreateCompatibleDC(NULL);
            if (probe) {
              HFONT hOld = data->hFont ? (HFONT)SelectObject(probe, data->hFont) : NULL;
              TEXTMETRIC tm;
              memset(&tm, 0, sizeof(tm));
              if (GetTextMetrics(probe, &tm) && tm.tmHeight > 0) lineHeight = tm.tmHeight;
              if (hOld) SelectObject(probe, hOld);
              DeleteDC(probe);
            }
          }

          data->hEdit = WH_FROM_HWND(CreateWindowExA(
                                        0, "EDIT", data->initial_text,
                                        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
                                        FLINT_EDITPOPUP_INSET,
                                        (rect.bottom / 2) + FLINT_EDITPOPUP_CENTER_OFFSET_Y,
                                        rect.right - FLINT_EDITPOPUP_INSET * 2, lineHeight,
                                        hwnd, (HMENU)1001, hInstance, NULL
                                       ));

          if (WH_TO_HWND(data->hEdit)) {
            SendMessageA(WH_TO_HWND(data->hEdit), EM_SETMARGINS,
                EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(0, 0));
            UI_Edit_SetFont(data->hEdit,data->hFont);
              UI_Edit_SetSel(data->hEdit,0,0);
              SetFocus(WH_TO_HWND(data->hEdit));
              FlintContext_Get()->window.hActiveEditControl = data->hEdit;
              
              SetWindowLongPtrA(WH_TO_HWND(data->hEdit), GWLP_USERDATA, (LONG_PTR)data);
              SetWindowSubclass(WH_TO_HWND(data->hEdit), EditPopupSubclassProc, 0, (DWORD_PTR)data);
          }
      break;
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
        if (EnableShaders && ShaderPreset != 0) win32_draw_background(hdc, &rect);
        else { HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG); FillRect(hdc, &rect, hBgBrush); DeleteObject(hBgBrush); }
        
        if (g_vim_mode != NULL && VimModeText) {
          wchar_t wmode[256];
            MultiByteToWideChar(CP_UTF8, 0, g_vim_mode, -1, wmode, 256);
            
            HFONT hOldFont = (HFONT)SelectObject(hdc, g_win32_hMainFont);
            SetBkMode(hdc, TRANSPARENT);
            
            COLORREF modeColor;
            if (g_current_mode == MODE_NORMAL) {
              modeColor = COLOR_NORMAL_MODE_TEXT;
            } else if (g_current_mode == MODE_INSERT) {
              modeColor = COLOR_INSERT_MODE_TEXT;
            } else {
              modeColor = COLOR_VISUAL_MODE_TEXT;
            }
          SetTextColor(hdc, modeColor);

            SIZE msize;
            GetTextExtentPoint32W(hdc, wmode, (int)wcslen(wmode), &msize);

            RECT modeRect;
            modeRect.left = FLINT_EDITPOPUP_MODE_X;
            modeRect.top = FLINT_EDITPOPUP_MODE_Y;
            modeRect.right = rect.right - FLINT_EDITPOPUP_MODE_X;
            modeRect.bottom = modeRect.top + msize.cy;

            DrawTextW(hdc, wmode, -1, &modeRect, DT_LEFT | DT_TOP | DT_SINGLELINE);

            SelectObject(hdc, hOldFont);
        }
      
        EndPaint(hwnd, &ps);
        return 0;
    }
            
    case WM_DESTROY: {
      FlintContext_Get()->window.hActiveEditControl = FlintContext_Get()->window.hEditControl;
        if (data) {
          if (data->initial_text) { free((void*)data->initial_text); data->initial_text = NULL; }
            if (data->hFont) { DeleteObject(data->hFont); data->hFont = NULL; }
              SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
        }
        ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, FlintContext_Get()->window.hEditControl);
        last_index = lb ? lb->selected_index : -1;
        SetModeWithListBox(MODE_NORMAL, lb);
        UpdateTextCaret(FlintContext_Get()->window.hEditControl);
        break;
    }
    }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}
