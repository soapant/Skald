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
#include "platform/win32/subclasses_win32.h"
#include "wingdi.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_file.h"
#include "helpers/helpers_ui.h"
#include "platform/win32/popup_helpers_win32.h"

LRESULT CALLBACK DisplayMessageProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  FadeWindowData* data = (FadeWindowData*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    FlintContext_Get()->window.hDisplayMessage = WH_FROM_HWND(hwnd);
    
    switch (msg) {
    case WM_NCCREATE: {
      CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    case WM_PAINT: {
      if (!data) break;
        PAINTSTRUCT ps;
          HDC hdc = BeginPaint(hwnd, &ps);
          
          RECT rect;
          GetClientRect(hwnd, &rect);
          
          if (EnableShaders && ShaderPreset != 0) Win32_DrawBackground(hdc, &rect);
          else { HBRUSH hBackgroundBrush = CreateSolidBrush(COLOR_BG); FillRect(hdc, &rect, hBackgroundBrush); DeleteObject(hBackgroundBrush); }
          
          SetBkMode(hdc, TRANSPARENT);
          SetTextColor(hdc, data->color ? data->color : COLOR_TEXT);
      
        HFONT hOldFont = NULL;
        if (FH_TO_HFONT(data->hFont)) {
          hOldFont = (HFONT)SelectObject(hdc, FH_TO_HFONT(data->hFont));
        }
      
        if (data->message) {
          RECT textRect = rect;
            textRect.left += 15;  
            textRect.right -= 15;
            textRect.top += 10;
            textRect.bottom -= 10;
            
            RECT calcRect = textRect;
            DrawTextA(hdc, data->message, -1, &calcRect, DT_CENTER | DT_WORDBREAK | DT_CALCRECT);
            
            int textHeight = calcRect.bottom - calcRect.top;
            int availableHeight = rect.bottom - rect.top;
            textRect.top = (availableHeight - textHeight) / 2;
            textRect.bottom = textRect.top + textHeight;
            
            DrawTextA(hdc, data->message, -1, &textRect, DT_CENTER | DT_WORDBREAK);
        }
      
        if (hOldFont) SelectObject(hdc, hOldFont);
          EndPaint(hwnd, &ps);
            return 0;
    }
    case WM_TIMER: {
      if (wParam == 999 && data) {
        if (FlintContext_Get()->g_timeout == 6969) {
          return 0;
        }
        
          if (FlintContext_Get()->g_timeout > 0) {
            FlintContext_Get()->g_timeout--;
              return 0; 
          }
        
          data->alpha -= 12; 
          
          if (data->alpha <= 0) {
            KillTimer(hwnd, 999);
              FlintContext_Get()->window.hDisplayMessage = WH_NULL; 
              DestroyWindow(hwnd);
          } else {
            SetLayeredWindowAttributes(hwnd, 0, (BYTE)data->alpha, LWA_ALPHA);
          }
      }
      return 0;
    }
    case WM_NCDESTROY: {
      if (data) {
        if (data->message) free((void*)data->message);
          if (FH_TO_HFONT(data->hFont)) Platform_DestroyFont(data->hFont);
            free(data);
              SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
      }
      break;
    }
    }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}
