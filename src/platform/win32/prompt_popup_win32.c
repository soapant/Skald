#include "config_values.h"
#include "core/context.h"
#include "core/types/color.h"
#include "core/types/keys.h"
#include "core/window_manager.h"
#include "core/listbox_hover.h"
#include "core/types/prompt.h"
#include "core/types/window.h"
#include "platform/wm_platform.h"
#include "helpers/helpers_ui.h"
#include "helpers/win32/common_ui_win32.h"
#include "helpers/win32/win32_state.h"
#include <windows.h>

static HWND g_prev_fg = NULL;
static BOOL old_track = FALSE;
static BOOL track_saved = FALSE;

static void CALLBACK win32_reenable_track_proc(HWND hwnd, UINT msg, UINT_PTR id,
                                               DWORD time) {

  (void)hwnd;
  (void)msg;
  (void)time;
  KillTimer(NULL, id);
  if (track_saved) {
    SystemParametersInfoA(SPI_SETACTIVEWINDOWTRACKING, 0,
                          (PVOID)(uintptr_t)old_track, 0);
    track_saved = FALSE;
  }
}

static bool is_popup_class(HWND hwnd) {

  if (!hwnd || !IsWindow(hwnd))
    return false;
  char cls[64] = {0};
  GetClassNameA(hwnd, cls, sizeof(cls));
  {
    return strcmp(cls, SKALD_CLASS) == 0 || strcmp(cls, MESSAGE_CLASS) == 0;
  }
}

void win32_set_prev_fg(HWND h) {
  if (h && IsWindow(h))
    g_prev_fg = h;
}

bool Platform_PromptPopupShowWindow(Popup *self, int x, int y, int w, int h) {
  (void)self;

  listbox_invalidate_mouse_pos();
  HWND fg = GetForegroundWindow();
  if (fg && IsWindow(fg) && !is_popup_class(fg))
    g_prev_fg = fg;
  else if (!fg || !IsWindow(fg))
    g_prev_fg = NULL;
  char _save_title[128] = {0};
  if (g_prev_fg)
    GetWindowTextA(g_prev_fg, _save_title, sizeof(_save_title));
  {
    BOOL cur = FALSE;
    if (SystemParametersInfoA(SPI_GETACTIVEWINDOWTRACKING, 0, &cur, 0)) {
      if (!track_saved)
        old_track = cur;
      track_saved = TRUE;
      if (cur) {
        SystemParametersInfoA(SPI_SETACTIVEWINDOWTRACKING, 0, (PVOID)FALSE, 0);
      }
    }
  }
  HINSTANCE hInstance = GetModuleHandle(NULL);
  SkaldContext_Get()->window.hPopupWnd = WH_FROM_HWND(CreateWindowExA(
      WS_EX_TOOLWINDOW | WS_EX_COMPOSITED | WS_EX_LAYERED, SKALD_CLASS, NULL,
      WS_POPUP | WS_CLIPCHILDREN, x, y, w, h, NULL, NULL, hInstance, NULL));
  if (WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd) == NULL) {
    return false;
  }
  HWND hwnd = WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd);
  if (self)
    self->handle = SkaldContext_Get()->window.hPopupWnd;
  SetLayeredWindowAttributes(hwnd, 0, (BYTE)WindowOpacity, LWA_ALPHA);
  win32_apply_rounded_corners(hwnd, w, h);
  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);
  win32_force_window_focus(SkaldContext_Get()->window.hPopupWnd);
  return true;
}

void Platform_PromptPopupHideWindow(Popup *self) {
  HWND ctx_hwnd = WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd);
  HWND hwnd = self ? WH_TO_HWND(self->handle) : ctx_hwnd;
  if (!hwnd) {
    if (self)
      self->handle = WH_NULL;
    return;
  }
  bool subwindow_hide = ctx_hwnd != NULL && ctx_hwnd != hwnd;

  if (hwnd != NULL && IsWindow(hwnd)) {
    KillTimer(hwnd, 999);
    KillTimer(hwnd, UI_TICK_TIMER_ID);
    DestroyWindow(hwnd);
    if (hwnd == WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd)) {
      SkaldContext_Get()->window.hPopupWnd = WH_NULL;
    }
  }

  if (self)
    self->handle = WH_NULL;

  if (subwindow_hide) {
    if (IsWindow(ctx_hwnd))
      win32_force_window_focus(WH_FROM_HWND(ctx_hwnd));
    return;
  }

  if (g_prev_fg && IsWindow(g_prev_fg)) {
    if (track_saved && old_track)
      SetTimer(NULL, 0xC0DF, 800, win32_reenable_track_proc);
    else
      track_saved = FALSE;
    g_prev_fg = NULL;
  } else {
    if (track_saved && old_track)
      SetTimer(NULL, 0xC0DF, 800, win32_reenable_track_proc);
    else
      track_saved = FALSE;
  }
}

void Platform_PromptPopupRender(Popup *self) {
  PromptState *state = (PromptState *)self->user_data;
  if (!state)
    return;
  if (state->callbacks->render) {
    state->callbacks->render(state);
  }
}

void Platform_PromptPopupDestroyWindow(Popup *self) {
  Platform_PromptPopupHideWindow(self);
}

bool Platform_PromptPopupOnKey(const Popup *self, const PopupKeyEvent *key) {
  return false;
}
