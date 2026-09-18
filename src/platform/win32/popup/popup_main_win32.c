#include <windows.h>
#include "platform/win32/win32_state.h"
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
#include "platform/win32/subclasses_win32.h"
#include "platform/input_state.h"
#include "wingdi.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_file.h"
#include "helpers/helpers_ui.h"
#include "platform/win32/popup_helpers_win32.h"
#include "core/flapi_platform.h"

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
  if(msg==WM_KEYDOWN){ int mods=0; if(GetKeyState(VK_CONTROL)&0x8000) mods|=HOTKEY_MOD_CTRL; if(GetKeyState(VK_MENU)&0x8000) mods|=HOTKEY_MOD_ALT; if(GetKeyState(VK_SHIFT)&0x8000) mods|=HOTKEY_MOD_SHIFT; if(GetKeyState(VK_LWIN)&0x8000||GetKeyState(VK_RWIN)&0x8000) mods|=HOTKEY_MOD_SUPER; Platform_InputState_Update((int)wParam,true,mods); }
  if(msg==WM_KEYUP){ int mods=0; if(GetKeyState(VK_CONTROL)&0x8000) mods|=HOTKEY_MOD_CTRL; if(GetKeyState(VK_MENU)&0x8000) mods|=HOTKEY_MOD_ALT; if(GetKeyState(VK_SHIFT)&0x8000) mods|=HOTKEY_MOD_SHIFT; if(GetKeyState(VK_LWIN)&0x8000||GetKeyState(VK_RWIN)&0x8000) mods|=HOTKEY_MOD_SUPER; Platform_InputState_Update((int)wParam,false,mods); }
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
  HWND hParent = GetParent(hwnd);
    (void)GetDlgItem(hParent, IDC_LIST_BOX);
    (void)hwnd;
    
    if (msg == WM_KEYDOWN) {
      FlintContext_Get()->window.hActiveEditControl = FlintContext_Get()->window.hEditControl;
        bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
        if (wParam == VK_CONTROL || wParam == VK_SHIFT) {
          return DefSubclassProc(hwnd, msg, wParam, lParam);
        }
      
        if (!ctrl_pressed) {
          if (wParam == VK_UP || wParam == VK_DOWN) {
            if (WH_TO_HWND(FlintContext_Get()->window.hListBox) != NULL) {
              if (wParam == VK_UP) {
                UI_NavigateUp(lb, FlintContext_Get()->window.hListBox);
              } else {
                UI_NavigateDown(lb, FlintContext_Get()->window.hListBox);
              }
            }
            return 0;
          }
        }
      
        if (wParam == VK_RETURN) {
          bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
            FlintContext_Get()->window.output_buffer[0] = '\0';
            if (UI_IsListBoxSelected(lb)) {
              if (lb && lb->selected_index >=0 && lb->selected_index < lb->populated_count) strncpy(FlintContext_Get()->window.output_buffer, lb->populated_cache[lb->selected_index], sizeof(FlintContext_Get()->window.output_buffer) - 1);
              FlintContext_Get()->window.output_buffer[sizeof(FlintContext_Get()->window.output_buffer)-1] = '\0';
            } else {
              GetWindowTextA(hwnd, FlintContext_Get()->window.output_buffer, sizeof(FlintContext_Get()->window.output_buffer));
            }
            if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
              KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
                DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
                FlintContext_Get()->window.hDisplayMessage = WH_NULL;
            }
          if (ctrl_pressed) {
              bool should_close = false;
              if (FlintContext_Get()->prompt_callbacks.ctrl_enter != NULL) {
                char sel_copy[256];
                strncpy(sel_copy, FlintContext_Get()->window.output_buffer, sizeof(sel_copy)-1);
                sel_copy[sizeof(sel_copy)-1] = '\0';
                should_close = FlintContext_Get()->prompt_callbacks.ctrl_enter(sel_copy, lb);
              } else {
                should_close = false;
              }
              if (should_close) FlintContext_Get()->window.selection_complete = true;
              else if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL) ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, FlintContext_Get()->window.hEditControl);
            return 0;
          } else {
              bool should_close = false;
              if (FlintContext_Get()->prompt_callbacks.enter != NULL) {
                char sel_copy[256];
                strncpy(sel_copy, FlintContext_Get()->window.output_buffer, sizeof(sel_copy)-1);
                sel_copy[sizeof(sel_copy)-1] = '\0';
                should_close = FlintContext_Get()->prompt_callbacks.enter(sel_copy, lb);
              } else {
                should_close = true;
              }
              if (should_close) FlintContext_Get()->window.selection_complete = true;
              else if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL) ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, FlintContext_Get()->window.hEditControl);
              return 0;
          }
        }
      
        if (wParam == VK_ESCAPE) {
          FlintContext_Get()->window.output_buffer[0] = '\0'; 
            FlintContext_Get()->window.selection_complete = true;  
            if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
              KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
                DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
            }
          return 1;
        }
      
        if (wParam == VK_TAB) {
          struct feature_data* data = GetFeatureData(FlintContext_Get()->window.current_feature);
            if (FlintContext_Get()->prompt_callbacks.tab != NULL) {
              FlintContext_Get()->prompt_callbacks.tab(lb);
            }
          for (int i = 0;i < data->selection_count;++i) {
            log_info("%s ", data->last_selections[i]);
          }
          return 0;
        }
      if (wParam == VK_BACK) {
        if (ctrl_pressed) {
          UI_SetText(FlintContext_Get()->window.hEditControl,"");
            UI_Redraw(lb, FlintContext_Get()->window.hListBox);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        } else {
          char buf[512] = {0};
          GetWindowTextA(hwnd, buf, sizeof(buf));
          DWORD selStart = 0, selEnd = 0;
          SendMessageA(hwnd, EM_GETSEL, (WPARAM)&selStart, (WPARAM)&selEnd);
          int len = (int)strlen(buf);
          if (selStart != selEnd) {
            memmove(buf + selStart, buf + selEnd, len - selEnd + 1);
            SetWindowTextA(hwnd, buf);
            SendMessageA(hwnd, EM_SETSEL, selStart, selStart);
          } else if (selStart > 0) {
            memmove(buf + selStart - 1, buf + selStart, len - selStart + 1);
            SetWindowTextA(hwnd, buf);
            SendMessageA(hwnd, EM_SETSEL, selStart - 1, selStart - 1);
          }
          InvalidateRect(hwnd, NULL, TRUE);
          return 0;
        }
      }
      if (wParam == VK_DELETE) {
        char buf[512] = {0};
        GetWindowTextA(hwnd, buf, sizeof(buf));
        DWORD selStart = 0, selEnd = 0;
        SendMessageA(hwnd, EM_GETSEL, (WPARAM)&selStart, (WPARAM)&selEnd);
        int len = (int)strlen(buf);
        if (selStart != selEnd) {
          memmove(buf + selStart, buf + selEnd, len - selEnd + 1);
          SetWindowTextA(hwnd, buf);
          SendMessageA(hwnd, EM_SETSEL, selStart, selStart);
        } else if (selStart < (DWORD)len) {
          memmove(buf + selStart, buf + selStart + 1, len - selStart);
          SetWindowTextA(hwnd, buf);
          SendMessageA(hwnd, EM_SETSEL, selStart, selStart);
        }
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
      }
      if (wParam == COPY_SELECTION_KEY) {
        bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
          bool shift_pressed = (GetKeyState(VK_SHIFT) & 0x8000);
          if (shift_pressed) {
            if (UI_IsListBoxSelected(lb)) {
              if (lb && lb->selected_index >=0 && lb->selected_index < lb->populated_count) strncpy(FlintContext_Get()->window.output_buffer, lb->populated_cache[lb->selected_index], sizeof(FlintContext_Get()->window.output_buffer) - 1);
                FlintContext_Get()->window.output_buffer[sizeof(FlintContext_Get()->window.output_buffer) - 1] = '\0';
            }
            int true_idx = -1;
            if (lb && lb->selected_index >= 0 && lb->selected_index < lb->populated_count) true_idx = lb->matching_indices[lb->selected_index];
            else true_idx = UI_LookupTrueIndex(FlintContext_Get()->window.output_buffer);
            if (FlintContext_Get()->prompt_callbacks.copy != NULL) {
              FlintContext_Get()->prompt_callbacks.copy(FlintContext_Get()->window.output_buffer, true_idx, 0);
            } else {
              Platform_CopyToClipboard(FlintContext_Get()->window.output_buffer);
            }
          } else if (ctrl_pressed) {
            if (UI_IsListBoxSelected(lb)) {
              if (lb && lb->selected_index >=0 && lb->selected_index < lb->populated_count) strncpy(FlintContext_Get()->window.output_buffer, lb->populated_cache[lb->selected_index], sizeof(FlintContext_Get()->window.output_buffer) - 1);
                FlintContext_Get()->window.output_buffer[sizeof(FlintContext_Get()->window.output_buffer) - 1] = '\0';
            }
            int true_idx = -1;
            if (lb && lb->selected_index >= 0 && lb->selected_index < lb->populated_count) true_idx = lb->matching_indices[lb->selected_index];
            else true_idx = UI_LookupTrueIndex(FlintContext_Get()->window.output_buffer);
              if (FlintContext_Get()->prompt_callbacks.copy != NULL) {
                FlintContext_Get()->prompt_callbacks.copy(FlintContext_Get()->window.output_buffer, true_idx, 1);
              } else {
                Platform_CopyToClipboard(FlintContext_Get()->window.output_buffer);
              }
          }
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

      if (ctrl_pressed) {
        if (wParam == VK_OEM_MINUS) {
          FontSize -= 1;
        } 
        if (wParam == VK_OEM_PLUS) {
          FontSize += 1;
        } 
      }
    } 
  if (msg == WM_CHAR) {
    if (wParam == 0x08 || wParam == 0x7F) {
      return 0; 
    }
  }
  
    return DefSubclassProc(hwnd, msg, wParam, lParam);
} 

LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData){
  static HBRUSH s_bg_brush = NULL;
    switch (uMsg) {
    case WM_NCDESTROY:
                      RemoveWindowSubclass(hWnd, ListBoxSubclassProc, uIdSubclass);
                        if (s_bg_brush) { DeleteObject(s_bg_brush); s_bg_brush = NULL; }
                            break;
    case WM_ERASEBKGND:
                       return TRUE;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hWnd, &ps);
      RECT rc; GetClientRect(hWnd, &rc);
      if (rc.right <= 0 || rc.bottom <= 0) { EndPaint(hWnd, &ps); return 0; }
      HDC memDC = CreateCompatibleDC(hdc);
      HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
      if (!memDC || !memBmp) {
        if (memDC) DeleteDC(memDC);
        if (memBmp) DeleteObject(memBmp);
        EndPaint(hWnd, &ps);
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
      }
      HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
      HWND hParent = GetParent(hWnd);
      RECT winRc; GetClientRect(hParent ? hParent : hWnd, &winRc);
      int winW = winRc.right - winRc.left;
      int winH = winRc.bottom - winRc.top;
      POINT lbOrigin = {0,0};
      if (hParent) { ClientToScreen(hWnd, &lbOrigin); ScreenToClient(hParent, &lbOrigin); }
      if (EnableShaders && ShaderPreset != 0 && winW > 0 && winH > 0) {
        RECT full = rc;
        Win32_DrawBackgroundSlice(memDC, &full, COLOR_BG, winW, winH, lbOrigin.x, lbOrigin.y);
      } else {
        if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
        FillRect(memDC, &rc, s_bg_brush);
      }
      SendMessage(hWnd, WM_PRINTCLIENT, (WPARAM)memDC, PRF_CLIENT);
      {
        int itemH = (int)SendMessageA(hWnd, LB_GETITEMHEIGHT, 0, 0);
        int count = (int)SendMessageA(hWnd, LB_GETCOUNT, 0, 0);
        if (itemH > 0 && count >= 0) {
          LONG itemsBottom = (LONG)count * itemH;
          if (itemsBottom < 0) itemsBottom = 0;
          if (itemsBottom < rc.bottom) {
            RECT empty = { rc.left, itemsBottom, rc.right, rc.bottom };
            RECT inter;
            if (empty.top < ps.rcPaint.bottom && empty.bottom > ps.rcPaint.top) {
              if (!IntersectRect(&inter, &empty, &ps.rcPaint)) inter = empty;
              if (EnableShaders && ShaderPreset != 0 && winW > 0 && winH > 0) {
                int gx = lbOrigin.x + empty.left;
                int gy = lbOrigin.y + empty.top;
                Win32_DrawBackgroundSlice(memDC, &empty, COLOR_BG, winW, winH, gx, gy);
              } else {
                if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
                FillRect(memDC, &empty, s_bg_brush);
              }
            }
          }
        } else if (count == 0) {
          if (EnableShaders && ShaderPreset != 0 && winW > 0 && winH > 0) {
            RECT full = rc;
            Win32_DrawBackgroundSlice(memDC, &full, COLOR_BG, winW, winH, lbOrigin.x, lbOrigin.y);
          } else {
            if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
            FillRect(memDC, &rc, s_bg_brush);
          }
        }
      }
      BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
      SelectObject(memDC, oldBmp);
      DeleteObject(memBmp);
      DeleteDC(memDC);
      EndPaint(hWnd, &ps);
      return 0;
    }
    default:
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }
  return 0;
}

LRESULT CALLBACK PopupWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if(msg==WM_KEYDOWN){ int mods=0; if(GetKeyState(VK_CONTROL)&0x8000) mods|=HOTKEY_MOD_CTRL; if(GetKeyState(VK_MENU)&0x8000) mods|=HOTKEY_MOD_ALT; if(GetKeyState(VK_SHIFT)&0x8000) mods|=HOTKEY_MOD_SHIFT; if(GetKeyState(VK_LWIN)&0x8000||GetKeyState(VK_RWIN)&0x8000) mods|=HOTKEY_MOD_SUPER; Platform_InputState_Update((int)wParam,true,mods); }
  if(msg==WM_KEYUP){ int mods=0; if(GetKeyState(VK_CONTROL)&0x8000) mods|=HOTKEY_MOD_CTRL; if(GetKeyState(VK_MENU)&0x8000) mods|=HOTKEY_MOD_ALT; if(GetKeyState(VK_SHIFT)&0x8000) mods|=HOTKEY_MOD_SHIFT; if(GetKeyState(VK_LWIN)&0x8000||GetKeyState(VK_RWIN)&0x8000) mods|=HOTKEY_MOD_SUPER; Platform_InputState_Update((int)wParam,false,mods); }
  ListBoxInfo* lb = GetLBForPopupWindow(hwnd);
  static HBRUSH s_bg_brush = NULL;
    static HBRUSH s_bg_gradient_brushes[128] = {0};
    static bool s_bg_gradient_ready = false;
    static HBRUSH s_sel_brush = NULL;
    (void)s_bg_gradient_brushes; (void)s_bg_gradient_ready;

    
    switch (msg) {
    case WM_ACTIVATE: {
        if (CloseOnLossOfFocus && LOWORD(wParam) == WA_INACTIVE) {
            FlintContext_Get()->window.output_buffer[0] = '\0';
            FlintContext_Get()->window.selection_complete = true;
        }
        break;
    }
    case WM_KILLFOCUS: {
        if (CloseOnLossOfFocus) {
            FlintContext_Get()->window.output_buffer[0] = '\0';
            FlintContext_Get()->window.selection_complete = true;
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
        if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
        return (LRESULT)s_bg_brush;
      }
    }
    case WM_CTLCOLORLISTBOX: {
      HDC hdc = (HDC)wParam;
      if (EnableShaders && ShaderPreset != 0) {
        SetTextColor(hdc, COLOR_TEXT);
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
      } else {
        SetTextColor(hdc, COLOR_TEXT);
        SetBkColor(hdc, COLOR_BG);
        if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
        return (LRESULT)s_bg_brush;
      }
    }
    case WM_CREATE: {
      if (g_win32_hMainFont) {
        DeleteObject(g_win32_hMainFont);
        g_win32_hMainFont = NULL;
      }
      g_win32_hMainFont = FH_TO_HFONT(Platform_CreateFont(FontSize, UsedFont));
      if (only_once) selection_feature = FlintContext_Get()->window.current_feature;
        only_once = false;
          break;
    }
    case WM_SHOWWINDOW: {
      if (wParam && g_pending_navigate >= 0) {
        int idx = g_pending_navigate;
          g_pending_navigate = -1;
          if (WH_TO_HWND(FlintContext_Get()->window.hListBox) != NULL && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hListBox))) {
            UI_NavigateTo(lb, FlintContext_Get()->window.hListBox, idx);
              UpdateWindow(WH_TO_HWND(FlintContext_Get()->window.hListBox));
          }
      }
      break;
    }
    case WM_COMMAND: {
      int wmId   = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);
        if (wmId == IDC_INPUT_BOX && wmEvent == EN_CHANGE) {
          if (GetKeyState(VK_UP) & 0x8000 ||
              GetKeyState(VK_DOWN) & 0x8000 ||
              GetKeyState(VK_RETURN) & 0x8000 ||
              GetKeyState(VK_ESCAPE) & 0x8000)
          {
            break;
          }
            if (WH_TO_HWND(FlintContext_Get()->window.hEditControl) != NULL && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hEditControl))) {
              char buf[sizeof(FlintContext_Get()->window.filter)];
              GetWindowTextA(WH_TO_HWND(FlintContext_Get()->window.hEditControl), buf, (int)sizeof(buf));
              strncpy(FlintContext_Get()->window.filter, buf, sizeof(FlintContext_Get()->window.filter) - 1);
              FlintContext_Get()->window.filter[sizeof(FlintContext_Get()->window.filter) - 1] = '\0';
              DWORD s = 0, e = 0;
              SendMessageA(WH_TO_HWND(FlintContext_Get()->window.hEditControl), EM_GETSEL, (WPARAM)&s, (LPARAM)&e);
              FlintContext_Get()->window.filter_cursor = (int)s;
              InvalidateRect(WH_TO_HWND(FlintContext_Get()->window.hEditControl), NULL, TRUE);
              UpdateWindow(WH_TO_HWND(FlintContext_Get()->window.hEditControl));
            }
            if (WH_TO_HWND(FlintContext_Get()->window.hEditControl) != NULL && WH_TO_HWND(FlintContext_Get()->window.hListBox) != NULL) {
              UI_Redraw(lb, FlintContext_Get()->window.hListBox);
            }
        }
      
        if (wmId == IDC_LIST_BOX && wmEvent == LBN_SELCHANGE) {
          break; 
        }
      
        break;
    }
    case WM_TIMER: {
      if (wParam == UI_DEBOUNCE_TIMER_ID) {
        KillTimer(hwnd, UI_DEBOUNCE_TIMER_ID);
          UI_RedrawNow(lb, FlintContext_Get()->window.hListBox);
          return 0;
      }
      break;
    }
    case WM_MEASUREITEM: {
      MEASUREITEMSTRUCT* pmis = (MEASUREITEMSTRUCT*)lParam;
        if (pmis->CtlID == IDC_LIST_BOX) {
          pmis->itemHeight = 40;
            return TRUE;
        }
      break;
    }
    case WM_DRAWITEM: {
      DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
        if (pdis->CtlID == IDC_LIST_BOX && pdis->itemID != -1) {
          BOOL isSelected = (pdis->itemID == (lb ? lb->selected_index : -1));
            if (pdis->itemID >= (lb ? lb->populated_count : 0)) {
              return TRUE;
            }
          if (RoundedCorners && CornerRadius > 0) {
            int R = 6;
            if (R > CornerRadius) R = CornerRadius;
            int t = 1;
            RECT inner = pdis->rcItem;
            inner.left += t; inner.top += t + 1; inner.right -= t; inner.bottom -= t + 1;
            if (inner.right <= inner.left || inner.bottom <= inner.top) { inner = pdis->rcItem; t = 0; }
            int innerR = R - 1; if (innerR < 0) innerR = 0;
            {
              RECT winRc; GetClientRect(hwnd, &winRc);
              int winW = winRc.right - winRc.left;
              int winH = winRc.bottom - winRc.top;
              POINT lbOrigin2 = {0,0};
              ClientToScreen(pdis->hwndItem, &lbOrigin2);
              ScreenToClient(hwnd, &lbOrigin2);
              int gx = lbOrigin2.x + pdis->rcItem.left;
              int gy = lbOrigin2.y + pdis->rcItem.top;
              if (EnableShaders && ShaderPreset != 0 && winW > 0 && winH > 0) {
                Win32_DrawBackgroundSlice(pdis->hDC, &pdis->rcItem, COLOR_BG, winW, winH, gx, gy);
              }
            }
            if (isSelected) {
              if (EnableShaders && ShaderPreset != 0) {
                Win32_DrawRoundedSelection(pdis->hDC, &inner, COLOR_SELECTED_TEXT, innerR);
              } else if (EnableShaders && ShaderPreset != 0) {
                Win32_DrawRoundedSelection(pdis->hDC, &inner, COLOR_SELECTED_TEXT, innerR);
              } else {
                Win32_FillRoundedRect(pdis->hDC, &inner, COLOR_SELECTED_TEXT, innerR);
              }
            } else {
              if (EnableShaders && ShaderPreset != 0) {
                RECT winRc; GetClientRect(hwnd, &winRc);
                int winW = winRc.right - winRc.left;
                int winH = winRc.bottom - winRc.top;
                POINT lbOrigin = {0,0};
                ClientToScreen(pdis->hwndItem, &lbOrigin);
                ScreenToClient(hwnd, &lbOrigin);
                int globalX = lbOrigin.x + inner.left;
                int globalY = lbOrigin.y + inner.top;
                if (winW <= 0) winW = pdis->rcItem.right - pdis->rcItem.left;
                if (winH <= 0) { RECT tr; GetClientRect(hwnd,&tr); winH = tr.bottom - tr.top; if(winH<=0) winH = pdis->rcItem.bottom - pdis->rcItem.top + 600; }
                Win32_FillRoundedRectSlice(pdis->hDC, &inner, COLOR_BG, winW, winH, globalX, globalY, innerR);
              } else {
                Win32_FillRoundedRect(pdis->hDC, &inner, COLOR_BG, innerR);
              }
            }
          } else if (EnableShaders && ShaderPreset != 0) {
            if (isSelected) {
              //temporary there are no selection shaders for windows it seems
              if (EnableShaders && ShaderPreset != 0) {
                Win32_DrawSelectionBackground(pdis->hDC, &pdis->rcItem, COLOR_SELECTED_TEXT);
              } else {
                Win32_DrawSelectionBackground(pdis->hDC, &pdis->rcItem, COLOR_SELECTED_TEXT);
              }
            } else {
              RECT winRc; GetClientRect(hwnd, &winRc);
              int winW = winRc.right - winRc.left;
              int winH = winRc.bottom - winRc.top;
              POINT lbOrigin = {0,0};
              ClientToScreen(pdis->hwndItem, &lbOrigin);
              ScreenToClient(hwnd, &lbOrigin);
              int globalX = lbOrigin.x + pdis->rcItem.left;
              int globalY = lbOrigin.y + pdis->rcItem.top;
              if (winW <= 0) winW = pdis->rcItem.right - pdis->rcItem.left;
              if (winH <= 0) { RECT tr2; GetClientRect(hwnd,&tr2); winH = tr2.bottom - tr2.top; if(winH<=0) winH = pdis->rcItem.bottom - pdis->rcItem.top + 600; }
              Win32_DrawBackgroundSlice(pdis->hDC, &pdis->rcItem, COLOR_BG, winW, winH, globalX, globalY);
            }
          } else {
            if (isSelected) {
              if (!s_sel_brush) s_sel_brush = CreateSolidBrush(COLOR_SELECTED_TEXT);
              FillRect(pdis->hDC, &pdis->rcItem, s_sel_brush);
            } else {
              if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
              FillRect(pdis->hDC, &pdis->rcItem, s_bg_brush);
            }
          }
          if (UI_IsListBoxSelected(lb)) {
                  char measureBuffer[512];
                    strncpy(measureBuffer, lb->populated_cache[pdis->itemID], sizeof(measureBuffer) - 1);
                    measureBuffer[sizeof(measureBuffer) - 1] = '\0';
                    char* pos = strchr(measureBuffer, '|');
                    if (pos) {
                      *pos = '\0';
                    }
                  if (!FullTextNeedsKeyPress) {
                    if (isSelected && strlen(measureBuffer) >= 70) {
                      if (g_last_selected_index != pdis->itemID || WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) == NULL) {
                        g_last_selected_index = pdis->itemID;
                          PostMessageA(hwnd, WM_PREVIEW_SELECTION, 0, (LPARAM)pdis->itemID);
                      }
                    } else {
                      if (g_last_selected_index == pdis->itemID) {
                        g_last_selected_index = -1;
                          PostMessageA(hwnd, WM_PREVIEW_SELECTION, 0, (LPARAM)-1);
                      }
                    }
                  } else {
                    if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
                      KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
                        DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
                        FlintContext_Get()->window.hDisplayMessage = WH_NULL;
                    }
                  } 
                } 
           
            const char* textBuffer = "";
            if (pdis->itemID < (lb ? lb->populated_count : 0)) {
              int raw = lb->matching_indices[pdis->itemID];
                if (g_alias_display_active && g_alias_display_count > 0 && raw >= 0 && raw < g_alias_display_count && g_alias_display[raw][0] != '\0') {
                  textBuffer = g_alias_display[raw];
                } else {
                  textBuffer = lb->populated_cache[pdis->itemID];
                }
            }
          
            if (pdis->itemID == -1 || pdis->itemID >= (lb ? lb->populated_count : 0)) {
              return TRUE;
            }
          
            
            SetBkMode(pdis->hDC, TRANSPARENT);
            RECT textRect = pdis->rcItem;
            textRect.left += 10;
          
            Platform_SetPreRenderStateWin32(textBuffer, &textRect);
            Platform_SetDrawContextWin32(pdis->hDC);
            // temporary
            // if (!fl_prerender()) return TRUE;
            if (FlintContext_Get()->prompt_callbacks.render != NULL) {
              return TRUE;
            } else {
              HFONT hOldMainFont = (HFONT)SelectObject(pdis->hDC, g_win32_hMainFont);
                if (!isSelected) {
                    unsigned long ncol, bcol, pcol;
                    int orig_idx = (int)pdis->itemID;
                    int orig_cnt = lb ? lb->populated_count : 0;
                    if (lb && (int)pdis->itemID >= 0 && pdis->itemID < (UINT)lb->populated_count) {
                      orig_idx = lb->matching_indices[pdis->itemID];
                      orig_cnt = lb->last_item_count > 0 ? lb->last_item_count : lb->populated_count;
                    }
                    Platform_GetItemColors(orig_idx, orig_cnt, &ncol, &bcol, &pcol);
                    SetTextColor(pdis->hDC, ncol);
                } else {
                    SetTextColor(pdis->hDC, COLOR_TEXT);
                }
                DrawTextA(pdis->hDC, textBuffer, -1, &textRect, DT_SINGLELINE | DT_VCENTER);
                const char* alias_pos = strstr(textBuffer, " [");
                if (alias_pos) {
                  SIZE sz;
                    GetTextExtentPoint32A(pdis->hDC, textBuffer, (int)(alias_pos - textBuffer), &sz);
                    RECT aliasRect = textRect;
                    aliasRect.left += sz.cx;
                    SetTextColor(pdis->hDC, COLOR_ALIAS_TEXT);
                    DrawTextA(pdis->hDC, alias_pos, -1, &aliasRect, DT_SINGLELINE | DT_VCENTER);
                }
              SelectObject(pdis->hDC, hOldMainFont);
            }
          
            return TRUE;
        }
      break;
    }
    case WM_PAINT: {
      PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        if (EnableShaders && ShaderPreset != 0) {
          Win32_DrawBackground(hdc, &rect);
        } else {
          if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
            FillRect(hdc, &ps.rcPaint, s_bg_brush);
        }
        if (FlintContext_Get()->prompt_callbacks.render) {
            extern PromptState* Platform_GetWin32PromptState(void);
            PromptState *state = Platform_GetWin32PromptState();
            if (state) {
                fl_editbox_reset_placed();
                Platform_SetDrawContextWin32(hdc);
                FlintContext_Get()->prompt_callbacks.render(state);
            }
        }
      if (g_vim_mode != NULL && VimModeText) {
        wchar_t wmode[256];
          MultiByteToWideChar(CP_UTF8, 0, g_vim_mode, -1, wmode, 256);
          HFONT hOldFont = (HFONT)SelectObject(hdc, g_win32_hMainFont);
          SetBkMode(hdc, TRANSPARENT);
          if (g_current_mode == MODE_NORMAL) {
            SetTextColor(hdc,COLOR_NORMAL_MODE_TEXT);
          } else if (g_current_mode == MODE_INSERT) {
            SetTextColor(hdc,COLOR_INSERT_MODE_TEXT);
          } else {
            SetTextColor(hdc,COLOR_VISUAL_MODE_TEXT);
          }
          SIZE sz;
          GetTextExtentPoint32W(hdc, wmode, (int)wcslen(wmode), &sz);
          RECT modeRect;
          modeRect.left = rect.right - sz.cx - 20;
          modeRect.top = rect.bottom - sz.cy - 10;
          modeRect.right = rect.right - 10;
          modeRect.bottom = modeRect.top + sz.cy;
          DrawTextW(hdc, wmode, -1, &modeRect, DT_LEFT);
          SelectObject(hdc, hOldFont);
      }
      if (FlintContext_Get()->window.title != NULL && strlen(FlintContext_Get()->window.title) > 0 && WindowTitleText) {
        wchar_t wtitle[256];
          MultiByteToWideChar(CP_UTF8, 0, FlintContext_Get()->window.title, -1, wtitle, 256);
          HFONT hOldFont = (HFONT)SelectObject(hdc, g_win32_hMainFont);
          SetBkMode(hdc, TRANSPARENT);
          SIZE sz;
          GetTextExtentPoint32W(hdc, wtitle, (int)wcslen(wtitle), &sz);
          RECT titleRect;
          titleRect.left = 15;
          titleRect.top = rect.bottom - sz.cy - 10;
          titleRect.right = titleRect.left + sz.cx + 10;
          titleRect.bottom = titleRect.top + sz.cy;
          COLORREF titleColor;
          if ((lb ? lb->populated_count : 0) < 25) {
            titleColor = RGB(0, 255, 0);
          } else if ((lb ? lb->populated_count : 0) < 50) {
            titleColor = RGB(100, 255, 0);
          } else if ((lb ? lb->populated_count : 0) < 75) {
            titleColor = RGB(200, 255, 0);
          } else if ((lb ? lb->populated_count : 0) < 100) {
            titleColor = RGB(255, 255, 0);
          } else if ((lb ? lb->populated_count : 0) < 125) {
            titleColor = RGB(255, 200, 0);
          } else if ((lb ? lb->populated_count : 0) < 150) {
            titleColor = RGB(255, 150, 0);
          } else if ((lb ? lb->populated_count : 0) < 175) {
            titleColor = RGB(255, 100, 0);
          } else if ((lb ? lb->populated_count : 0) < 200) {
            titleColor = RGB(255, 50, 0);
          } else {
            titleColor = RGB(255, 0, 0);
          }
          SetTextColor(hdc, titleColor);
          DrawTextW(hdc, wtitle, -1, &titleRect, DT_LEFT | DT_TOP);
          SelectObject(hdc, hOldFont);
      }
        EndPaint(hwnd, &ps);
        break;
    }
                         break;              
    case WM_ERASEBKGND: {
      return TRUE;
    }
    case WM_DESTROY: {
      static const WORD held_modifiers[] = { VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL,
        VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN };
        if (!KeepSelectionAfterClose)  {
          last_index = 0;
            if (lb) lb->selected_index = 0;
        }
      
        extern EditorMode g_last_mode;
        g_last_mode = g_current_mode;
        selection_feature = FlintContext_Get()->window.current_feature;
        for (int i = 0; i < (int)ARRAY_LEN(held_modifiers); i++) {
          if (GetAsyncKeyState(held_modifiers[i]) & 0x8000) {
            INPUT up = {0};
              up.type = INPUT_KEYBOARD;
              up.ki.wVk = held_modifiers[i];
              up.ki.dwFlags = KEYEVENTF_KEYUP;
              SendInput(1, &up, sizeof(INPUT));
          }
        }
      if (s_sel_brush) { DeleteObject(s_sel_brush); s_sel_brush = NULL; }
        rainbow_init = false;
        break;
    }
    default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
              
    }
  return 0;
}
