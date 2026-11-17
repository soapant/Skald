#include "helpers/win32/raw_message_popup_win32.h"

static int win32_msg_w = 0;

static int win32_msg_h = 0;

static HWND hover_msg_hwnd = NULL;

static HWND last_created_msg_hwnd = NULL;

HWND g_msg_slots[MSG_SLOT_MAX];

int g_msg_slot_count = 0;

int win32_msg_slot_find(HWND hwnd) {
  for (int i = 0; i < g_msg_slot_count; i++) {
    if (g_msg_slots[i] == hwnd)
      return i;
  }
  return -1;
}

void win32_msg_slot_unregister(HWND hwnd) {
  int idx = win32_msg_slot_find(hwnd);
  if (idx < 0)
    return;
  for (int i = idx; i < g_msg_slot_count - 1; i++)
    g_msg_slots[i] = g_msg_slots[i + 1];
  g_msg_slot_count--;
  if (hwnd == hover_msg_hwnd)
    hover_msg_hwnd = NULL;
}

void win32_msg_slot_register(HWND hwnd) {
  if (g_msg_slot_count >= MSG_SLOT_MAX) {
    HWND oldest = g_msg_slots[0];
    KillTimer(oldest, 999);
    DestroyWindow(oldest);
  }
  if (g_msg_slot_count >= MSG_SLOT_MAX)
    return;
  g_msg_slots[g_msg_slot_count++] = hwnd;
  last_created_msg_hwnd = hwnd;
}

bool win32_message_popup_any_open(void) { return g_msg_slot_count > 0; }

int win32_msg_slot_destroy_where(int mode, const char *name) {
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
      data->render(data->render_data);
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

FadeWindowData *s_msg_show_data = NULL;

bool s_msg_show_no_bg = false;
bool s_msg_rounded = true;
int s_msg_radius = 14;

bool win32_message_popup_create(int x, int y, int w, int h) {
  if (!s_msg_show_data) {
    log_error("MSG create null payload");
    return false;
  }
  HINSTANCE hInstance = GetModuleHandle(NULL);
  DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED |
                  WS_EX_NOACTIVATE | WS_EX_TRANSPARENT;
  HWND hPopupWnd =
      CreateWindowExA(exStyle, MESSAGE_CLASS, NULL, WS_POPUP, x, y, w, h, NULL,
                      NULL, hInstance, s_msg_show_data);
  if (hPopupWnd == NULL) {
    log_error("MSG CreateWindowExA failed: %lu", GetLastError());
    return false;
  }
  s_msg_show_data = NULL;
  if (!s_msg_show_no_bg) {
    win32_apply_rounded(hPopupWnd, w, h, s_msg_rounded, s_msg_radius);
    SetLayeredWindowAttributes(hPopupWnd, 0, (BYTE)WindowOpacity, LWA_ALPHA);
  } else {
    win32_apply_rounded(hPopupWnd, w, h, s_msg_rounded, s_msg_radius);
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

static char hover_key[64] = {0};

void win32_msg_created_clear(void) { last_created_msg_hwnd = NULL; }

void win32_msg_hover_release(const char *key) {
  if (!key || !key[0]) {
    return;
  }
  if (strcmp(hover_key, key) != 0) {
    return;
  }
  HWND hw = hover_msg_hwnd;
  if (hw && IsWindow(hw)) {
    KillTimer(hw, 999);
    DestroyWindow(hw);
  }
  hover_msg_hwnd = NULL;
  hover_key[0] = '\0';
}

void win32_msg_hover_present(HoverMessageSpec spec) {
  HOVER_MSG_UNPACK(spec);

  HWND existing = hover_msg_hwnd;
  if (hover_key[0] && strcmp(hover_key, key) == 0 && existing &&
      IsWindow(existing)) {
    SetWindowPos(existing, HWND_TOPMOST, center_x - win32_msg_w / 2,
                 center_y - win32_msg_h / 2, win32_msg_w, win32_msg_h,
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
    Platform_GetTextExtents(msgFont, message, max_width - 30, &textWidth,
                            &textHeight);
    Platform_DestroyFont(msgFont);
    winWidth = textWidth + 30;
    winHeight = textHeight + 30;
    if (max_width > 0 && winWidth > max_width)
      winWidth = max_width;
    if (max_height > 0 && winHeight > max_height)
      winHeight = max_height;
  }

  win32_msg_hover_release(hover_key);
  Platform_CreateDisplayMessageWindow(
      (CreateDisplayMessageWindowSpec){.message = message,
                                         .x = center_x,
                                         .y = center_y,
                                         .max_width = winWidth,
                                         .max_height = winHeight,
                                         .timeout = MSG_TIMEOUT_INDEFINITE,
                                         .color = color,
                                         .no_bg = no_bg,
                                         .render = NULL,
                                         .name = NULL});
  win32_msg_w = winWidth;
  win32_msg_h = winHeight;
  snprintf(hover_key, sizeof(hover_key), "%s", key);
  hover_msg_hwnd = last_created_msg_hwnd;

  HWND hw = hover_msg_hwnd;
  if (hw && IsWindow(hw)) {
    KillTimer(hw, 999);
    SetWindowPos(hw, HWND_TOPMOST, center_x - winWidth / 2,
                 center_y - winHeight / 2, winWidth, winHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(hw, NULL, TRUE);
  }
}
