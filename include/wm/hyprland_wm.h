#ifndef HYPRLAND_WM_H
#define HYPRLAND_WM_H
#include <stdbool.h>

#include "platform/platform.h"
#include "core/types.h"
#include "core/types/window.h"

bool get_title_hyprland(char *buffer, int buffer_size);
bool get_app_id_hyprland(char *buffer, int buffer_size);
char *paste_hyprland(const char *content);
bool focus_window_title_hyprland(const char *title);
PlWindowList get_window_titles_hyprland(void);
PlWindowList get_window_names_hyprland(void);
bool close_active_window_hyprland(void);
bool toggle_fullscreen_active_hyprland(void);
bool move_active_window_to_monitor_hyprland(bool right);
bool move_active_window_left_hyprland(void);
bool move_active_window_up_hyprland(void);
bool move_active_window_down_hyprland(void);
bool move_active_window_right_hyprland(void);
bool focus_window_left_hyprland(void);
bool focus_window_up_hyprland(void);
bool focus_window_down_hyprland(void);
bool focus_window_right_hyprland(void);
bool minimize_active_window_hyprland(void);
bool inject_key_hyprland(int vk, int down);
bool get_mouse_pos_hyprland(int *x, int *y);
#endif
