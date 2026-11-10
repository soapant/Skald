#include "config_values.h"
#include "core/types/prompt.h"
#include "platform/wm_platform.h"
#include "helpers/helpers_monitor.h"
#include "platform/platform.h"
#include "helpers/helpers_input_state.h"
#include "core/window_manager.h"
#include "core/remap_manager.h"
#include "deps/log.h"
#include <windows.h>
#include "core/prompt_geometry.h"
#include "core/context.h"
#include "core/listbox_hover.h"

extern int WindowManager_RunWin32(WindowManager *wm);
extern void WindowManager_QuitWin32(WindowManager *wm);
extern void WindowManager_ProcessEventsWin32(WindowManager *wm);

bool Platform_WM_InitDisplay(WindowManager *wm) {
  (void)wm;
  return true;
}

int Platform_WM_Run(WindowManager *wm) { return WindowManager_RunWin32(wm); }

void Platform_PromptFlushDisplay(WindowManager *wm) {
  (void)wm;
}

void Platform_WM_Quit(WindowManager *wm) { WindowManager_QuitWin32(wm); }

void Platform_WM_ProcessEvents(WindowManager *wm) {
  WindowManager_ProcessEventsWin32(wm);
}

void Platform_WM_OnPopEmpty(WindowManager *wm) {
  (void)wm;
  remap_reset();
  InputState_Clear();
}

void Platform_WM_GetMonitorGeometry(int *ox, int *oy, int *ow, int *oh) {
  int x = 0, y = 0, w = GetSystemMetrics(SM_CXSCREEN),
      h = GetSystemMetrics(SM_CYSCREEN);
  if (!w)
    w = 1920;
  if (!h)
    h = 1080;
  POINT pt = {0, 0};
  if (GetCursorPos(&pt)) {
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    if (mon) {
      MONITORINFO mi;
      mi.cbSize = sizeof(mi);
      if (GetMonitorInfoA(mon, &mi)) {
        x = mi.rcMonitor.left;
        y = mi.rcMonitor.top;
        w = mi.rcMonitor.right - mi.rcMonitor.left;
        h = mi.rcMonitor.bottom - mi.rcMonitor.top;
      }
    }
  }
  if (!w)
    w = 1920;
  if (!h)
    h = 1080;
  log_info("Platform_WM_GetMonitorGeometry cursor=(%ld,%ld) mon=(%d,%d %dx%d)",
           pt.x, pt.y, x, y, w, h);
  if (ox)
    *ox = x;
  if (oy)
    *oy = y;
  if (ow)
    *ow = w;
  if (oh)
    *oh = h;
}

void Platform_GetTextLayoutMetrics(int *line_h, int *row_h, int *list_top) {
  int lh = FontSize;
  int rh = PromptRowHeight();
  if (line_h)
    *line_h = lh;
  if (row_h)
    *row_h = rh;
  if (list_top)
    *list_top = PromptListTop();
}


void Platform_ProcessPendingSubwindow(void) {
  SkaldContext *_c = SkaldContext_Get();
  if (!_c || !_c->window.subwindow.pending)
    return;
  _c->window.subwindow.pending = false;
  _c->window.subwindow.created = true;
  WindowManager *wm = WindowManager_Get();
  if (wm && wm->popup_count > 1)
    WindowManager_PopPopup(wm);
  _c->window.selection_complete = false;
  (void)PromptPopup_Create(wm, &_c->window.subwindow.pending_subwindow);
}

typedef struct {
  RECT rc;
  char name[128];
  bool is_primary;
} Win32MonitorEntry;

typedef struct {
  Win32MonitorEntry list[16];
  int count;
} Win32MonitorList;

static BOOL CALLBACK win32_monitor_enum(HMONITOR hMonitor, HDC hdc,
                                        LPRECT rect, LPARAM lParam) {
  (void)hdc;
  Win32MonitorList *list = (Win32MonitorList *)lParam;
  if (list->count >= 16)
    return TRUE;
  MONITORINFOEXW mi;
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfoW(hMonitor, (MONITORINFO *)&mi))
    return TRUE;
  list->list[list->count].rc = mi.rcMonitor;
  list->list[list->count].is_primary =
      (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
  WideCharToMultiByte(CP_UTF8, 0, mi.szDevice, -1, list->list[list->count].name,
                      sizeof(list->list[list->count].name), NULL, NULL);
  list->count++;
  return TRUE;
}

int MonitorFill(MonitorEntry *out, int max_entries) {
  Win32MonitorList list = {{{0}}, 0};
  EnumDisplayMonitors(NULL, NULL, win32_monitor_enum, (LPARAM)&list);
  if (list.count == 0)
    return 0;
  int mon_cap = list.count < max_entries ? list.count : max_entries;
  for (int i = 0; i < mon_cap; i++) {
    out[i].x = list.list[i].rc.left;
    out[i].y = list.list[i].rc.top;
    out[i].w = list.list[i].rc.right - list.list[i].rc.left;
    out[i].h = list.list[i].rc.bottom - list.list[i].rc.top;
    out[i].is_primary = list.list[i].is_primary;
    snprintf(out[i].name, sizeof(out[i].name), "%s", list.list[i].name);
  }
  int primary_at = 0;
  for (int i = 0; i < mon_cap; i++)
    if (out[i].is_primary) {
      primary_at = i;
      break;
    }
  if (primary_at > 0) {
    MonitorEntry t = out[0];
    out[0] = out[primary_at];
    out[primary_at] = t;
  }
  return list.count;
}

