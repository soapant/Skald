#include "config_values.h"
#include "core/context.h"
#include "core/editbox.h"
#include "core/focus.h"
#include "core/prompt_geometry.h"
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
#include "core/draw_state.h"
#include "platform/platform.h"
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
  DWORD ex_style = WS_EX_TOOLWINDOW | WS_EX_LAYERED;
  SkaldContext_Get()->window.hPopupWnd = WH_FROM_HWND(CreateWindowExA(
      ex_style, SKALD_CLASS, NULL, WS_POPUP | WS_CLIPCHILDREN, x, y, w, h, NULL,
      NULL, hInstance, NULL));
  if (WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd) == NULL) {
    return false;
  }
  HWND hwnd = WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd);
  if (self)
    self->handle = SkaldContext_Get()->window.hPopupWnd;
  win32_apply_rounded_corners(hwnd, w, h);
  ShowWindow(hwnd, SW_SHOW);
  win32_force_window_focus(SkaldContext_Get()->window.hPopupWnd);
  return true;
}

bool Platform_PromptPopupResize(Popup *self, int x, int y, int w, int h) {
  if (w <= 0 || h <= 0)
    return false;
  HWND hwnd = self ? WH_TO_HWND(self->handle) : NULL;
  if (!hwnd || !IsWindow(hwnd))
    hwnd = WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd);
  if (!hwnd || !IsWindow(hwnd))
    return false;
  if (!SetWindowPos(hwnd, NULL, x, y, w, h,
                    SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER))
    return false;
  win32_apply_rounded_corners(hwnd, w, h);
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
    long long now_ticks = Platform_TimerNowTicks();
    long long freq = Platform_TimerTickFrequency();
    if (state->last_frame_ticks && freq > 0)
      state->delta_time =
          (double)(now_ticks - state->last_frame_ticks) * 1000.0 /
          (double)freq;
    state->last_frame_ticks = now_ticks;
    prompt_band_take_placed();
    bool sep_prev = draw_state_get()->suppress_separators;
    draw_state_get()->suppress_separators = state->no_separators;
    focus_frame_begin(state->lb != NULL, 0, prompt_list_top(), state->width,
                      state->height - prompt_list_top() -
                          prompt_list_bottom_margin());
    editbox_registry_register_focus(state);
    editbox_registry_tick(state);
    editbox_registry_begin_frame(state);
    state->callbacks->render(state);
    prompt_band_commit_placed();
    focus_frame_end();
    draw_state_get()->suppress_separators = sep_prev;
  }
}

void Platform_PromptPopupDestroyWindow(Popup *self) {
  Platform_PromptPopupHideWindow(self);
}

bool Platform_PromptPopupOnKey(const Popup *self, const PopupKeyEvent *key) {
  return false;
}
