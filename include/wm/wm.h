#ifndef WINDOW_PLATFORM_H
#define WINDOW_PLATFORM_H

#include <stdbool.h>
#include <ctype.h>
#include <string.h>

char *wm_trim_focus_title(const char *title);

#include "core/types.h"
#include "core/types/window.h"

#include "helpers/helpers_text.h"

bool wm_append_window_name(PlWindowList *list, const char *name,
                           bool toupper_first);

char *wm_get_active_window_title(void);
char *wm_get_active_window_app_id(void);
bool wm_focus_by_title(const char *title);
char *wm_paste(const char *content);
PlWindowList Wm_GetWindowTitles(void);
PlWindowList Wm_GetWindowNames(void);
bool wm_close_active_window(void);
bool wm_toggle_fullscreen_active(void);
bool wm_move_active_window_to_monitor(bool right);
bool wm_move_active_window_left(void);
bool wm_move_active_window_up(void);
bool wm_move_active_window_down(void);
bool wm_move_active_window_right(void);
bool wm_focus_window_left(void);
bool wm_focus_window_up(void);
bool wm_focus_window_down(void);
bool wm_focus_window_right(void);
bool wm_minimize_active_window(void);
bool wm_inject_key(int vk, int down);
bool wm_get_mouse_pos(int *x, int *y);

#endif
