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
#include "helpers/helpers_ui.h"
#include "platform/win32/popup_helpers_win32.h"
#include "platform/win32/win32_state.h"
#include "platform/win32/common_ui_win32.h"

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
          
          if (EnableShaders && ShaderPreset != 0) win32_draw_background(hdc, &rect);
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
        if (g_win32_timeout == 6969) {
          return 0;
        }
        
          if (g_win32_timeout > 0) {
            g_win32_timeout--;
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

void Platform_DisplayMessage(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout, ColorRef color) {
  if (WH_TO_HWND(FlintContext_Get()->window.hGlobalTrayWnd) != NULL && GetCurrentThreadId() != g_win32_main_thread_id) {
    DisplayMessageParams params = { message, centerX, centerY, maxWidth, maxHeight, timeout, color };
    SendMessageA(WH_TO_HWND(FlintContext_Get()->window.hGlobalTrayWnd), WM_DISPLAY_MESSAGE, 0, (LPARAM)&params);
    return;
  }
  Platform_CreateDisplayMessageWindow(message, centerX, centerY, maxWidth, maxHeight, timeout, color);
log_info("Platform_DisplayMessage message='%s' centerX=%d centerY=%d maxWidth=%d maxHeight=%d timeout=%d color=%p", message ? message : "", centerX, centerY, maxWidth, maxHeight, timeout, color);;
}

void Platform_CreateDisplayMessageWindow(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout, ColorRef color) {
  HINSTANCE hInstance = GetModuleHandle(NULL);

  DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
  KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage),999);

  FontHandle msgFont = Platform_CreateFont(FontSize, UsedFont);
  int textWidth = 0, textHeight = 0;
  Platform_GetTextExtents(msgFont, message, maxWidth - 30, &textWidth, &textHeight);

  int padding = 30;
  int winWidth = textWidth + padding;
  int winHeight = textHeight + padding;

  if (winWidth > maxWidth) winWidth = maxWidth;
  if (winHeight > maxHeight) winHeight = maxHeight;

  FadeWindowData* pWndData = (FadeWindowData*)malloc(sizeof(FadeWindowData));
  if (!pWndData) {
    Platform_DestroyFont(msgFont);
    return;
  }

  pWndData->message = _strdup(message);
  pWndData->hFont = msgFont;
  pWndData->alpha = WindowOpacity;
  pWndData->color = color;
  g_win32_timeout = timeout * 20;

  int topLeftX = centerX - (winWidth / 2);
  int topLeftY = centerY - (winHeight / 2);

  HWND hPopupWnd = CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE, 
      "DisplayMessageModalClass", NULL, WS_POPUP, 
      topLeftX, topLeftY, winWidth, winHeight, NULL, NULL, hInstance, pWndData
      );

  if (hPopupWnd == NULL) {
    Platform_DestroyFont(pWndData->hFont);
    free(pWndData);
    return;
  }
  win32_apply_rounded_corners(hPopupWnd, winWidth, winHeight);

  SetLayeredWindowAttributes(hPopupWnd, 0, (BYTE)pWndData->alpha, LWA_ALPHA);

  ShowWindow(hPopupWnd, SW_SHOWNOACTIVATE);
  UpdateWindow(hPopupWnd);

  SetTimer(hPopupWnd, 999, 50, NULL);
  log_info("Platform_CreateDisplayMessageWindow message='%s' centerX=%d centerY=%d maxWidth=%d maxHeight=%d timeout=%d", message ? message : "", centerX, centerY, maxWidth, maxHeight, timeout);
  return; 
}
