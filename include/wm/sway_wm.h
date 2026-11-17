#ifndef SWAY_WM_H
#define SWAY_WM_H
#include <stdbool.h>

#include "platform/platform.h"
#include "core/types.h"
#include "core/types/window.h"

bool get_title_sway(char *buffer, int buffer_size);
bool get_id_sway(char *buffer, int buffer_size);
char *paste_sway(const char *content);
bool focus_window_title_sway(const char *title);
WindowList get_window_titles_sway(void);
WindowList get_window_ids_sway(void);
bool close_active_window_sway(void);
bool switch_to_workspace_sway(int index);
bool move_active_window_to_workspace_sway(int index);
bool set_active_window_opacity_sway(int percent);
bool close_windows_matching_title_sway(const char *needle);
bool close_windows_matching_id_sway(const char *needle);
bool toggle_fullscreen_active_sway(void);
bool move_active_window_to_monitor_sway(bool right);
bool move_active_window_left_sway(void);
bool move_active_window_up_sway(void);
bool move_active_window_down_sway(void);
bool move_active_window_right_sway(void);
bool focus_window_left_sway(void);
bool focus_window_up_sway(void);
bool focus_window_down_sway(void);
bool focus_window_right_sway(void);
bool minimize_active_window_sway(void);
bool toggle_maximize_active_sway(void);
bool toggle_always_on_top_active_sway(void);
bool raise_active_window_sway(void);
bool lower_active_window_sway(void);
bool center_active_window_sway(void);
bool snap_active_window_sway(SnapPreset preset);
long get_pid_sway(void);
bool inject_key_sway(int vk, int down);
bool get_mouse_pos_sway(int *x, int *y);

#endif
