#ifndef X11_WM_H
#define X11_WM_H
#include <stdbool.h>

#include "platform/platform.h"
#include "core/types.h"
#include "core/types/window.h"

bool x11_get_title(char *buffer, int buffer_size);
bool x11_get_id(char *buffer, int buffer_size);
char *x11_paste(const char *content);
bool x11_focus_window_title(const char *title);
unsigned long x11_find_window_by_title(const char *title);
PlWindowList x11_get_window_titles(void);
PlWindowList x11_get_window_names(void);
void x11_wm_cleanup_cursor_display(void);
bool x11_close_active_window(void);
bool x11_switch_to_workspace(int index);
bool x11_move_active_window_to_workspace(int index);
bool x11_set_active_window_opacity(int percent);
bool x11_close_windows_by_title(const char *needle);
bool x11_close_windows_by_id(const char *needle);
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
bool x11_toggle_maximize_active(void);
bool x11_toggle_always_on_top_active(void);
bool x11_raise_active_window(void);
bool x11_lower_active_window(void);
bool x11_center_active_window(void);
bool x11_snap_active_window(SnapPreset preset);
long x11_get_active_window_pid(void);
bool x11_get_mouse_pos(int *x, int *y);
#endif
