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
#include "platform/widget.h"
#include "platform/render_thread.h"

#define MSG_AUTO_WRAP_MAX_W 900
#define MSG_AUTO_WRAP_MAX_H 600

#define MSG_TEXT_INSET_X 15
#define MSG_TEXT_INSET_Y 15
#define MSG_BOTTOM_CLIP 5

static int g_win32_msg_w = 0;
static int g_win32_msg_h = 0;

static wchar_t* msg_widen(const char* in, int* out_len) {
    if (out_len) *out_len = 0;
    if (!in || !in[0]) return NULL;
    int need = MultiByteToWideChar(CP_UTF8, 0, in, -1, NULL, 0);
    if (need <= 0) return NULL;
    wchar_t* w = (wchar_t*)malloc((size_t)need * sizeof(wchar_t));
    if (!w) return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, in, -1, w, need) <= 0) { free(w); return NULL; }
    if (out_len) *out_len = need - 1;
    return w;
}

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
            textRect.left += MSG_TEXT_INSET_X;
            textRect.right -= MSG_TEXT_INSET_X;
            textRect.top += MSG_TEXT_INSET_Y;
            textRect.bottom -= MSG_TEXT_INSET_Y;

            int wlen = 0;
            wchar_t* wide = msg_widen(data->message, &wlen);
            if (wide) {
              RECT calcRect = textRect;
              DrawTextW(hdc, wide, wlen, &calcRect, DT_CENTER | DT_WORDBREAK | DT_CALCRECT);

              int textHeight = calcRect.bottom - calcRect.top;
              int availableHeight = rect.bottom - rect.top;
              textRect.top = (availableHeight - textHeight) / 2;
              textRect.bottom = textRect.top + textHeight;
              if (textRect.bottom > rect.bottom - MSG_BOTTOM_CLIP) textRect.bottom = rect.bottom - MSG_BOTTOM_CLIP;

              DrawTextW(hdc, wide, wlen, &textRect, DT_CENTER | DT_WORDBREAK);
              free(wide);
            }
        }
      
        if (hOldFont) SelectObject(hdc, hOldFont);
          EndPaint(hwnd, &ps);
            return 0;
    }
    case WM_TIMER: {
      if (wParam == 999 && data) {
        if (g_win32_timeout == 6969.0) {
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

void Platform_DisplayMessage(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, double timeout, ColorRef color) {
  if (WH_TO_HWND(FlintContext_Get()->window.hGlobalTrayWnd) != NULL && GetCurrentThreadId() != g_win32_main_thread_id) {
    DisplayMessageParams params = { message, centerX, centerY, maxWidth, maxHeight, timeout, color };
    SendMessageA(WH_TO_HWND(FlintContext_Get()->window.hGlobalTrayWnd), WM_DISPLAY_MESSAGE, 0, (LPARAM)&params);
    return;
  }
  Platform_CreateDisplayMessageWindow(message, centerX, centerY, maxWidth, maxHeight, timeout, color);
log_info("Platform_DisplayMessage message='%s' centerX=%d centerY=%d maxWidth=%d maxHeight=%d timeout=%g color=%p", message ? message : "", centerX, centerY, maxWidth, maxHeight, timeout, color);;
}

static char g_hover_key[64] = {0};

#define HOVER_WIDGET_MAX 8

typedef struct {
  char key[64];
  PlatformWidget* w;
  void (*render)(void* ud);
  void* ud;
  void (*release)(void* ud);
  void* release_ud;
  int fps;
  bool no_bg;
  unsigned long bg_hex;
  int x, y, w_px, h_px;
  bool dead;
} HoverWidgetSlot;

static HoverWidgetSlot g_hover_widgets[HOVER_WIDGET_MAX];

static int HoverWidgetFindLocked(const char* key) {
  for (int i = 0; i < HOVER_WIDGET_MAX; i++) {
    if (g_hover_widgets[i].w && !g_hover_widgets[i].dead && strcmp(g_hover_widgets[i].key, key) == 0) return i;
  }
  return -1;
}

static void HoverWidgetFrame(void* ud) {
  HoverWidgetSlot* s = (HoverWidgetSlot*)ud;
  if (!s || !s->w || s->dead) return;
  if (s->render) s->render(s->ud);
}

void Platform_HoverWidgetHide(const char* key) {
  if (!key || !key[0]) return;
  int idx = HoverWidgetFindLocked(key);
  if (idx < 0) return;
  PlatformWidget* w = g_hover_widgets[idx].w;
  void (*release)(void*) = g_hover_widgets[idx].release;
  void* release_ud = g_hover_widgets[idx].release_ud;
  memset(&g_hover_widgets[idx], 0, sizeof(g_hover_widgets[idx]));
  if (w) {
    Platform_Widget_SetFrameCallback(w, NULL, NULL);
    Platform_Widget_Destroy(w);
  }
  if (release) release(release_ud);
  if (!Platform_RenderThread_IsCurrentThread()) Platform_RenderThread_Sync();
}

void Platform_HoverWidgetShow(const char* key, void (*render)(void* ud), void* ud, int x, int y,
                              int w, int h, int fps, bool no_bg, unsigned long bg_hex,
                              void (*release)(void* ud), void* release_ud) {
  if (!key || !key[0] || w <= 0 || h <= 0) return;
  int idx = HoverWidgetFindLocked(key);
  if (idx >= 0) {
    HoverWidgetSlot* s = &g_hover_widgets[idx];
    void (*old_release)(void*) = s->release;
    void* old_release_ud = s->release_ud;
    s->render = render;
    s->ud = ud;
    s->release = release;
    s->release_ud = release_ud;
    bool geom_changed = (s->x != x || s->y != y || s->w_px != w || s->h_px != h);
    s->x = x; s->y = y; s->w_px = w; s->h_px = h;
    PlatformWidget* pw = s->w;
    int ax = x - w / 2;
    int ay = y - h / 2;
    if (geom_changed) {
      Platform_Widget_Resize(pw, w, h);
      Platform_Widget_Move(pw, ax, ay);
    }
    Platform_Widget_Invalidate(pw);
    if (old_release && old_release != release) old_release(old_release_ud);
    return;
  }
  idx = -1;
  for (int i = 0; i < HOVER_WIDGET_MAX; i++) {
    if (!g_hover_widgets[i].w) { idx = i; break; }
  }
  if (idx < 0) {
    if (release) release(release_ud);
    return;
  }
  HoverWidgetSlot* s = &g_hover_widgets[idx];
  memset(s, 0, sizeof(*s));
  snprintf(s->key, sizeof(s->key), "%s", key);
  s->render = render;
  s->ud = ud;
  s->release = release;
  s->release_ud = release_ud;
  s->fps = fps;
  s->no_bg = no_bg;
  s->bg_hex = bg_hex;
  s->x = x; s->y = y; s->w_px = w; s->h_px = h;

  PlatformWidget* pw = Platform_Widget_Create(x - w / 2, y - h / 2, w, h, fps, no_bg, NULL, key);
  if (!pw) {
    memset(s, 0, sizeof(*s));
    if (release) release(release_ud);
    return;
  }
  s->w = pw;
  Platform_Widget_SetBgColor(pw, no_bg ? 0 : bg_hex);
  Platform_Widget_SetNoBg(pw, no_bg);
  Platform_Widget_SetFrameCallback(pw, HoverWidgetFrame, s);
}

void Platform_HoverMessageRelease(const char* key) {
  if (!key || !key[0]) return;
  if (strcmp(g_hover_key, key) != 0) return;
  HWND hw = WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage);
  if (hw && IsWindow(hw)) {
    KillTimer(hw, 999);
    FlintContext_Get()->window.hDisplayMessage = WH_NULL;
    DestroyWindow(hw);
  }
  g_hover_key[0] = '\0';
}

void Platform_HoverMessage(const char* key, const char* message, int centerX, int centerY, int w, int h, int maxWidth, int maxHeight, ColorRef color) {
  if (!message || !message[0] || !key || !key[0]) return;

  HWND existing = WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage);
  if (g_hover_key[0] && strcmp(g_hover_key, key) == 0 && existing && IsWindow(existing)) {
    SetWindowPos(existing, HWND_TOPMOST, centerX - g_win32_msg_w / 2, centerY - g_win32_msg_h / 2,
                 g_win32_msg_w, g_win32_msg_h, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetWindowTextA(existing, message);
    InvalidateRect(existing, NULL, TRUE);
    return;
  }

  int winWidth = w;
  int winHeight = h;
  if (!(winWidth > 0 && winHeight > 0)) {
    FontHandle msgFont = Platform_CreateFont(FontSize, UsedFont);
    int textWidth = 0, textHeight = 0;
    Platform_GetTextExtents(msgFont, message, maxWidth - 30, &textWidth, &textHeight);
    winWidth = textWidth + 30;
    winHeight = textHeight + 30;
    if (winWidth > maxWidth) winWidth = maxWidth;
    if (winHeight > maxHeight) winHeight = maxHeight;
  }

  Platform_HoverMessageRelease(g_hover_key);
  Platform_CreateDisplayMessageWindow(message, centerX, centerY, maxWidth, maxHeight, 0.0, color);
  g_win32_msg_w = winWidth;
  g_win32_msg_h = winHeight;
  snprintf(g_hover_key, sizeof(g_hover_key), "%s", key);

  HWND hw = WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage);
  if (hw && IsWindow(hw)) {
    KillTimer(hw, 999);
    g_win32_timeout = 0.0;
    SetWindowPos(hw, HWND_TOPMOST, centerX - winWidth / 2, centerY - winHeight / 2,
                 winWidth, winHeight, SWP_SHOWWINDOW);
    InvalidateRect(hw, NULL, TRUE);
  }
}

void Platform_DisplayMessageAnchored(const char* message, int rect_x, int rect_y, int rect_w, int rect_h, int h_slot, int v_slot, int gap, int outward, int maxWidth, int maxHeight, double timeout, ColorRef color) {
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

void Platform_CreateDisplayMessageWindow(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, double timeout, ColorRef color) {
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
  g_win32_timeout = timeout * 20.0;

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
  log_info("Platform_CreateDisplayMessageWindow message='%s' centerX=%d centerY=%d maxWidth=%d maxHeight=%d timeout=%g", message ? message : "", centerX, centerY, maxWidth, maxHeight, timeout);
  return;
}
