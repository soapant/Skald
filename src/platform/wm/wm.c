#include "wm/wm.h"

#include <strings.h>
#include <stdlib.h>

#include "platform/x11/env_helpers_x11.h"
#include "wm/kwin_wm.h"
#include "wm/niri_wm.h"
#include "wm/sway_wm.h"
#include "wm/hyprland_wm.h"
#include "wm/x11_wm.h"
#include "deps/log.h"

char *Wm_GetActiveWindowTitle(void) {
  static char buffer[1024];

  buffer[0] = '\0';

  if (Env_IsWayland()) {
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
  if (get_title_x11(buffer, sizeof(buffer))) {
    if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0)
      return buffer;
  }
  log_error("Coulnt Get Active Window Title");
  return NULL;
}

char *Wm_GetActiveWindowAppId(void) {
  static char buffer[1024];

  buffer[0] = '\0';

  if (Env_IsWayland()) {
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
  if (get_app_id_x11(buffer, sizeof(buffer))) {
    if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0)
      return buffer;
  }
  return NULL;
}

bool Wm_FocusByTitle(const char *title) {
  if (Env_IsWayland()) {
    if (focus_window_title_kwin((char *)title)) {
      return true;
    }
    if (focus_window_title_niri((char *)title)) {
      return true;
    }
    if (focus_window_title_hyprland(title)) {
      return true;
    }
    if (focus_window_title_sway(title)) {
      return true;
    }
  }
  if (focus_window_title_x11(title)) {
    return true;
  }
  log_error("Coulnt Focus By TItle %s", title);
  return NULL;
}

char *Wm_Paste(char *content) {
  char *result;
  if (Env_IsWayland()) {
    if ((result = paste_kwin(content)))
      return result;
    if ((result = paste_niri(content)))
      return result;
    if ((result = paste_hyprland(content)))
      return result;
    if ((result = paste_sway(content)))
      return result;
  }
  if ((result = paste_x11(content)))
    return result;
  log_error("content='%s'", content ? content : "null");
  return NULL;
}

PlWindowList Wm_GetWindowTitles() {
  PlWindowList result;
  PlWindowList empty = {.windows = NULL, .count = 0};
  if (Env_IsWayland()) {
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
  result = get_window_titles_x11();
  if (result.windows != NULL)
    return result;
  log_error("GetWindowTitles Didnt Work");
  return empty;
}
PlWindowList Wm_GetWindowNames() {
  PlWindowList result;
  PlWindowList empty = {.windows = NULL, .count = 0};
  if (Env_IsWayland()) {
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
  result = get_window_names_x11();
  if (result.windows != NULL)
    return result;
  log_error("GetWindowNames Didnt Work");
  return empty;
}

bool Wm_CloseActiveWindow(void) {
  if (Env_IsWayland()) {
    if (close_active_window_kwin())
      return true;
    if (close_active_window_niri())
      return true;
    if (close_active_window_hyprland())
      return true;
    if (close_active_window_sway())
      return true;
  }
  if (close_active_window_x11())
    return true;
  log_error("CloseActiveWindow Didnt Work");
  return false;
}

bool Wm_ToggleFullscreenActive(void) {
  if (Env_IsWayland()) {
    if (toggle_fullscreen_active_kwin())
      return true;
    if (toggle_fullscreen_active_niri())
      return true;
    if (toggle_fullscreen_active_hyprland())
      return true;
    if (toggle_fullscreen_active_sway())
      return true;
  }
  if (toggle_fullscreen_active_x11())
    return true;
  log_error("ToggleFullscreenActive Didnt Work");
  return false;
}

bool Wm_MoveActiveWindowToMonitor(bool right) {
  if (Env_IsWayland()) {
    if (move_active_window_to_monitor_kwin(right))
      return true;
    if (move_active_window_to_monitor_niri(right))
      return true;
    if (move_active_window_to_monitor_hyprland(right))
      return true;
    if (move_active_window_to_monitor_sway(right))
      return true;
  }
  if (move_active_window_to_monitor_x11(right))
    return true;
  log_error("MoveActiveWindowToMonitor Didnt Work");
  return false;
}

bool Wm_MoveActiveWindowLeft(void) {
  if (Env_IsWayland()) {
    if (move_active_window_left_kwin())
      return true;
    if (move_active_window_left_niri())
      return true;
    if (move_active_window_left_hyprland())
      return true;
    if (move_active_window_left_sway())
      return true;
  }
  if (move_active_window_left_x11())
    return true;
  log_error("MoveActiveWindowLeft Didnt Work");
  return false;
}

bool Wm_MoveActiveWindowRight(void) {
  if (Env_IsWayland()) {
    if (move_active_window_right_kwin())
      return true;
    if (move_active_window_right_niri())
      return true;
    if (move_active_window_right_hyprland())
      return true;
    if (move_active_window_right_sway())
      return true;
  }
  if (move_active_window_right_x11())
    return true;
  log_error("MoveActiveWindowRight Didnt Work");
  return false;
}

bool Wm_MoveActiveWindowUp(void) {
  if (Env_IsWayland()) {
    if (move_active_window_up_kwin())
      return true;
    if (move_active_window_up_niri())
      return true;
    if (move_active_window_up_hyprland())
      return true;
    if (move_active_window_up_sway())
      return true;
  }
  if (move_active_window_up_x11())
    return true;
  log_error("MoveActiveWindowUp Didnt Work");
  return false;
}

bool Wm_MoveActiveWindowDown(void) {
  if (Env_IsWayland()) {
    if (move_active_window_down_kwin())
      return true;
    if (move_active_window_down_niri())
      return true;
    if (move_active_window_down_hyprland())
      return true;
    if (move_active_window_down_sway())
      return true;
  }
  if (move_active_window_down_x11())
    return true;
  log_error("MoveActiveWindowDown Didnt Work");
  return false;
}

bool Wm_FocusWindowLeft(void) {
  if (Env_IsWayland()) {
    if (focus_window_left_kwin())
      return true;
    if (focus_window_left_niri())
      return true;
    if (focus_window_left_hyprland())
      return true;
    if (focus_window_left_sway())
      return true;
  }
  if (focus_window_left_x11())
    return true;
  log_error("FocusWindowLeft Didnt Work");
  return false;
}

bool Wm_FocusWindowRight(void) {
  if (Env_IsWayland()) {
    if (focus_window_right_kwin())
      return true;
    if (focus_window_right_niri())
      return true;
    if (focus_window_right_hyprland())
      return true;
    if (focus_window_right_sway())
      return true;
  }
  if (focus_window_right_x11())
    return true;
  log_error("FocusWindowRight Didnt Work");
  return false;
}

bool Wm_FocusWindowUp(void) {
  if (Env_IsWayland()) {
    if (focus_window_up_kwin())
      return true;
    if (focus_window_up_niri())
      return true;
    if (focus_window_up_hyprland())
      return true;
    if (focus_window_up_sway())
      return true;
  }
  if (focus_window_up_x11())
    return true;
  log_error("FocusWindowUp Didnt Work");
  return false;
}

bool Wm_FocusWindowDown(void) {
  if (Env_IsWayland()) {
    if (focus_window_down_kwin())
      return true;
    if (focus_window_down_niri())
      return true;
    if (focus_window_down_hyprland())
      return true;
    if (focus_window_down_sway())
      return true;
  }
  if (focus_window_down_x11())
    return true;
  log_error("FocusWindowDown Didnt Work");
  return false;
}

bool Wm_MinimizeActiveWindow(void) {
  if (Env_IsWayland()) {
    if (minimize_active_window_kwin())
      return true;
    if (minimize_active_window_niri())
      return true;
    if (minimize_active_window_hyprland())
      return true;
    if (minimize_active_window_sway())
      return true;
  }
  if (minimize_active_window_x11())
    return true;
  log_error("MinimizeActiveWindow Didnt Work");
  return false;
}

bool Wm_GetMousePos(int *x, int *y) {
  if (Env_IsWayland()) {
    if (get_mouse_pos_hyprland(x, y))
      return true;
    if (get_mouse_pos_kwin(x, y))
      return true;
    if (get_mouse_pos_niri(x, y))
      return true;
    if (get_mouse_pos_sway(x, y))
      return true;
  }
  if (get_mouse_pos_x11(x, y))
    return true;
  return false;
}
