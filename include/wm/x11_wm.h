#ifndef X11_WM_H
#define X11_WM_H
#include <stdbool.h>
#include "platform/platform.h"
#include "core/types.h"
bool get_title_x11(char *buffer, int buffer_size);
bool get_app_id_x11(char *buffer, int buffer_size);
char* paste_x11(char* content);
bool focus_window_title_x11(char* title);
PlWindowList get_window_titles_x11();
PlWindowList get_window_names_x11();
void x11_wm_cleanup_cursor_display(void);
bool close_active_window_x11(void);
bool toggle_fullscreen_active_x11(void);
bool move_active_window_to_monitor_x11(bool right);
bool move_active_window_left_x11(void);
bool move_active_window_right_x11(void);
bool focus_window_left_x11(void);
bool focus_window_right_x11(void);
bool minimize_active_window_x11(void);
#endif
