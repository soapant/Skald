#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <X11/X.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <stdbool.h>

#include "helpers/x11/input_x11.h"
#include "deps/log.h"
#include "core/hotkey_manager.h"
#include "helpers/helpers_hotkey.h"
#include "core/remap_manager.h"
#include "core/snippet_manager.h"
#include "platform/platform.h"
#include "helpers/x11/env_helpers_x11.h"
#include "helpers/x11/misc_x11.h"
#include "wm/wm.h"
#include "helpers/helpers_input_state.h"
#include "core/types/keys.h"
#include <linux/uinput.h>
#include <errno.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <signal.h>

#include "helpers/x11/display_x11.h"
#include "core/hotkey_pending.h"

bool Platform_GetMousePos(int *out_x, int *out_y) {
  int mx = 0, my = 0;
  if (x11_env_is_wayland() && Wm_GetMousePos(&mx, &my)) {
    if (out_x)
      *out_x = mx;
    if (out_y)
      *out_y = my;
    return true;
  }
  Display *dpy = x11_get_display();
  if (!dpy)
    return false;
  Window root = DefaultRootWindow(dpy);
  Window rw = 0, cw = 0;
  int rx = 0, ry = 0, wx = 0, wy = 0;
  int mask = 0;
  if (!XQueryPointer(dpy, root, &rw, &cw, &rx, &ry, &wx, &wy, &mask))
    return false;
  if (out_x)
    *out_x = rx;
  if (out_y)
    *out_y = ry;
  return true;
}
int Platform_CharForKey(int vk, int scancode, int mods) {
  Display *dpy = x11_get_display();
  if (!dpy)
    return 0;
  KeySym ks = 0;
  char buf[32];
  XKeyEvent xe;
  memset(&xe, 0, sizeof(xe));
  xe.type = KeyPress;
  xe.display = dpy;
  xe.keycode = (KeyCode)(scancode + 8);
  if (mods & HOTKEY_MOD_SHIFT)
    xe.state |= ShiftMask;
  if (mods & HOTKEY_MOD_CTRL)
    xe.state |= ControlMask;
  if (mods & HOTKEY_MOD_ALT)
    xe.state |= Mod1Mask;
  int len = XLookupString(&xe, buf, sizeof(buf), &ks, NULL);
  if (len <= 0)
    return 0;
  return (int)(unsigned char)buf[0];
}
