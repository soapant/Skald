#ifndef X11_WM_H
#define X11_WM_H
#include <stdbool.h>

#include "platform/platform.h"
#include "core/types.h"
#include "core/types/window.h"

bool x11_get_title(char *buffer, int buffer_size);
bool x11_get_app_id(char *buffer, int buffer_size);
char *x11_paste(const char *content);
bool x11_focus_window_title(const char *title);
unsigned long x11_find_window_by_title(const char *title);
PlWindowList x11_get_window_titles(void);
PlWindowList x11_get_window_names(void);
void x11_wm_cleanup_cursor_display(void);
bool x11_close_active_window(void);
bool x11_toggle_fullscreen_active(void);
bool x11_move_active_window_to_monitor(bool right);
bool x11_move_active_window_left(void);
bool x11_move_active_window_right(void);
bool x11_move_active_window_up(void);
bool x11_move_active_window_down(void);
bool x11_focus_window_left(void);
bool x11_focus_window_right(void);
bool x11_focus_window_up(void);
bool x11_focus_window_down(void);
bool x11_minimize_active_window(void);
bool x11_get_mouse_pos(int *x, int *y);
#endif
