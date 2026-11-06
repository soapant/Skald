#include "platform/platform.h"
#include "platform/x11/input_x11.h"
#include "platform/x11/display_x11.h"
#include "deps/log.h"
#include "core/types/keys.h"
#include <linux/input.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <linux/input-event-codes.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#if __has_include(<X11/extensions/XTest.h>)
#include <X11/extensions/XTest.h>
#endif

static int g_inject_backend = 0; /* 0=auto,1=uinput,2=wtype,3=ydotool,4=xtest */

static int vk_to_linux(int vk_code) {
  int linux_code = x11_vk_to_linux_key(vk_code);
  if (!linux_code) {
    if (vk_code == VK_LCONTROL) linux_code = KEY_LEFTCTRL;
    else if (vk_code == VK_RCONTROL) linux_code = KEY_RIGHTCTRL;
    else if (vk_code == VK_LSHIFT) linux_code = KEY_LEFTSHIFT;
    else if (vk_code == VK_RSHIFT) linux_code = KEY_RIGHTSHIFT;
    else if (vk_code == VK_LMENU) linux_code = KEY_LEFTALT;
    else if (vk_code == VK_RMENU) linux_code = KEY_RIGHTALT;
    else if (vk_code == VK_LWIN) linux_code = KEY_LEFTMETA;
    else if (vk_code == VK_RWIN) linux_code = KEY_RIGHTMETA;
    else if (vk_code == 0xDC) linux_code = KEY_BACKSLASH;
    else if (vk_code == VK_UP) linux_code = KEY_UP;
    else if (vk_code == VK_DOWN) linux_code = KEY_DOWN;
    else if (vk_code == VK_LEFT) linux_code = KEY_LEFT;
    else if (vk_code == VK_RIGHT) linux_code = KEY_RIGHT;
    else if (vk_code == VK_RETURN) linux_code = KEY_ENTER;
    else if (vk_code == VK_SPACE) linux_code = KEY_SPACE;
    else if (vk_code == VK_TAB) linux_code = KEY_TAB;
    else if (vk_code == VK_ESCAPE) linux_code = KEY_ESC;
  }
  return linux_code;
}

static bool inject_uinput(int vk_code, int down) {
  int linux_code = vk_to_linux(vk_code);
  if (!linux_code || g_x11_uinput_fd < 0)
    return false;
  struct input_event ev;
  memset(&ev, 0, sizeof(ev));
  gettimeofday(&ev.time, NULL);
  ev.type = EV_KEY;
  ev.code = linux_code;
  ev.value = down ? 1 : 0;
  x11_uinput_emit(&ev);
  return true;
}

static bool inject_wtype(int vk_code, int down) {
  (void)vk_code;
  (void)down;
  /* wtype backend not yet implemented */
  return false;
}

static bool inject_ydotool(int vk_code, int down) {
  (void)vk_code;
  (void)down;
  /* ydotool backend not yet implemented */
  return false;
}

static bool inject_xtest(int vk_code, int down) {
#if __has_include(<X11/extensions/XTest.h>)
  Display *dpy = get_display();
  if (!dpy)
    return false;
  KeySym ks = 0;
  if (vk_code >= 'A' && vk_code <= 'Z') ks = XK_A + (vk_code - 'A');
  else if (vk_code >= '0' && vk_code <= '9') ks = XK_0 + (vk_code - '0');
  else if (vk_code == VK_UP) ks = XK_Up;
  else if (vk_code == VK_DOWN) ks = XK_Down;
  else if (vk_code == VK_LEFT) ks = XK_Left;
  else if (vk_code == VK_RIGHT) ks = XK_Right;
  else if (vk_code == VK_RETURN) ks = XK_Return;
  else if (vk_code == VK_SPACE) ks = XK_space;
  else if (vk_code == VK_TAB) ks = XK_Tab;
  else if (vk_code == VK_ESCAPE) ks = XK_Escape;
  else if (vk_code == VK_OEM_3) ks = XK_grave;
  else if (vk_code == 0xDC) ks = XK_backslash;
  else if (vk_code == VK_LSHIFT) ks = XK_Shift_L;
  else if (vk_code == VK_RSHIFT) ks = XK_Shift_R;
  else if (vk_code == VK_LCONTROL) ks = XK_Control_L;
  else if (vk_code == VK_RCONTROL) ks = XK_Control_R;
  else if (vk_code == VK_LMENU) ks = XK_Alt_L;
  else if (vk_code == VK_RMENU) ks = XK_Alt_R;
  if (!ks)
    return false;
  KeyCode kc = XKeysymToKeycode(dpy, ks);
  if (!kc)
    return false;
  XTestFakeKeyEvent(dpy, kc, down, CurrentTime);
  XFlush(dpy);
  return true;
#else
  return false;
#endif
}

void Platform_EmitKey(int vk_code, int down) {
  int linux_code = vk_to_linux(vk_code);
  if (!linux_code && vk_code != VK_LCONTROL && vk_code != VK_RCONTROL &&
      vk_code != VK_LSHIFT && vk_code != VK_RSHIFT && vk_code != VK_LMENU &&
      vk_code != VK_RMENU && vk_code != VK_LWIN && vk_code != VK_RWIN) {
    /* keep going; xtest may still handle */
  }
  if (!linux_code) {
    log_warn("Platform_EmitKey no linux_code for vk=%d", vk_code);
  }

  /* ordered backends */
  bool (*backends[4])(int, int) = {inject_uinput, inject_wtype, inject_ydotool, inject_xtest};
  int start = (g_inject_backend > 0 && g_inject_backend <= 4) ? g_inject_backend - 1 : 0;
  for (int i = start; i < 4; i++) {
    if (backends[i](vk_code, down)) {
      g_inject_backend = i + 1;
      return;
    }
  }
  /* reset and try all from beginning if cached failed */
  if (start != 0) {
    for (int i = 0; i < start; i++) {
      if (backends[i](vk_code, down)) {
        g_inject_backend = i + 1;
        return;
      }
    }
  }
  log_warn("Platform_EmitKey all backends failed for vk=%d", vk_code);
}
