#ifndef SWAY_WM_H
#define SWAY_WM_H
#include <stdbool.h>
#include "core/platform.h"
#include "core/types.h"
bool get_title_sway(char *buffer, int buffer_size);
bool get_app_id_sway(char *buffer, int buffer_size);
char* paste_sway(char* content);
bool focus_window_title_sway(char* title);
Fl_WindowList get_window_titles_sway();
Fl_WindowList get_window_names_sway();
Fl_Pos get_cursor_position_sway(void);
bool close_active_window_sway(void);
bool toggle_fullscreen_active_sway(void);
bool move_active_window_to_monitor_sway(bool right);
bool minimize_active_window_sway(void);

#endif

