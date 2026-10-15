#ifndef SWAY_WM_H
#define SWAY_WM_H
#include <stdbool.h>
#include "platform/platform.h"
#include "core/types.h"
bool get_title_sway(char *buffer, int buffer_size);
bool get_app_id_sway(char *buffer, int buffer_size);
char* paste_sway(char* content);
bool focus_window_title_sway(char* title);
PlWindowList get_window_titles_sway();
PlWindowList get_window_names_sway();
bool close_active_window_sway(void);
bool toggle_fullscreen_active_sway(void);
bool move_active_window_to_monitor_sway(bool right);
bool move_active_window_left_sway(void);
bool move_active_window_right_sway(void);
bool focus_window_left_sway(void);
bool focus_window_right_sway(void);
bool minimize_active_window_sway(void);

#endif

