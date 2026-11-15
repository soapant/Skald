#ifndef KWIN_WM_H
#define KWIN_WM_H
#include <stdbool.h>

#include "platform/platform.h"
#include "core/types.h"
#include "core/types/window.h"

bool kwin_is_active(void);
bool get_title_kwin(char *buffer, int buffer_size);
bool get_id_kwin(char *buffer, int buffer_size);
char *paste_kwin(const char *content);
bool focus_window_title_kwin(const char *title);
PlWindowList get_window_titles_kwin(void);
PlWindowList get_window_ids_kwin(void);
bool close_active_window_kwin(void);
bool switch_to_workspace_kwin(int index);
bool move_active_window_to_workspace_kwin(int index);
bool set_active_window_opacity_kwin(int percent);
bool close_windows_matching_title_kwin(const char *needle);
bool close_windows_matching_id_kwin(const char *needle);
bool toggle_fullscreen_active_kwin(void);
bool move_active_window_to_monitor_kwin(bool right);
bool move_active_window_left_kwin(void);
bool move_active_window_up_kwin(void);
bool move_active_window_down_kwin(void);
bool move_active_window_right_kwin(void);
bool focus_window_left_kwin(void);
bool focus_window_up_kwin(void);
bool focus_window_down_kwin(void);
bool focus_window_right_kwin(void);
bool minimize_active_window_kwin(void);
bool toggle_maximize_active_kwin(void);
bool toggle_always_on_top_active_kwin(void);
bool raise_active_window_kwin(void);
bool lower_active_window_kwin(void);
bool center_active_window_kwin(void);
bool snap_active_window_kwin(SnapPreset preset);
long get_pid_kwin(void);
bool inject_key_kwin(int vk, int down);
bool get_mouse_pos_kwin(int *x, int *y);

#endif
