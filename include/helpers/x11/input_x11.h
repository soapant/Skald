#ifndef INPUT_X11_H
#define INPUT_X11_H

#include <stdbool.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <X11/keysym.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include "core/types.h"

int x11_keyboard_count_get(void);
int x11_mouse_count_get(void);
int x11_keyboard_fd(int i);
int x11_mouse_fd(int i);
void x11_keyboard_fd_set(int i, int fd);
bool x11_device_grabbed_get(int i);
void x11_device_grabbed_set(int i, bool v);
bool x11_mouse_grabbed_get(int i);
int x11_uinput_fd_get(void);
void x11_uinput_fd_set(int fd);

struct input_event;

#define X11_MAX_KEYBOARDS 16
#define X11_MAX_MICE 8

typedef struct {
  int fd;
  char path[256];
} X11Keyboard;

void x11_clear_suppress(void);
bool x11_is_suppressed(int linux_code);
void x11_set_suppress(int linux_code, bool v);
bool x11_handle_evdev(struct input_event *ev, bool has_popup, bool grabbed);
bool x11_handle_evdev_mouse(struct input_event *ev);
struct WindowManager;

void x11_set_window_manager(struct WindowManager *wm);

bool x11_should_use_grab(void);
bool x11_grab_blocked(void);
int x11_init_uinput(void);
void x11_destroy_uinput(void);
void x11_uinput_emit(const struct input_event *ev);
void x11_mouse_grab_begin(void);
void x11_mouse_grab_end(void);
void x11_mouse_forward(const struct input_event *ev, int grabbed);
void x11_scan_keyboards(void);
int x11_keysym_to_vk(KeySym ks);
int x11_vk_to_linux_key(int vk);
bool x11_handle_global_key(int key_code, int value);

#endif
