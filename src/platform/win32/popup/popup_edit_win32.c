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
#include "platform/wm_platform.h"

static void EditPopup_Close(bool confirmed) {
    WindowManager* wm = WindowManager_Get();
    if (!wm) return;
    Popup* top = WindowManager_GetTopPopup(wm);
    if (top && top->type == POPUP_EDIT) {
        WMEditPopupData* ed = (WMEditPopupData*)top;
        if (confirmed && ed->on_confirm) {
            ed->on_confirm(ed->text ? ed->text : "", ed->user_data);
        }
        ed->confirmed = confirmed;
    }
    FlintContext_Get()->window.selection_complete = false;
    WindowManager_PopPopup(wm);
}

static bool EditPopupWindow_OnKey(HWND hWnd, WMEditPopupData* data, const PopupKeyEvent* key) {
    if (data->base.vtable && data->base.vtable->on_key && data->base.vtable->on_key(&data->base, key)) return true;
    return false;
}

LRESULT CALLBACK EditPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  ListBoxInfo* lb = NULL;
  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) lb = GetLBFromPopup(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
  WMEditPopupData* data = (WMEditPopupData*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_NCCREATE: {
      CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }

    case WM_CREATE: {
      if (!data) break;
        SetFocus(hwnd);
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
      Platform_EditPopupRender((Popup*)data);
      return 0;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
      if (!data) break;
      PopupKeyEvent key;
      memset(&key, 0, sizeof(key));
      key.vk = (int)wParam;
      key.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
      key.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
      key.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
      {
        extern void win32_fill_popup_key_text(PopupKeyEvent* key);
        win32_fill_popup_key_text(&key);
      }
      if (EditPopupWindow_OnKey(hwnd, data, &key)) {
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
      }
      if (msg == WM_SYSKEYDOWN) return 0;
      break;
    }

    case WM_CHAR: {
      if (!data) break;
      if (wParam == 0x0D || wParam == 0x1B) return 0;
      if (wParam < 0x20 && wParam != 0x08 && wParam != 0x09) return 0;
      if (IsNormalMode()) return 0;
      if (wParam >= 0x20 && wParam <= 0x7E) {
        char ins[2] = { (char)wParam, '\0' };
        PopupKeyEvent key;
        memset(&key, 0, sizeof(key));
        key.vk = 0;
        key.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        key.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        key.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        key.text[0] = ins[0];
        key.text_len = 1;
        if (data->base.vtable && data->base.vtable->on_key && data->base.vtable->on_key(&data->base, &key)) {
          InvalidateRect(hwnd, NULL, FALSE);
          return 0;
        }
      }
      break;
    }

    case WM_ERASEBKGND: {
      return 1;
    }

    case WM_DESTROY: {
        if (data) {
              SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
        }
        ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, WH_NULL);
        last_index = lb ? lb->selected_index : -1;
        UpdateTextCaret();
        break;
    }
    }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}
