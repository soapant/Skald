#include "wm/wm.h"

#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

#include "helpers/x11/env_helpers_x11.h"
#include "wm/kwin_wm.h"
#include "wm/niri_wm.h"
#include "wm/sway_wm.h"
#include "wm/hyprland_wm.h"
#include "wm/i3_wm.h"
#include "wm/x11_wm.h"
#include "deps/log.h"

bool wm_append_window_name(WindowList *list, const char *name,
                           bool toupper_first) {
  if (!strcasecmp(name, "window") || !strcasecmp(name, "colored"))
    return true;
  char buf[1024];
  snprintf(buf, sizeof(buf), "%s", name);
  if (toupper_first)
    buf[0] = toupper((unsigned char)buf[0]);
  return list_append(list, buf);
}

char *wm_trim_focus_title(const char *title) {
  static __thread char trimmed[1024];
  if (!title)
    return NULL;
  const char *start = title;
  while (*start &&
         (iscntrl((unsigned char)*start) || isspace((unsigned char)*start)))
    start++;
  const char *end = start + strlen(start);
  while (end > start &&
         (iscntrl((unsigned char)end[-1]) || isspace((unsigned char)end[-1])))
    end--;
  size_t len = (size_t)(end - start);
  if (len >= sizeof(trimmed))
    len = sizeof(trimmed) - 1;
  memcpy(trimmed, start, len);
  trimmed[len] = '\0';
  return trimmed;
}

char *wm_get_active_window_title(void) {
  static char buffer[1024];

  buffer[0] = '\0';

  if (x11_env_is_wayland()) {
    if (get_title_kwin(buffer, sizeof(buffer))) {
      if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0)
        return buffer;
    }
    if (get_title_niri(buffer, sizeof(buffer))) {
      if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0)
        return buffer;
    }
    if (get_title_hyprland(buffer, sizeof(buffer))) {
      if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0)
        return buffer;
    }
    if (get_title_sway(buffer, sizeof(buffer))) {
      if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0)
        return buffer;
    }
  }
  if (x11_get_title(buffer, sizeof(buffer))) {
    if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0)
      return buffer;
  }
  return NULL;
}

char *wm_get_active_window_id(void) {
  static char buffer[1024];

  buffer[0] = '\0';

  if (x11_env_is_wayland()) {
    if (get_id_kwin(buffer, sizeof(buffer))) {
      if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0)
        return buffer;
    }
    if (get_id_niri(buffer, sizeof(buffer))) {
      if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0)
        return buffer;
    }
    if (get_id_hyprland(buffer, sizeof(buffer))) {
      if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0)
        return buffer;
    }
    if (get_id_sway(buffer, sizeof(buffer))) {
      if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0)
        return buffer;
    }
  }
  if (x11_get_id(buffer, sizeof(buffer))) {
    if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0)
      return buffer;
  }
  return NULL;
}

bool wm_focus_by_title(const char *title) {
  if (x11_env_is_wayland()) {
    if (focus_window_title_kwin(title)) {
      return true;
    }
    if (focus_window_title_niri(title)) {
      return true;
    }
    if (focus_window_title_hyprland(title)) {
      return true;
    }
    if (focus_window_title_sway(title)) {
      return true;
    }
  }
  if (x11_focus_window_title(title)) {
    return true;
  }
  return NULL;
}

char *wm_paste(const char *content) {
  char *result;
  if (x11_env_is_wayland()) {
    if ((result = paste_kwin(content)))
      return result;
    if ((result = paste_niri(content)))
      return result;
    if ((result = paste_hyprland(content)))
      return result;
    if ((result = paste_sway(content)))
      return result;
  }
  if ((result = x11_paste(content)))
    return result;
  log_error("content='%s'", content ? content : "null");
  return NULL;
}

WindowList wm_get_window_titles(void) {
  WindowList result;
  WindowList empty = {.windows = NULL, .count = 0};
  if (x11_env_is_wayland()) {
    result = get_window_titles_kwin();
    if (result.windows != NULL)
      return result;
    result = get_window_titles_niri();
    if (result.windows != NULL)
      return result;
    result = get_window_titles_hyprland();
    if (result.windows != NULL)
      return result;
    result = get_window_titles_sway();
    if (result.windows != NULL)
      return result;
  }
  result = x11_get_window_titles();
  if (result.windows != NULL)
    return result;
  return empty;
}
WindowList wm_get_window_ids(void) {
  WindowList result;
  WindowList empty = {.windows = NULL, .count = 0};
  if (x11_env_is_wayland()) {
    result = get_window_ids_kwin();
    if (result.windows != NULL)
      return result;
    result = get_window_ids_niri();
    if (result.windows != NULL)
      return result;
    result = get_window_ids_hyprland();
    if (result.windows != NULL)
      return result;
    result = get_window_ids_sway();
    if (result.windows != NULL)
      return result;
  }
  result = x11_get_window_names();
  if (result.windows != NULL)
    return result;
  return empty;
}

bool wm_close_active_window(void) {
  if (x11_env_is_wayland()) {
    if (close_active_window_kwin())
      return true;
    if (close_active_window_niri())
      return true;
    if (close_active_window_hyprland())
      return true;
    if (close_active_window_sway())
      return true;
  }
  if (x11_close_active_window())
    return true;
  return false;
}

bool wm_toggle_fullscreen_active(void) {
  if (x11_env_is_wayland()) {
    if (toggle_fullscreen_active_kwin())
      return true;
    if (toggle_fullscreen_active_niri())
      return true;
    if (toggle_fullscreen_active_hyprland())
      return true;
    if (toggle_fullscreen_active_sway())
      return true;
  }
  if (x11_toggle_fullscreen_active())
    return true;
  return false;
}

bool wm_move_active_window_to_monitor(bool right) {
  if (x11_env_is_wayland()) {
    if (move_active_window_to_monitor_kwin(right))
      return true;
    if (move_active_window_to_monitor_niri(right))
      return true;
    if (move_active_window_to_monitor_hyprland(right))
      return true;
    if (move_active_window_to_monitor_sway(right))
      return true;
  }
  if (move_active_window_to_monitor_i3(right))
    return true;
  if (x11_move_active_window_to_monitor(right))
    return true;
  return false;
}

bool wm_move_active_window_left(void) {
  if (x11_env_is_wayland()) {
    if (move_active_window_left_kwin())
      return true;
    if (move_active_window_left_niri())
      return true;
    if (move_active_window_left_hyprland())
      return true;
    if (move_active_window_left_sway())
      return true;
  }
  if (move_active_window_left_i3())
    return true;
  if (x11_move_active_window_left())
    return true;
  return false;
}

bool wm_move_active_window_right(void) {
  if (x11_env_is_wayland()) {
    if (move_active_window_right_kwin())
      return true;
    if (move_active_window_right_niri())
      return true;
    if (move_active_window_right_hyprland())
      return true;
    if (move_active_window_right_sway())
      return true;
  }
  if (move_active_window_right_i3())
    return true;
  if (x11_move_active_window_right())
    return true;
  return false;
}

bool wm_move_active_window_up(void) {
  if (x11_env_is_wayland()) {
    if (move_active_window_up_kwin())
      return true;
    if (move_active_window_up_niri())
      return true;
    if (move_active_window_up_hyprland())
      return true;
    if (move_active_window_up_sway())
      return true;
  }
  if (x11_move_active_window_up())
    return true;
  return false;
}

bool wm_move_active_window_down(void) {
  if (x11_env_is_wayland()) {
    if (move_active_window_down_kwin())
      return true;
    if (move_active_window_down_niri())
      return true;
    if (move_active_window_down_hyprland())
      return true;
    if (move_active_window_down_sway())
      return true;
  }
  if (x11_move_active_window_down())
    return true;
  return false;
}

bool wm_focus_window_left(void) {
  if (x11_env_is_wayland()) {
    if (focus_window_left_kwin())
      return true;
    if (focus_window_left_niri())
      return true;
    if (focus_window_left_hyprland())
      return true;
    if (focus_window_left_sway())
      return true;
  }
  if (x11_focus_window_left())
    return true;
  return false;
}

bool wm_focus_window_right(void) {
  if (x11_env_is_wayland()) {
    if (focus_window_right_kwin())
      return true;
    if (focus_window_right_niri())
      return true;
    if (focus_window_right_hyprland())
      return true;
    if (focus_window_right_sway())
      return true;
  }
  if (x11_focus_window_right())
    return true;
  return false;
}

bool wm_focus_window_up(void) {
  if (x11_env_is_wayland()) {
    if (focus_window_up_kwin())
      return true;
    if (focus_window_up_niri())
      return true;
    if (focus_window_up_hyprland())
      return true;
    if (focus_window_up_sway())
      return true;
  }
  if (x11_focus_window_up())
    return true;
  return false;
}

bool wm_focus_window_down(void) {
  if (x11_env_is_wayland()) {
    if (focus_window_down_kwin())
      return true;
    if (focus_window_down_niri())
      return true;
    if (focus_window_down_hyprland())
      return true;
    if (focus_window_down_sway())
      return true;
  }
  if (x11_focus_window_down())
    return true;
  return false;
}

bool wm_inject_key(int vk, int down) {
  if (x11_env_is_wayland()) {
    if (inject_key_kwin(vk, down))
      return true;
    if (inject_key_niri(vk, down))
      return true;
    if (inject_key_hyprland(vk, down))
      return true;
    if (inject_key_sway(vk, down))
      return true;
  }
  return false;
}

bool wm_minimize_active_window(void) {
  if (x11_env_is_wayland()) {
    if (minimize_active_window_kwin())
      return true;
    if (minimize_active_window_niri())
      return true;
    if (minimize_active_window_hyprland())
      return true;
    if (minimize_active_window_sway())
      return true;
  }
  if (x11_minimize_active_window())
    return true;
  return false;
}

bool wm_get_mouse_pos(int *x, int *y) {
  if (x11_env_is_wayland()) {
    if (get_mouse_pos_hyprland(x, y))
      return true;
    if (get_mouse_pos_kwin(x, y))
      return true;
    if (get_mouse_pos_niri(x, y))
      return true;
    if (get_mouse_pos_sway(x, y))
      return true;
  }
  if (x11_get_mouse_pos(x, y))
    return true;
  return false;
}

bool wm_toggle_maximize_active(void) {
  if (x11_env_is_wayland()) {
    if (toggle_maximize_active_kwin())
      return true;
    if (toggle_maximize_active_niri())
      return true;
    if (toggle_maximize_active_hyprland())
      return true;
    if (toggle_maximize_active_sway())
      return true;
  }
  if (x11_toggle_maximize_active())
    return true;
  return false;
}

bool wm_toggle_always_on_top_active(void) {
  if (x11_env_is_wayland()) {
    if (toggle_always_on_top_active_kwin())
      return true;
    if (toggle_always_on_top_active_niri())
      return true;
    if (toggle_always_on_top_active_hyprland())
      return true;
    if (toggle_always_on_top_active_sway())
      return true;
  }
  if (x11_toggle_always_on_top_active())
    return true;
  return false;
}

bool wm_raise_active_window(void) {
  if (x11_env_is_wayland()) {
    if (raise_active_window_kwin())
      return true;
    if (raise_active_window_niri())
      return true;
    if (raise_active_window_hyprland())
      return true;
    if (raise_active_window_sway())
      return true;
  }
  if (x11_raise_active_window())
    return true;
  return false;
}

bool wm_lower_active_window(void) {
  if (x11_env_is_wayland()) {
    if (lower_active_window_kwin())
      return true;
    if (lower_active_window_niri())
      return true;
    if (lower_active_window_hyprland())
      return true;
    if (lower_active_window_sway())
      return true;
  }
  if (x11_lower_active_window())
    return true;
  return false;
}

bool wm_center_active_window(void) {
  if (x11_env_is_wayland()) {
    if (center_active_window_kwin())
      return true;
    if (center_active_window_niri())
      return true;
    if (center_active_window_hyprland())
      return true;
    if (center_active_window_sway())
      return true;
  }
  if (x11_center_active_window())
    return true;
  return false;
}

bool wm_snap_active_window(SnapPreset preset) {
  if (x11_env_is_wayland()) {
    if (snap_active_window_kwin(preset))
      return true;
    if (snap_active_window_niri(preset))
      return true;
    if (snap_active_window_hyprland(preset))
      return true;
    if (snap_active_window_sway(preset))
      return true;
  }
  if (x11_snap_active_window(preset))
    return true;
  return false;
}

long wm_get_active_window_pid(void) {
  if (x11_env_is_wayland()) {
    long pid = get_pid_kwin();
    if (pid)
      return pid;
    pid = get_pid_niri();
    if (pid)
      return pid;
    pid = get_pid_hyprland();
    if (pid)
      return pid;
    pid = get_pid_sway();
    if (pid)
      return pid;
  }
  long pid = x11_get_active_window_pid();
  if (pid)
    return pid;
  return 0;
}

bool wm_switch_to_workspace(int index) {
  if (index < 1)
    return false;
  if (x11_env_is_wayland()) {
    if (switch_to_workspace_kwin(index))
      return true;
    if (switch_to_workspace_niri(index))
      return true;
    if (switch_to_workspace_hyprland(index))
      return true;
    if (switch_to_workspace_sway(index))
      return true;
  }
  if (x11_switch_to_workspace(index))
    return true;
  return false;
}

bool wm_move_active_window_to_workspace(int index) {
  if (index < 1)
    return false;
  if (x11_env_is_wayland()) {
    if (move_active_window_to_workspace_kwin(index))
      return true;
    if (move_active_window_to_workspace_niri(index))
      return true;
    if (move_active_window_to_workspace_hyprland(index))
      return true;
    if (move_active_window_to_workspace_sway(index))
      return true;
  }
  if (x11_move_active_window_to_workspace(index))
    return true;
  return false;
}

bool wm_set_active_window_opacity(int percent) {
  if (percent < 0 || percent > 100)
    return false;
  if (x11_env_is_wayland()) {
    if (set_active_window_opacity_kwin(percent))
      return true;
    if (set_active_window_opacity_niri(percent))
      return true;
    if (set_active_window_opacity_hyprland(percent))
      return true;
    if (set_active_window_opacity_sway(percent))
      return true;
  }
  if (x11_set_active_window_opacity(percent))
    return true;
  return false;
}

bool wm_close_windows_matching_title(const char *needle) {
  if (x11_env_is_wayland()) {
    if (close_windows_matching_title_kwin(needle))
      return true;
    if (close_windows_matching_title_niri(needle))
      return true;
    if (close_windows_matching_title_hyprland(needle))
      return true;
    if (close_windows_matching_title_sway(needle))
      return true;
  }
  if (x11_close_windows_by_title(needle))
    return true;
  return false;
}

bool wm_close_windows_matching_id(const char *needle) {
  if (x11_env_is_wayland()) {
    if (close_windows_matching_id_kwin(needle))
      return true;
    if (close_windows_matching_id_niri(needle))
      return true;
    if (close_windows_matching_id_hyprland(needle))
      return true;
    if (close_windows_matching_id_sway(needle))
      return true;
  }
  if (x11_close_windows_by_id(needle))
    return true;
  return false;
}

bool wm_focus_by_id(const char *id) {
  if (!id || !id[0])
    return false;
  if (x11_env_is_wayland()) {
    if (focus_window_id_kwin(id))
      return true;
    if (focus_window_id_niri(id))
      return true;
    if (focus_window_id_hyprland(id))
      return true;
    if (focus_window_id_sway(id))
      return true;
  }
  return x11_focus_window_id(id);
}

WindowHandle wm_get_active_window_handle(void) {
  if (x11_env_is_wayland()) {
    WindowHandle h = get_active_handle_kwin();
    if (!WH_EQUALS(h, WH_NULL))
      return h;
    h = get_active_handle_niri();
    if (!WH_EQUALS(h, WH_NULL))
      return h;
    h = get_active_handle_hyprland();
    if (!WH_EQUALS(h, WH_NULL))
      return h;
    h = get_active_handle_sway();
    if (!WH_EQUALS(h, WH_NULL))
      return h;
  }
  return x11_get_active_window_handle();
}

WindowHandle wm_find_window_by_id(const char *id) {
  if (!id || !id[0])
    return WH_NULL;
  if (!x11_env_is_wayland()) {
    unsigned long xid = x11_find_window_by_id(id);
    return xid ? WH_FROM_XID(xid) : WH_NULL;
  }
  if (wm_focus_by_id(id))
    return wm_get_active_window_handle();
  return WH_NULL;
}

WindowHandle wm_get_window_handle(const char *selector) {
  if (!selector || !selector[0])
    return WH_NULL;
  if (!x11_env_is_wayland()) {
    unsigned long xid = x11_find_window_by_title(selector);
    if (!xid)
      xid = x11_find_window_by_id(selector);
    return xid ? WH_FROM_XID(xid) : WH_NULL;
  }
  if (wm_focus_by_title(selector)) {
    WindowHandle h = wm_get_active_window_handle();
    if (!WH_EQUALS(h, WH_NULL))
      return h;
  }
  if (wm_focus_by_id(selector)) {
    WindowHandle h = wm_get_active_window_handle();
    if (!WH_EQUALS(h, WH_NULL))
      return h;
  }
  return WH_NULL;
}

bool wm_focus_window_handle(WindowHandle window) {
  if (WH_EQUALS(window, WH_NULL))
    return false;
  if (WH_HAS_ID(window))
    return wm_focus_by_id(WH_TO_ID(window));
  return x11_focus_window_xid(WH_TO_XID(window));
}

static bool wm_move_active_window_to_xy(int x, int y) {
  if (x11_env_is_wayland()) {
    if (move_active_window_to_xy_kwin(x, y))
      return true;
    if (move_active_window_to_xy_hyprland(x, y))
      return true;
    if (move_active_window_to_xy_sway(x, y))
      return true;
    if (move_active_window_to_xy_niri(x, y))
      return true;
  }
  return x11_move_active_window_to(x, y);
}

static bool wm_set_active_window_size(int w, int h) {
  if (x11_env_is_wayland()) {
    if (set_active_window_size_kwin(w, h))
      return true;
    if (set_active_window_size_hyprland(w, h))
      return true;
    if (set_active_window_size_sway(w, h))
      return true;
    if (set_active_window_size_niri(w, h))
      return true;
  }
  return x11_set_active_window_size(w, h);
}

static bool wm_get_active_window_geometry(DrawRect *out) {
  if (x11_env_is_wayland()) {
    if (get_active_window_geometry_kwin(out))
      return true;
    if (get_active_window_geometry_niri(out))
      return true;
    if (get_active_window_geometry_hyprland(out))
      return true;
    if (get_active_window_geometry_sway(out))
      return true;
  }
  return x11_get_active_window_geometry(out);
}

bool wm_move_window(WindowHandle window, int x, int y) {
  if (!WH_HAS_ID(window) && WH_TO_XID(window))
    return x11_move_window_xid(WH_TO_XID(window), x, y);
  if (!wm_focus_window_handle(window))
    return false;
  return wm_move_active_window_to_xy(x, y);
}

bool wm_set_window_size(WindowHandle window, int w, int h) {
  if (!WH_HAS_ID(window) && WH_TO_XID(window))
    return x11_set_window_size_xid(WH_TO_XID(window), w, h);
  if (!wm_focus_window_handle(window))
    return false;
  return wm_set_active_window_size(w, h);
}

bool wm_set_window_opacity_for(WindowHandle window, int percent) {
  if (!WH_HAS_ID(window) && WH_TO_XID(window))
    return x11_set_window_opacity_xid(WH_TO_XID(window), percent);
  if (!wm_focus_window_handle(window))
    return false;
  return wm_set_active_window_opacity(percent);
}

DrawRect wm_get_window_geometry(WindowHandle window) {
  DrawRect r = {0};
  if (!WH_HAS_ID(window) && WH_TO_XID(window)) {
    x11_get_window_geometry_xid(WH_TO_XID(window), &r);
    return r;
  }
  if (!wm_focus_window_handle(window))
    return r;
  wm_get_active_window_geometry(&r);
  return r;
}

bool wm_set_window_geometry(WindowHandle window, DrawRect rect) {
  if (!WH_HAS_ID(window) && WH_TO_XID(window))
    return x11_set_window_geometry_xid(WH_TO_XID(window), rect);
  if (rect.w <= 0 || rect.h <= 0)
    return false;
  if (!wm_focus_window_handle(window))
    return false;
  if (!wm_move_active_window_to_xy(rect.x, rect.y))
    return false;
  return wm_set_active_window_size(rect.w, rect.h);
}

WindowHandle wm_get_held_window(void) {
  int mx = 0, my = 0;
  if (!wm_get_mouse_pos(&mx, &my))
    return WH_NULL;
  WindowHandle h = wm_get_active_window_handle();
  if (WH_EQUALS(h, WH_NULL))
    return WH_NULL;
  DrawRect r = wm_get_window_geometry(h);
  if (r.w <= 0 || r.h <= 0)
    return WH_NULL;
  if (mx < r.x || my < r.y || mx >= r.x + r.w || my >= r.y + r.h)
    return WH_NULL;
  return h;
}
