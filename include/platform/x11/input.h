#ifndef PLATFORM_X11_INPUT_H
#define PLATFORM_X11_INPUT_H

#include <stdbool.h>
#include <linux/input.h>
#include <X11/keysym.h>
#include "core/types.h"

#define X11_MAX_KEYBOARDS 16

typedef struct {
    int fd;
    char path[256];
} X11Keyboard;

// Shared state - single instance owned by input.c
extern X11Keyboard g_x11_keyboards[X11_MAX_KEYBOARDS];
extern int g_x11_keyboard_count;
extern int g_x11_uinput_fd;
extern bool g_x11_suppress_release[KEY_MAX + 1];
extern bool g_x11_device_grabbed[X11_MAX_KEYBOARDS];

// Common modifier state (unified)
extern bool g_x11_ctrl_held;
extern bool g_x11_alt_held;
extern bool g_x11_shift_held;

// Legacy aliases for existing code that uses g_linux_* / g_Linux_* / active_kbs etc
#define g_linux_ctrl_held g_x11_ctrl_held
#define g_linux_alt_held g_x11_alt_held
#define g_linux_shift_held g_x11_shift_held
#define g_Linux_CtrlHeld g_x11_ctrl_held
#define g_Linux_AltHeld g_x11_alt_held
#define g_Linux_ShiftHeld g_x11_shift_held
// old keyboard arrays
#define active_kbs g_x11_keyboards
#define active_kb_count g_x11_keyboard_count
#define g_uinput_fd g_x11_uinput_fd
#define g_suppress_release g_x11_suppress_release
#define g_device_grabbed g_x11_device_grabbed
#define MAX_KEYBOARDS X11_MAX_KEYBOARDS
#define SystemKeyboard X11Keyboard

bool x11_should_use_grab(void);
int x11_init_uinput(void);
void x11_destroy_uinput(void);
void x11_uinput_emit(const struct input_event *ev);
void x11_scan_keyboards(void);
int x11_keysym_to_vk(KeySym ks);
int x11_vk_to_linux_key(int vk);
bool x11_handle_global_key(int key_code, int value);
void x11_cleanup_keyboards(void);

#endif
