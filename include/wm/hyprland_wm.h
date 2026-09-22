#ifndef HYPRLAND_WM_H
#define HYPRLAND_WM_H
#include <stdbool.h>
#include "core/platform.h"
#include "core/types.h"
bool get_title_hyprland(char *buffer, int buffer_size);
bool get_app_id_hyprland(char *buffer, int buffer_size);
char* paste_hyprland(char* content);
bool focus_window_title_hyprland(char* title);
Fl_WindowList get_window_titles_hyprland();
Fl_WindowList get_window_names_hyprland();
Fl_Pos get_cursor_position_hyprland(void);
bool close_active_window_hyprland(void);
bool toggle_fullscreen_active_hyprland(void);
bool move_active_window_to_monitor_hyprland(bool right);
bool minimize_active_window_hyprland(void);
#endif
