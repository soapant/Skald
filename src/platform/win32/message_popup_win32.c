#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "config_values.h"
#include "core/types/keys.h"
#include "core/types/color.h"
#include "core/types/window.h"
#include "deps/log.h"
#include "core/window_manager.h"
#include <commctrl.h>
#include <shellapi.h>
#include "helpers/win32/subclasses_win32.h"
#include "helpers/win32/common_ui_win32.h"
#include "helpers/win32/bg_image_win32.h"
#include "wingdi.h"
#include "platform/platform.h"
#include "helpers/win32/win32_state.h"
#include "helpers/helpers_owner.h"
#include "helpers/helpers_ui.h"

#define MSG_AUTO_WRAP_MAX_W 900
#define MSG_AUTO_WRAP_MAX_H 600

#define MSG_TEXT_INSET_X 15
#define MSG_TEXT_INSET_Y 15
#define MSG_BOTTOM_CLIP 5

static int g_win32_msg_w = 0;
static int g_win32_msg_h = 0;
static HWND g_hover_msg_hwnd = NULL;
static HWND g_last_created_msg_hwnd = NULL;

#define MSG_SLOT_MAX 24
static HWND g_msg_slots[MSG_SLOT_MAX];
static int g_msg_slot_count = 0;

static int win32_msg_slot_find(HWND hwnd) {
  for (int i = 0; i < g_msg_slot_count; i++) {
    if (g_msg_slots[i] == hwnd)
      return i;
  }
  return -1;
}

static void win32_msg_slot_unregister(HWND hwnd) {
  int idx = win32_msg_slot_find(hwnd);
  if (idx < 0)
    return;
  for (int i = idx; i < g_msg_slot_count - 1; i++)
    g_msg_slots[i] = g_msg_slots[i + 1];
  g_msg_slot_count--;
  if (hwnd == g_hover_msg_hwnd)
    g_hover_msg_hwnd = NULL;
}

static void win32_msg_slot_register(HWND hwnd) {
  if (g_msg_slot_count >= MSG_SLOT_MAX) {
    HWND oldest = g_msg_slots[0];
    KillTimer(oldest, 999);
    DestroyWindow(oldest);
  }
  if (g_msg_slot_count >= MSG_SLOT_MAX)
    return;
  g_msg_slots[g_msg_slot_count++] = hwnd;
  g_last_created_msg_hwnd = hwnd;
}

bool win32_message_popup_any_open(void) { return g_msg_slot_count > 0; }

enum { MSG_DESTROY_ALL, MSG_DESTROY_INDEFINITE, MSG_DESTROY_NAMED };

static int win32_msg_slot_destroy_where(int mode, const char *name) {
  int destroyed = 0;
  for (int i = g_msg_slot_count - 1; i >= 0; i--) {
    HWND h = g_msg_slots[i];
    FadeWindowData *fd = (FadeWindowData *)GetWindowLongPtrA(h, GWLP_USERDATA);
    bool indefinite = fd && (MsgTimeoutIsIndefinite(fd->timeout));
    const char *fdname = fd ? fd->name : "";
    bool match = (mode == MSG_DESTROY_ALL) ||
                 (mode == MSG_DESTROY_INDEFINITE && indefinite) ||
                 (mode == MSG_DESTROY_NAMED && name && name[0] &&
                  strcmp(fdname, name) == 0);
    if (!match)
      continue;
    KillTimer(h, 999);
    DestroyWindow(h);
    destroyed++;
  }
  return destroyed;
}

void Platform_MessagePopup_PurgeOwner(const char *path_a, const char *path_b) {
  if ((!path_a || !path_a[0]) && (!path_b || !path_b[0]))
    return;
  for (int i = g_msg_slot_count - 1; i >= 0; i--) {
    HWND h = g_msg_slots[i];
    FadeWindowData *fd = (FadeWindowData *)GetWindowLongPtrA(h, GWLP_USERDATA);
    if (!fd || fd->render_owner[0] == '\0')
      continue;
    if (!Owner_ModuleMatches(fd->render_owner, path_a, path_b))
      continue;
    fd->render = NULL;
    fd->render_owner[0] = '\0';
    KillTimer(h, 999);
    DestroyWindow(h);
  }
}

static wchar_t *msg_widen(const char *in, int *out_len) {

  if (out_len)
    *out_len = 0;
  if (!in || !in[0])
    return NULL;
  int need = MultiByteToWideChar(CP_UTF8, 0, in, -1, NULL, 0);
  if (need <= 0)
    return NULL;
  wchar_t *w = (wchar_t *)malloc((size_t)need * sizeof(wchar_t));
  if (!w)
    return NULL;
  if (MultiByteToWideChar(CP_UTF8, 0, in, -1, w, need) <= 0) {
    free(w);
    return NULL;
  }
  if (out_len)
    *out_len = need - 1;
  return w;
}

LRESULT CALLBACK win32_display_message_proc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam) {

  FadeWindowData *data =
      (FadeWindowData *)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

  switch (msg) {
  case WM_NCCREATE: {
    CREATESTRUCTA *cs = (CREATESTRUCTA *)lParam;
    SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    return DefWindowProcA(hwnd, msg, wParam, lParam);
  }
  case WM_ERASEBKGND: {
    return 1;
  }
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    log_info("MSG WM_PAINT hwnd=%p data=%p render=%p no_bg=%d", (void *)hwnd,
             (void *)data, (void *)(data ? data->render : NULL),
             (data && data->no_bg) ? 1 : 0);

    RECT rect;
    GetClientRect(hwnd, &rect);

    if (data && data->no_bg) {
      win32_fill_rounded_rect(hdc, &rect, RGB(0, 0, 0), 0);
    } else if (win32_bg_image_enabled() &&
               win32_draw_bg_image_slice(hdc, &rect, rect.right, rect.bottom, 0,
                                         0)) {
    } else {
      HBRUSH hBackgroundBrush = CreateSolidBrush(COLOR_BG);
      FillRect(hdc, &rect, hBackgroundBrush);
      DeleteObject(hBackgroundBrush);
    }
    if (!(data && data->no_bg)) {
      win32_draw_window_border(hdc, 0, 0, rect.right, rect.bottom, NULL);
    }

    if (data && data->render) {
      win32_set_draw_context(hdc);
      data->render();
      win32_set_draw_context(NULL);
    }

    if (data) {
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
        wchar_t *wide = msg_widen(data->message, &wlen);
        if (wide) {
          RECT calcRect = textRect;
          DrawTextW(hdc, wide, wlen, &calcRect,
                    DT_CENTER | DT_WORDBREAK | DT_CALCRECT);

          int textHeight = calcRect.bottom - calcRect.top;
          int availableHeight = rect.bottom - rect.top;
          textRect.top = (availableHeight - textHeight) / 2;
          textRect.bottom = textRect.top + textHeight;
          if (textRect.bottom > rect.bottom - MSG_BOTTOM_CLIP)
            textRect.bottom = rect.bottom - MSG_BOTTOM_CLIP;

          DrawTextW(hdc, wide, wlen, &textRect, DT_CENTER | DT_WORDBREAK);
          free(wide);
        }
      }

      if (hOldFont)
        SelectObject(hdc, hOldFont);
    }
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_TIMER: {
    if (wParam == 999 && data) {
      if (data->timeout == MSG_TIMEOUT_INDEFINITE) {
        return 0;
      }

      ULONGLONG now = GetTickCount64();
      if (now < data->deadline_ms) {
        return 0;
      }

      data->alpha -= 12;

      if (data->alpha <= 0) {
        KillTimer(hwnd, 999);
        DestroyWindow(hwnd);
      } else {
        SetLayeredWindowAttributes(hwnd, 0, (BYTE)data->alpha, LWA_ALPHA);
      }
    }
    return 0;
  }
  case WM_NCDESTROY: {
    log_info("MSG WM_NCDESTROY hwnd=%p data=%p", (void *)hwnd, (void *)data);
    if (data) {
      if (data->message)
        free((void *)data->message);
      if (FH_TO_HFONT(data->hFont))
        Platform_DestroyFont(data->hFont);
      free(data);
      SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
    }
    break;
  }
  case WM_DESTROY: {
    HWND dead = hwnd;
    log_info("MSG WM_DESTROY hwnd=%p", (void *)dead);
    win32_msg_slot_unregister(dead);
    return 0;
  }
  }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static FadeWindowData *s_msg_show_data = NULL;
static bool s_msg_show_no_bg = false;

static bool message_popup_create(int x, int y, int w, int h) {
  if (!s_msg_show_data) {
    log_error("MSG create null payload");
    return false;
  }
  HINSTANCE hInstance = GetModuleHandle(NULL);
  DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED |
                  WS_EX_NOACTIVATE | WS_EX_TRANSPARENT;
  HWND hPopupWnd =
      CreateWindowExA(exStyle, MESSAGE_CLASS, NULL, WS_POPUP, x, y,
                      w, h, NULL, NULL, hInstance, s_msg_show_data);
  if (hPopupWnd == NULL) {
    log_error("MSG CreateWindowExA failed: %lu", GetLastError());
    return false;
  }
  s_msg_show_data = NULL;
  if (!s_msg_show_no_bg) {
    win32_apply_rounded_corners(hPopupWnd, w, h);
    SetLayeredWindowAttributes(hPopupWnd, 0, (BYTE)WindowOpacity, LWA_ALPHA);
  } else {
    SetLayeredWindowAttributes(hPopupWnd, 0, 0, LWA_COLORKEY);
  }
  ShowWindow(hPopupWnd, SW_SHOWNOACTIVATE);
  UpdateWindow(hPopupWnd);
  SetTimer(hPopupWnd, 999, 16, NULL);
  win32_msg_slot_register(hPopupWnd);
  {
    RECT vr;
    GetWindowRect(hPopupWnd, &vr);
    log_info("MSG after-show hwnd=%p IsWindowVisible=%d rect=(%d,%d,%d,%d) "
             "exstyle=%08lX",
             (void *)hPopupWnd, IsWindowVisible(hPopupWnd) ? 1 : 0, vr.left,
             vr.top, vr.right, vr.bottom,
             (unsigned long)GetWindowLongA(hPopupWnd, GWL_EXSTYLE));
  }
  return true;
}


void Platform_DisplayMessage(PlDisplayMessageSpec spec) {
  const char *message = spec.message;
  int centerX = spec.x;
  int centerY = spec.y;
  bool no_bg = spec.no_bg;
  int w = spec.w;
  int h = spec.h;
  int maxWidth = spec.max_width;
  int maxHeight = spec.max_height;
  double timeout = spec.timeout;
  ColorRef color = spec.color;
  void (*render)(void) = spec.render;
  const char *name = spec.name;

  int useMaxW = maxWidth;
  int useMaxH = maxHeight;
  if (w > 0 && h > 0) {
    if (useMaxW <= 0 || w > useMaxW)
      useMaxW = w;
    if (useMaxH <= 0 || h > useMaxH)
      useMaxH = h;
  }
  if (WH_TO_HWND(win32_tray_window()) != NULL &&
      !win32_is_main_thread()) {
    DisplayMessageParams params;
    params.message = message;
    params.x = centerX;
    params.y = centerY;
    params.max_width = useMaxW;
    params.max_height = useMaxH;
    params.timeout = timeout;
    params.color = color;
    params.no_bg = no_bg;
    params.render = render;
    params.name = name;
    SendMessageA(WH_TO_HWND(win32_tray_window()), WM_DISPLAY_MESSAGE, 0,
                 (LPARAM)&params);
    return;
  }
  Platform_CreateDisplayMessageWindow(
      (PlCreateDisplayMessageWindowSpec){.message = message,
                                         .x = centerX,
                                         .y = centerY,
                                         .max_width = useMaxW,
                                         .max_height = useMaxH,
                                         .timeout = timeout,
                                         .color = color,
                                         .no_bg = no_bg,
                                         .render = render,
                                         .name = name});
}

void Platform_DismissDisplayMessage(void) {
  HWND tray = WH_TO_HWND(win32_tray_window());
  if (tray != NULL && !win32_is_main_thread()) {
    SendMessageA(tray, WM_DISMISS_MESSAGE, 0, 0);
    return;
  }
  win32_msg_slot_destroy_where(MSG_DESTROY_ALL, NULL);
}

bool Platform_DismissMessage(const char *name) {
  HWND tray = WH_TO_HWND(win32_tray_window());
  if (tray != NULL && !win32_is_main_thread()) {
    DismissMessageParams params;
    params.name = name;
    return SendMessageA(tray, WM_DISMISS_MESSAGE, 0, (LPARAM)&params) != 0;
  }
  if (!name || !name[0])
    return win32_msg_slot_destroy_where(MSG_DESTROY_ALL, NULL) > 0;
  return win32_msg_slot_destroy_where(MSG_DESTROY_NAMED, name) > 0;
}

void Platform_DismissIndefiniteMessage(void) {
  win32_msg_slot_destroy_where(MSG_DESTROY_INDEFINITE, NULL);
}

bool Platform_MessageActive(const char *name) {
  if (!name || !name[0])
    return false;
  for (int i = 0; i < g_msg_slot_count; i++) {
    HWND h = g_msg_slots[i];
    if (!h || !IsWindow(h))
      continue;
    FadeWindowData *data =
        (FadeWindowData *)GetWindowLongPtrA(h, GWLP_USERDATA);
    if (data && strcmp(data->name, name) == 0)
      return true;
  }
  return false;
}

static char g_hover_key[64] = {0};

void Platform_HoverMessageRelease(const char *key) {
  if (!key || !key[0]) {
    return;
  }
  if (strcmp(g_hover_key, key) != 0) {
    return;
  }
  HWND hw = g_hover_msg_hwnd;
  if (hw && IsWindow(hw)) {
    KillTimer(hw, 999);
    DestroyWindow(hw);
  }
  g_hover_msg_hwnd = NULL;
  g_hover_key[0] = '\0';
}

void Platform_HoverMessage(PlHoverMessageSpec spec) {
  const char *key = spec.key;
  const char *message = spec.message;
  int centerX = spec.x;
  int centerY = spec.y;
  int w = spec.w;
  int h = spec.h;
  int maxWidth = spec.max_width;
  int maxHeight = spec.max_height;
  ColorRef color = spec.color;
  bool no_bg = spec.no_bg;

  HWND existing = g_hover_msg_hwnd;
  if (g_hover_key[0] && strcmp(g_hover_key, key) == 0 && existing &&
      IsWindow(existing)) {
    SetWindowPos(existing, HWND_TOPMOST, centerX - g_win32_msg_w / 2,
                 centerY - g_win32_msg_h / 2, g_win32_msg_w, g_win32_msg_h,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    FadeWindowData *hd =
        (FadeWindowData *)GetWindowLongPtrA(existing, GWLP_USERDATA);
    if (hd) {
      if (hd->message)
        free((void *)hd->message);
      hd->message = _strdup(message ? message : "");
      hd->color = color;
    }
    InvalidateRect(existing, NULL, TRUE);
    return;
  }

  int winWidth = w;
  int winHeight = h;
  if (!(winWidth > 0 && winHeight > 0)) {
    FontHandle msgFont = Platform_CreateFont(FontSize, UsedFont);
    int textWidth = 0, textHeight = 0;
    Platform_GetTextExtents(msgFont, message, maxWidth - 30, &textWidth,
                            &textHeight);
    Platform_DestroyFont(msgFont);
    winWidth = textWidth + 30;
    winHeight = textHeight + 30;
    if (maxWidth > 0 && winWidth > maxWidth)
      winWidth = maxWidth;
    if (maxHeight > 0 && winHeight > maxHeight)
      winHeight = maxHeight;
  }

  Platform_HoverMessageRelease(g_hover_key);
  Platform_CreateDisplayMessageWindow(
      (PlCreateDisplayMessageWindowSpec){.message = message,
                                         .x = centerX,
                                         .y = centerY,
                                         .max_width = winWidth,
                                         .max_height = winHeight,
                                         .timeout = MSG_TIMEOUT_INDEFINITE,
                                         .color = color,
                                         .no_bg = no_bg,
                                         .render = NULL,
                                         .name = NULL});
  g_win32_msg_w = winWidth;
  g_win32_msg_h = winHeight;
  snprintf(g_hover_key, sizeof(g_hover_key), "%s", key);
  g_hover_msg_hwnd = g_last_created_msg_hwnd;

  HWND hw = g_hover_msg_hwnd;
  if (hw && IsWindow(hw)) {
    KillTimer(hw, 999);
    SetWindowPos(hw, HWND_TOPMOST, centerX - winWidth / 2,
                 centerY - winHeight / 2, winWidth, winHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(hw, NULL, TRUE);
  }
}

void Platform_DisplayMessageAnchored(PlAnchoredMessageSpec spec) {
  const char *message = spec.message;
  int rect_x = spec.x;
  int rect_y = spec.y;
  int rect_w = spec.w;
  int rect_h = spec.h;
  int h_slot = spec.h_slot;
  int v_slot = spec.v_slot;
  int gap = spec.gap;
  int outward = spec.outward;
  int maxWidth = spec.max_width;
  int maxHeight = spec.max_height;
  double timeout = spec.timeout;
  ColorRef color = spec.color;
  bool no_bg = spec.no_bg;
  void (*render)(void) = spec.render;
  const char *name = spec.name;

  if (!message && !render) {
    return;
  }
  if (maxWidth <= 0)
    maxWidth = MSG_AUTO_WRAP_MAX_W;
  if (maxHeight <= 0)
    maxHeight = MSG_AUTO_WRAP_MAX_H;

  FontHandle msgFont = Platform_CreateFont(FontSize, UsedFont);
  int textWidth = 0, textHeight = 0;
  Platform_GetTextExtents(msgFont, message, maxWidth - 30, &textWidth,
                          &textHeight);
  Platform_DestroyFont(msgFont);

  int winWidth = textWidth + 30;
  int winHeight = textHeight + 30;
  if (winWidth > maxWidth)
    winWidth = maxWidth;
  if (winHeight > maxHeight)
    winHeight = maxHeight;

  int centerX;
  if (h_slot == 0)
    centerX =
        outward ? rect_x - gap - winWidth / 2 : rect_x + gap + winWidth / 2;
  else if (h_slot == 2)
    centerX = outward ? rect_x + rect_w + gap + winWidth / 2
                      : rect_x + rect_w - gap - winWidth / 2;
  else
    centerX = rect_x + rect_w / 2;
  int centerY;
  if (v_slot == 0)
    centerY =
        outward ? rect_y - gap - winHeight / 2 : rect_y + gap + winHeight / 2;
  else if (v_slot == 2)
    centerY = outward ? rect_y + rect_h + gap + winHeight / 2
                      : rect_y + rect_h - gap - winHeight / 2;
  else
    centerY = rect_y + rect_h / 2;

  log_info("MSG anchored msg='%s' rect=(%d,%d,%d,%d) slot=(%d,%d) gap=%d "
           "outward=%d -> center=(%d,%d) win=(%d,%d)",
           message, rect_x, rect_y, rect_w, rect_h, h_slot, v_slot, gap,
           outward, centerX, centerY, winWidth, winHeight);

  Platform_DisplayMessage((PlDisplayMessageSpec){.message = message,
                                                 .x = centerX,
                                                 .y = centerY,
                                                 .no_bg = no_bg,
                                                 .w = winWidth,
                                                 .h = winHeight,
                                                 .max_width = maxWidth,
                                                 .max_height = maxHeight,
                                                 .timeout = timeout,
                                                 .color = color,
                                                 .render = render,
                                                 .name = name});
}

void Platform_CreateDisplayMessageWindow(
    PlCreateDisplayMessageWindowSpec spec) {
  const char *message = spec.message;
  int centerX = spec.x;
  int centerY = spec.y;
  int maxWidth = spec.max_width;
  int maxHeight = spec.max_height;
  double timeout = spec.timeout;
  ColorRef color = spec.color;
  bool no_bg = spec.no_bg;
  void (*render)(void) = spec.render;
  const char *name = spec.name;

  log_info("MSG create enter msg='%s' center=(%d,%d) max=(%d,%d) timeout=%g "
           "no_bg=%d render=%p",
           message ? message : "(null)", centerX, centerY, maxWidth, maxHeight,
           timeout, no_bg ? 1 : 0, (void *)render);
  g_last_created_msg_hwnd = NULL;

  FontHandle msgFont = Platform_CreateFont(FontSize, UsedFont);
  int textWidth = 0, textHeight = 0;
  int winWidth = 0, winHeight = 0;

  if ((!message || !message[0]) && maxWidth > 0 && maxHeight > 0) {
    winWidth = maxWidth;
    winHeight = maxHeight;
  } else {
    Platform_GetTextExtents(msgFont, message, maxWidth - 30, &textWidth,
                            &textHeight);
    int padding = 30;
    winWidth = textWidth + padding;
    winHeight = textHeight + padding;
    if (maxWidth > 0 && winWidth > maxWidth)
      winWidth = maxWidth;
    if (maxHeight > 0 && winHeight > maxHeight)
      winHeight = maxHeight;
  }

  FadeWindowData *pWndData = (FadeWindowData *)malloc(sizeof(FadeWindowData));
  if (!pWndData) {
    log_error("MSG create failed: malloc returned NULL");
    Platform_DestroyFont(msgFont);
    return;
  }

  pWndData->message = _strdup(message);
  pWndData->hFont = msgFont;
  pWndData->alpha = WindowOpacity;
  pWndData->color = color;
  pWndData->no_bg = no_bg;
  pWndData->render = render;
  pWndData->render_owner[0] = '\0';
  if (render)
    Owner_ResolveForFunc((void *)render, pWndData->render_owner,
                         sizeof(pWndData->render_owner));
  if (name && name[0])
    snprintf(pWndData->name, sizeof(pWndData->name), "%s", name);
  else
    pWndData->name[0] = '\0';
  pWndData->timeout = timeout;
  if (MsgTimeoutIsIndefinite(timeout)) {
    pWndData->deadline_ms = 0;
  } else {
    pWndData->deadline_ms = GetTickCount64() + (ULONGLONG)(timeout * 1000.0);
  }

  int topLeftX = centerX - (winWidth / 2);
  int topLeftY = centerY - (winHeight / 2);

  POINT msgPt;
  msgPt.x = topLeftX + winWidth / 2;
  msgPt.y = topLeftY + winHeight / 2;
  HMONITOR hmon = MonitorFromPoint(msgPt, MONITOR_DEFAULTTONULL);
  log_info(
      "MSG computed text=(%d,%d) win=(%d,%d) at (%d,%d) font=%d monitor=%s",
      textWidth, textHeight, winWidth, winHeight, topLeftX, topLeftY,
      FH_TO_HFONT(msgFont) ? 1 : 0, hmon ? "FOUND" : "NONE");

  s_msg_show_data = pWndData;
  s_msg_show_no_bg = no_bg;
  if (!message_popup_create(topLeftX, topLeftY, winWidth, winHeight)) {
    log_error("MSG create failed for '%s'", message ? message : "");
    s_msg_show_data = NULL;
    s_msg_show_no_bg = false;
    Platform_DestroyFont(pWndData->hFont);
    free((void *)pWndData->message);
    free(pWndData);
    return;
  }
  log_info("MSG show ok text=(%d,%d) win=(%d,%d) at (%d,%d)", textWidth,
           textHeight, winWidth, winHeight, topLeftX, topLeftY);
  return;
}
