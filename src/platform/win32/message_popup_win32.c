#include <windows.h>
#include "deps/log.h"
#include "core/context.h"
#include "core/window_manager.h"
#include <commctrl.h>
#include <shellapi.h>
#include "config.h"
#include "platform/ui.h"
#include "platform/win32/subclasses_win32.h"
#include "wingdi.h"
#include "platform/platform.h"
#include "platform/win32/win32_state.h"

#define MSG_AUTO_WRAP_MAX_W 900
#define MSG_AUTO_WRAP_MAX_H 600

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

static FadeWindowData* s_msg_show_data = NULL;

bool Platform_MessagePopupShowWindow(Popup* self, int x, int y, int w, int h) {
  (void)self;
  if (!s_msg_show_data) { log_error("Platform_MessagePopupShowWindow null payload"); return false; }
  HINSTANCE hInstance = GetModuleHandle(NULL);
  HWND hPopupWnd = CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
      "DisplayMessageModalClass", NULL, WS_POPUP,
      x, y, w, h, NULL, NULL, hInstance, s_msg_show_data
      );
  if (hPopupWnd == NULL) {
    log_error("CreateWindowExA failed: %lu", GetLastError());
    return false;
  }
  s_msg_show_data = NULL;
  win32_apply_rounded_corners(hPopupWnd, w, h);
  SetLayeredWindowAttributes(hPopupWnd, 0, (BYTE)WindowOpacity, LWA_ALPHA);
  ShowWindow(hPopupWnd, SW_SHOWNOACTIVATE);
  UpdateWindow(hPopupWnd);
  SetTimer(hPopupWnd, 999, 50, NULL);
  return true;
}

void Platform_MessagePopupHideWindow(Popup* self) {
  (void)self;
  HWND hMsg = WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage);
  if (hMsg != NULL && IsWindow(hMsg)) {
    DestroyWindow(hMsg);
    FlintContext_Get()->window.hDisplayMessage = WH_NULL;
  }
}

void Platform_MessagePopupRender(Popup* self) {
  (void)self;
}

void Platform_MessagePopupDestroyWindow(Popup* self) {
  Platform_MessagePopupHideWindow(self);
}

bool Platform_MessagePopupOnKey(Popup* self, const PopupKeyEvent* key) {
  (void)self;
  (void)key;
  return false;
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

void Platform_DisplayMessageAnchored(const char* message, int rect_x, int rect_y, int rect_w, int rect_h, int h_slot, int v_slot, int gap, int outward, int maxWidth, int maxHeight, int timeout, ColorRef color) {
  if (!message || !message[0]) return;
  if (maxWidth <= 0) maxWidth = MSG_AUTO_WRAP_MAX_W;
  if (maxHeight <= 0) maxHeight = MSG_AUTO_WRAP_MAX_H;

  FontHandle msgFont = Platform_CreateFont(FontSize, UsedFont);
  int textWidth = 0, textHeight = 0;
  Platform_GetTextExtents(msgFont, message, maxWidth - 30, &textWidth, &textHeight);
  Platform_DestroyFont(msgFont);

  int winWidth = textWidth + 30;
  int winHeight = textHeight + 30;
  if (winWidth > maxWidth) winWidth = maxWidth;
  if (winHeight > maxHeight) winHeight = maxHeight;

  int centerX;
  if (h_slot == 0) centerX = outward ? rect_x - gap - winWidth / 2 : rect_x + gap + winWidth / 2;
  else if (h_slot == 2) centerX = outward ? rect_x + rect_w + gap + winWidth / 2 : rect_x + rect_w - gap - winWidth / 2;
  else centerX = rect_x + rect_w / 2;
  int centerY;
  if (v_slot == 0) centerY = outward ? rect_y - gap - winHeight / 2 : rect_y + gap + winHeight / 2;
  else if (v_slot == 2) centerY = outward ? rect_y + rect_h + gap + winHeight / 2 : rect_y + rect_h - gap - winHeight / 2;
  else centerY = rect_y + rect_h / 2;

  Platform_DisplayMessage(message, centerX, centerY, winWidth, winHeight, timeout, color);
}

void Platform_CreateDisplayMessageWindow(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout, ColorRef color) {
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

  s_msg_show_data = pWndData;
  if (!Platform_MessagePopupShowWindow(NULL, topLeftX, topLeftY, winWidth, winHeight)) {
    s_msg_show_data = NULL;
    Platform_DestroyFont(pWndData->hFont);
    free((void*)pWndData->message);
    free(pWndData);
    return;
  }
  log_info("Platform_CreateDisplayMessageWindow message='%s' centerX=%d centerY=%d maxWidth=%d maxHeight=%d timeout=%d", message ? message : "", centerX, centerY, maxWidth, maxHeight, timeout);
  return;
}
