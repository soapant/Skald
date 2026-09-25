#ifndef PLATFORM_X11_INPUT_H
#define PLATFORM_X11_INPUT_H

#include <stdbool.h>
#include <linux/input.h>
#include <X11/keysym.h>
#include "core/types.h"
#include <X11/X.h>
#include <X11/Xlib.h>  

#define X11_MAX_KEYBOARDS 16

typedef struct {
    int fd;
    char path[256];
} X11Keyboard;


extern X11Keyboard g_x11_keyboards[X11_MAX_KEYBOARDS];
extern int g_x11_keyboard_count;
extern int g_x11_uinput_fd;
extern bool g_x11_suppress_release[KEY_MAX + 1];
extern bool g_x11_device_grabbed[X11_MAX_KEYBOARDS];

extern bool g_x11_ctrl_held;
extern bool g_x11_alt_held;
extern bool g_x11_shift_held;

void x11_sync_held_from_phys(void);
void x11_clear_suppress(void);
bool x11_is_suppressed(int linux_code);
void x11_set_suppress(int linux_code, bool v);
bool x11_handle_evdev(struct input_event *ev, bool has_popup, bool grabbed);
struct WindowManager;
void x11_set_window_manager(struct WindowManager *wm);

bool x11_should_use_grab(void);
int x11_init_uinput(void);
void x11_destroy_uinput(void);
void x11_uinput_emit(const struct input_event *ev);
void x11_scan_keyboards(void);
int x11_keysym_to_vk(KeySym ks);
int x11_vk_to_linux_key(int vk);
bool x11_handle_global_key(int key_code, int value);

extern int pending_hotkey_vk;
extern int pending_hotkey_mod;
extern long pending_hotkey_timeMs;

bool should_swallow_pending(int wParam, int mods);
int x11_keysym_to_windows_vk(KeySym keysym);
void ReadModifiers(const XEvent* ev, bool* ctrl_pressed, bool* shift_pressed, bool* alt_pressed);
bool TryVimMotion(int wParam, bool ctrl_pressed, bool shift_pressed, bool alt_pressed);

void HandleCommandExecution(ListBoxInfo* lb, char* string);

#endif
