#ifndef WINDOW_PLATFORM_H
#define WINDOW_PLATFORM_H

#include <stdbool.h>
#include <ctype.h>
#include <string.h>

#include <stdlib.h>
#include <string.h>

#include "core/types.h"
#include "core/types/window.h"

static inline void wm_snap_rect(int mx, int my, int mw, int mh,
                                SnapPreset p, int *x, int *y, int *w,
                                int *h) {
  int hw = mw / 2, hh = mh / 2;
  switch (p) {
  case SNAP_LEFT:
    *x = mx; *y = my; *w = hw; *h = mh; return;
  case SNAP_RIGHT:
    *x = mx + hw; *y = my; *w = mw - hw; *h = mh; return;
  case SNAP_TOP:
    *x = mx; *y = my; *w = mw; *h = hh; return;
  case SNAP_BOTTOM:
    *x = mx; *y = my + hh; *w = mw; *h = mh - hh; return;
  case SNAP_TOP_LEFT:
    *x = mx; *y = my; *w = hw; *h = hh; return;
  case SNAP_TOP_RIGHT:
    *x = mx + hw; *y = my; *w = mw - hw; *h = hh; return;
  case SNAP_BOTTOM_LEFT:
    *x = mx; *y = my + hh; *w = hw; *h = mh - hh; return;
  case SNAP_BOTTOM_RIGHT:
    *x = mx + hw; *y = my + hh; *w = mw - hw; *h = mh - hh; return;
  }
}

static inline bool wm_json_num_at(const char *json, const char *key,
                                  long *out_v) {
  const char *p = json ? strstr(json, key) : NULL;
  if (!p)
    return false;
  p += strlen(key);
  while (*p == ' ' || *p == '\t' || *p == ':')
    p++;
  char *end = NULL;
  long v = strtol(p, &end, 10);
  if (!end || end == p)
    return false;
  *out_v = v;
  return true;
}

char *wm_trim_focus_title(const char *title);


#include "helpers/helpers_text.h"

bool wm_append_window_name(PlWindowList *list, const char *name,
                           bool toupper_first);

char *wm_get_active_window_title(void);
char *wm_get_active_window_id(void);
bool wm_focus_by_title(const char *title);
char *wm_paste(const char *content);
PlWindowList Wm_GetWindowTitles(void);
PlWindowList Wm_GetWindowIds(void);
bool wm_close_active_window(void);
bool wm_toggle_fullscreen_active(void);
bool wm_move_active_window_to_monitor(bool right);
bool wm_move_active_window_left(void);
bool wm_move_active_window_up(void);
bool wm_move_active_window_down(void);
bool wm_move_active_window_right(void);
bool wm_focus_window_left(void);
bool wm_focus_window_up(void);
bool wm_focus_window_down(void);
bool wm_focus_window_right(void);
bool wm_minimize_active_window(void);
bool wm_toggle_maximize_active(void);
bool wm_toggle_always_on_top_active(void);
bool wm_raise_active_window(void);
bool wm_lower_active_window(void);
bool wm_center_active_window(void);
bool wm_snap_active_window(SnapPreset preset);
long wm_get_active_window_pid(void);
bool wm_inject_key(int vk, int down);
bool wm_switch_to_workspace(int index);
bool wm_move_active_window_to_workspace(int index);
bool wm_set_active_window_opacity(int percent);
bool wm_close_windows_matching_title(const char *needle);
bool wm_close_windows_matching_id(const char *needle);
bool wm_get_mouse_pos(int *x, int *y);

#endif
