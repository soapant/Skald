#ifndef X11_WM_H
#define X11_WM_H
#include <stdbool.h>
#include "core/platform.h"
#include "core/types.h"
bool get_title_x11(char *buffer, int buffer_size);
bool get_app_id_x11(char *buffer, int buffer_size);
char* paste_x11(char* content);
bool focus_window_title_x11(char* title);
Fl_WindowList get_window_titles_x11();
Fl_WindowList get_window_names_x11();
Fl_Pos get_cursor_position_x11(void);
bool close_active_window_x11(void);
bool toggle_fullscreen_active_x11(void);
bool move_active_window_to_monitor_x11(bool right);
bool minimize_active_window_x11(void);
#endif
