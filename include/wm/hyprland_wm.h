#ifndef HYPRLAND_WM_H
#define HYPRLAND_WM_H
#include <stdbool.h>
#include "platform/platform.h"
#include "core/types.h"
bool get_title_hyprland(char *buffer, int buffer_size);
bool get_app_id_hyprland(char *buffer, int buffer_size);
char* paste_hyprland(char* content);
bool focus_window_title_hyprland(char* title);
FlWindowList get_window_titles_hyprland();
FlWindowList get_window_names_hyprland();
FlPos get_cursor_position_hyprland(void);
bool close_active_window_hyprland(void);
bool toggle_fullscreen_active_hyprland(void);
bool move_active_window_to_monitor_hyprland(bool right);
bool move_active_window_left_hyprland(void);
bool move_active_window_right_hyprland(void);
bool focus_window_left_hyprland(void);
bool focus_window_right_hyprland(void);
bool minimize_active_window_hyprland(void);
#endif
