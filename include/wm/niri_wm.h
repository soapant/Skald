#ifndef NIRI_WM_H
#define NIRI_WM_H
#include <stdbool.h>
#include "platform/platform.h"
#include "core/types.h"
bool niri_is_active(void);
bool get_title_niri(char *buffer, int buffer_size);
bool get_app_id_niri(char *buffer, int buffer_size);
char* paste_niri(char* content);
bool focus_window_title_niri(char* title);
PlWindowList get_window_titles_niri();
PlWindowList get_window_names_niri();
bool close_active_window_niri(void);
bool toggle_fullscreen_active_niri(void);
bool move_active_window_to_monitor_niri(bool right);
bool move_active_window_left_niri(void);
bool move_active_window_right_niri(void);
bool focus_window_left_niri(void);
bool focus_window_right_niri(void);
bool minimize_active_window_niri(void);

#endif
