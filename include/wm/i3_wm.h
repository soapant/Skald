#ifndef I3_WM_H
#define I3_WM_H
#include <stdbool.h>
#include "platform/platform.h"
#include "core/types.h"

bool i3_is_active(void);
bool get_title_i3(char *buffer, int buffer_size);
bool get_app_id_i3(char *buffer, int buffer_size);
char* paste_i3(char* content);
bool focus_window_title_i3(char* title);
Fl_WindowList get_window_titles_i3();
Fl_WindowList get_window_names_i3();
Fl_Pos get_cursor_position_i3(void);
bool close_active_window_i3(void);
bool toggle_fullscreen_active_i3(void);
bool move_active_window_to_monitor_i3(bool right);
bool move_active_window_left_i3(void);
bool move_active_window_right_i3(void);
bool focus_window_left_i3(void);
bool focus_window_right_i3(void);
bool minimize_active_window_i3(void);

#endif

