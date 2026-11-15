#ifndef NIRI_WM_H
#define NIRI_WM_H
#include <stdbool.h>

#include "platform/platform.h"
#include "core/types.h"
#include "core/types/window.h"

bool niri_is_active(void);
bool get_title_niri(char *buffer, int buffer_size);
bool get_id_niri(char *buffer, int buffer_size);
char *paste_niri(const char *content);
bool focus_window_title_niri(const char *title);
PlWindowList get_window_titles_niri(void);
PlWindowList get_window_ids_niri(void);
bool close_active_window_niri(void);
bool switch_to_workspace_niri(int index);
bool move_active_window_to_workspace_niri(int index);
bool set_active_window_opacity_niri(int percent);
bool close_windows_matching_title_niri(const char *needle);
bool close_windows_matching_id_niri(const char *needle);
bool toggle_fullscreen_active_niri(void);
bool move_active_window_to_monitor_niri(bool right);
bool move_active_window_left_niri(void);
bool move_active_window_up_niri(void);
bool move_active_window_down_niri(void);
bool move_active_window_right_niri(void);
bool focus_window_left_niri(void);
bool focus_window_up_niri(void);
bool focus_window_down_niri(void);
bool focus_window_right_niri(void);
bool minimize_active_window_niri(void);
bool toggle_maximize_active_niri(void);
bool toggle_always_on_top_active_niri(void);
bool raise_active_window_niri(void);
bool lower_active_window_niri(void);
bool center_active_window_niri(void);
bool snap_active_window_niri(SnapPreset preset);
long get_pid_niri(void);
bool inject_key_niri(int vk, int down);
bool get_mouse_pos_niri(int *x, int *y);

#endif
