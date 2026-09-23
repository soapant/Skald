#ifndef KWIN_WM_H
#define KWIN_WM_H
#include <stdbool.h>
#include "core/platform.h"
#include "core/types.h"
bool kwin_is_active(void);
bool get_title_kwin(char *buffer, int buffer_size);
bool get_app_id_kwin(char *buffer, int buffer_size);
char* paste_kwin(char* content);
bool focus_window_title_kwin(char* title);
Fl_WindowList get_window_titles_kwin();
Fl_WindowList get_window_names_kwin();
Fl_Pos get_cursor_position_kwin(void);
bool close_active_window_kwin(void);
bool toggle_fullscreen_active_kwin(void);
bool move_active_window_to_monitor_kwin(bool right);
bool move_active_window_left_kwin(void);
bool move_active_window_right_kwin(void);
bool focus_window_left_kwin(void);
bool focus_window_right_kwin(void);
bool minimize_active_window_kwin(void);

#endif
