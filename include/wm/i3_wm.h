#ifndef I3_WM_H
#define I3_WM_H
#include <stdbool.h>

bool i3_is_active(void);
bool move_active_window_left_i3(void);
bool move_active_window_right_i3(void);
bool move_active_window_to_monitor_i3(bool right);

#endif
