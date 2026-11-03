#include "core/context.h"
#include "core/window_manager.h"
#include "core/timer.h"
#include "core/listbox_hover.h"
#include "core/types/prompt.h"
#include "core/types/window.h"
#include "core/prompt_geometry.h"
#include "platform/wm_platform.h"
#include "platform/ui.h"
#include "platform/win32/common_ui_win32.h"
#include "platform/win32/win32_state.h"
#include "platform/draw.h"
#include "config.h"
#include "deps/log.h"
#include <windows.h>
#include <stdlib.h>

static HWND g_prev_fg = NULL;
static BOOL g_old_track = FALSE;
static BOOL g_track_saved = FALSE;
static HWND g_pending_restore = NULL;

static void CALLBACK ReenableTrackProc(HWND hwnd, UINT msg, UINT_PTR id,
                                       DWORD time) {

  (void)hwnd;
  (void)msg;
  (void)time;
  KillTimer(NULL, id);
  if (g_track_saved) {
    SystemParametersInfoA(SPI_SETACTIVEWINDOWTRACKING, 0,
                          (PVOID)(uintptr_t)g_old_track, 0);
    g_track_saved = FALSE;
  }
}

static bool is_popup_class(HWND hwnd) {

  if (!hwnd || !IsWindow(hwnd))
    return false;
  char cls[64] = {0};
  GetClassNameA(hwnd, cls, sizeof(cls));
  {
    return strcmp(cls, "QuickSwitcherModalClass") == 0 ||
           strcmp(cls, "DisplayMessageModalClass") == 0 ||
           strcmp(cls, "EditPopupClass") == 0 ||
           strcmp(cls, "SearchPopupClass") == 0;
  }
}

void Platform_SetFlintPrevFg(HWND h) {
  if (h && IsWindow(h))
    g_prev_fg = h;
}

bool Platform_PromptPopupShowWindow(Popup *self, int x, int y, int w, int h) {
  (void)self;

  ListBox_InvalidateMousePos();
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
      g_old_track = cur;
      g_track_saved = TRUE;
      if (cur) {
        SystemParametersInfoA(SPI_SETACTIVEWINDOWTRACKING, 0, (PVOID)FALSE, 0);
      }
    }
  }
  HINSTANCE hInstance = GetModuleHandle(NULL);
  FlintContext_Get()->window.hPopupWnd = WH_FROM_HWND(CreateWindowExA(
      WS_EX_TOOLWINDOW | WS_EX_COMPOSITED | WS_EX_LAYERED,
      "QuickSwitcherModalClass", NULL, WS_POPUP | WS_CLIPCHILDREN, x, y, w, h,
      NULL, NULL, hInstance, NULL));
  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) == NULL) {
    return false;
  }
  HWND hwnd = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
  if (self)
    self->handle = FlintContext_Get()->window.hPopupWnd;
  SetLayeredWindowAttributes(hwnd, 0, (BYTE)WindowOpacity, LWA_ALPHA);
  if (true) {
    win32_apply_rounded_corners(hwnd, w, h);
  }
  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);
  ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, WH_NULL);
  return true;
}

void Platform_PromptPopupHideWindow(Popup *self) {
  HWND hwnd = (self && WH_TO_HWND(self->handle) != NULL)
                  ? WH_TO_HWND(self->handle)
                  : WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);

  if (hwnd != NULL && IsWindow(hwnd)) {
    KillTimer(hwnd, 999);
    KillTimer(hwnd, UI_TICK_TIMER_ID);
    DestroyWindow(hwnd);
    if (hwnd == WH_TO_HWND(FlintContext_Get()->window.hPopupWnd)) {
      FlintContext_Get()->window.hPopupWnd = WH_NULL;
    }
  }

  if (self)
    self->handle = WH_NULL;

  if (g_prev_fg && IsWindow(g_prev_fg)) {
    HWND target = g_prev_fg;
    g_pending_restore = target;
    if (g_track_saved && g_old_track)
      SetTimer(NULL, 0xC0DF, 800, ReenableTrackProc);
    else
      g_track_saved = FALSE;
    g_prev_fg = NULL;
  } else {
    if (g_track_saved && g_old_track)
      SetTimer(NULL, 0xC0DF, 800, ReenableTrackProc);
    else
      g_track_saved = FALSE;
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

void Platform_PromptPopupDestroyWindow(Popup *self) {}

bool Platform_PromptPopupOnKey(Popup *self, const PopupKeyEvent *key) {
  return false;
}
