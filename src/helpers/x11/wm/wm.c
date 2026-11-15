#include "wm/wm.h"

#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

#include "helpers/x11/env_helpers_x11.h"
#include "wm/kwin_wm.h"
#include "wm/niri_wm.h"
#include "wm/sway_wm.h"
#include "wm/hyprland_wm.h"
#include "wm/x11_wm.h"
#include "deps/log.h"

bool wm_append_window_name(PlWindowList *list, const char *name,
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
  log_error("Couldn't Get Active Window Title");
  return NULL;
}

char *wm_get_active_window_app_id(void) {
  static char buffer[1024];

  buffer[0] = '\0';

  if (x11_env_is_wayland()) {
    if (get_app_id_kwin(buffer, sizeof(buffer))) {
      if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0)
        return buffer;
    }
    if (get_app_id_niri(buffer, sizeof(buffer))) {
      if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0)
        return buffer;
    }
    if (get_app_id_hyprland(buffer, sizeof(buffer))) {
      if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0)
        return buffer;
    }
    if (get_app_id_sway(buffer, sizeof(buffer))) {
      if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0)
        return buffer;
    }
  }
  if (x11_get_app_id(buffer, sizeof(buffer))) {
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
  log_error("Couldn't Focus By Title %s", title);
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

PlWindowList Wm_GetWindowTitles(void) {
  PlWindowList result;
  PlWindowList empty = {.windows = NULL, .count = 0};
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
  log_error("GetWindowTitles Didn't Work");
  return empty;
}
PlWindowList Wm_GetWindowNames(void) {
  PlWindowList result;
  PlWindowList empty = {.windows = NULL, .count = 0};
  if (x11_env_is_wayland()) {
    result = get_window_names_kwin();
    if (result.windows != NULL)
      return result;
    result = get_window_names_niri();
    if (result.windows != NULL)
      return result;
    result = get_window_names_hyprland();
    if (result.windows != NULL)
      return result;
    result = get_window_names_sway();
    if (result.windows != NULL)
      return result;
  }
  result = x11_get_window_names();
  if (result.windows != NULL)
    return result;
  log_error("GetWindowNames Didn't Work");
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
  log_error("CloseActiveWindow Didn't Work");
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
  log_error("ToggleFullscreenActive Didn't Work");
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
  if (x11_move_active_window_to_monitor(right))
    return true;
  log_error("MoveActiveWindowToMonitor Didn't Work");
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
  if (x11_move_active_window_left())
    return true;
  log_error("MoveActiveWindowLeft Didn't Work");
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
  if (x11_move_active_window_right())
    return true;
  log_error("MoveActiveWindowRight Didn't Work");
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
  log_error("MoveActiveWindowUp Didn't Work");
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
  log_error("MoveActiveWindowDown Didn't Work");
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
  log_error("FocusWindowLeft Didn't Work");
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
  log_error("FocusWindowRight Didn't Work");
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
  log_error("FocusWindowUp Didn't Work");
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
  log_error("FocusWindowDown Didn't Work");
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
  log_error("MinimizeActiveWindow Didn't Work");
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
