#ifndef PLATFORM_X11_INPUT_PRIV_H
#define PLATFORM_X11_INPUT_PRIV_H

#include <linux/input.h>
#include <stdbool.h>

#include "platform/x11/input_x11.h"

extern X11Keyboard g_x11_keyboards[X11_MAX_KEYBOARDS];
extern int g_x11_keyboard_count;
extern X11Keyboard g_x11_mice[X11_MAX_MICE];
extern int g_x11_mouse_count;
extern int g_x11_uinput_fd;
extern int g_x11_uinput_mouse_fd;
extern bool g_x11_suppress_release[KEY_MAX + 1];
extern bool g_x11_device_grabbed[X11_MAX_KEYBOARDS];
extern bool g_x11_mouse_grabbed[X11_MAX_MICE];

extern bool g_x11_ctrl_held;
extern bool g_x11_alt_held;
extern bool g_x11_shift_held;

#endif
