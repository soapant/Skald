#include "platform/platform.h"
#include "helpers/x11/input_x11.h"
#include "helpers/x11/display_x11.h"
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
#include <stdio.h>
#if __has_include(<X11/extensions/XTest.h>)
#include <X11/extensions/XTest.h>
#endif

static int vk_to_linux(int vk_code) {
  int linux_code = x11_vk_to_linux_key(vk_code);
  if (!linux_code) {
    if (vk_code == VK_LCONTROL)
      linux_code = KEY_LEFTCTRL;
    else if (vk_code == VK_RCONTROL)
      linux_code = KEY_RIGHTCTRL;
    else if (vk_code == VK_LSHIFT)
      linux_code = KEY_LEFTSHIFT;
    else if (vk_code == VK_RSHIFT)
      linux_code = KEY_RIGHTSHIFT;
    else if (vk_code == VK_LMENU)
      linux_code = KEY_LEFTALT;
    else if (vk_code == VK_RMENU)
      linux_code = KEY_RIGHTALT;
    else if (vk_code == VK_LWIN)
      linux_code = KEY_LEFTMETA;
    else if (vk_code == VK_RWIN)
      linux_code = KEY_RIGHTMETA;
    else if (vk_code == 0xDC)
      linux_code = KEY_BACKSLASH;
    else if (vk_code == VK_UP)
      linux_code = KEY_UP;
    else if (vk_code == VK_DOWN)
      linux_code = KEY_DOWN;
    else if (vk_code == VK_LEFT)
      linux_code = KEY_LEFT;
    else if (vk_code == VK_RIGHT)
      linux_code = KEY_RIGHT;
    else if (vk_code == VK_RETURN)
      linux_code = KEY_ENTER;
    else if (vk_code == VK_SPACE)
      linux_code = KEY_SPACE;
    else if (vk_code == VK_TAB)
      linux_code = KEY_TAB;
    else if (vk_code == VK_ESCAPE)
      linux_code = KEY_ESC;
  }
  return linux_code;
}

static bool inject_uinput(int vk_code, int down) {
  int linux_code = vk_to_linux(vk_code);
  if (!linux_code || x11_uinput_fd_get() < 0)
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

static const char *vk_to_name(int vk_code, char *buf, size_t sz) {
  if (vk_code >= 'A' && vk_code <= 'Z') {
    snprintf(buf, sz, "%c", (char)(vk_code + 32));
    return buf;
  }
  if (vk_code >= '0' && vk_code <= '9') {
    snprintf(buf, sz, "%c", (char)vk_code);
    return buf;
  }
  switch (vk_code) {
  case VK_UP:
    return "Up";
  case VK_DOWN:
    return "Down";
  case VK_LEFT:
    return "Left";
  case VK_RIGHT:
    return "Right";
  case VK_RETURN:
    return "Return";
  case VK_SPACE:
    return "space";
  case VK_TAB:
    return "Tab";
  case VK_ESCAPE:
    return "Escape";
  case VK_LSHIFT:
    return "Shift_L";
  case VK_RSHIFT:
    return "Shift_R";
  case VK_LCONTROL:
    return "Control_L";
  case VK_RCONTROL:
    return "Control_R";
  case VK_LMENU:
    return "Alt_L";
  case VK_RMENU:
    return "Alt_R";
  case VK_LWIN:
    return "Super_L";
  case VK_RWIN:
    return "Super_R";
  case 0xDC:
    return "backslash";
  case VK_OEM_3:
    return "grave";
  default:
    return NULL;
  }
}

static bool inject_run(const char *cmd) {
  char *out = Platform_RunTerminalCommand((char *)cmd, false);
  if (out) {
    free(out);
    return true;
  }
  return false;
}

static bool inject_ydotool(int vk_code, int down) {
  int linux_code = vk_to_linux(vk_code);
  if (!linux_code)
    return false;
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "ydotool key %d:%d", linux_code, down ? 1 : 0);
  return inject_run(cmd);
}

static bool inject_keytool(const char *tool, const char *down_flag,
                           const char *up_flag, int vk_code, int down) {
  char buf[32];
  const char *name = vk_to_name(vk_code, buf, sizeof(buf));
  if (!name)
    return false;
  char cmd[256];
  snprintf(cmd, sizeof(cmd), "%s %s %s", tool, down ? down_flag : up_flag,
           name);
  return inject_run(cmd);
}

static bool inject_wtype(int vk_code, int down) {
  return inject_keytool("wtype", "-P", "-R", vk_code, down);
}

static bool inject_xdotool(int vk_code, int down) {
  return inject_keytool("xdotool", "keydown", "keyup", vk_code, down);
}

static bool inject_xtest(int vk_code, int down) {
#if __has_include(<X11/extensions/XTest.h>)
  Display *dpy = x11_get_display();
  if (!dpy)
    return false;
  KeySym ks = 0;
  if (vk_code >= 'A' && vk_code <= 'Z')
    ks = XK_A + (vk_code - 'A');
  else if (vk_code >= '0' && vk_code <= '9')
    ks = XK_0 + (vk_code - '0');
  else if (vk_code == VK_UP)
    ks = XK_Up;
  else if (vk_code == VK_DOWN)
    ks = XK_Down;
  else if (vk_code == VK_LEFT)
    ks = XK_Left;
  else if (vk_code == VK_RIGHT)
    ks = XK_Right;
  else if (vk_code == VK_RETURN)
    ks = XK_Return;
  else if (vk_code == VK_SPACE)
    ks = XK_space;
  else if (vk_code == VK_TAB)
    ks = XK_Tab;
  else if (vk_code == VK_ESCAPE)
    ks = XK_Escape;
  else if (vk_code == VK_OEM_3)
    ks = XK_grave;
  else if (vk_code == 0xDC)
    ks = XK_backslash;
  else if (vk_code == VK_LSHIFT)
    ks = XK_Shift_L;
  else if (vk_code == VK_RSHIFT)
    ks = XK_Shift_R;
  else if (vk_code == VK_LCONTROL)
    ks = XK_Control_L;
  else if (vk_code == VK_RCONTROL)
    ks = XK_Control_R;
  else if (vk_code == VK_LMENU)
    ks = XK_Alt_L;
  else if (vk_code == VK_RMENU)
    ks = XK_Alt_R;
  if (!ks)
    return false;
  KeyCode kc = XKeysymToKeycode(dpy, ks);
  if (!kc)
    return false;
  XTestFakeKeyEvent(dpy, kc, down, CurrentTime);
  XFlush(dpy);
  return true;
#else
  log_warn("inject_xtest: XTest Isnt Available");
  (void)vk_code;
  (void)down;
  return false;
#endif
}

void Platform_EmitKey(int vk_code, int down) {
  if (Platform_InjectKey(vk_code, down))
    return;
  if (inject_uinput(vk_code, down))
    return;
  if (inject_ydotool(vk_code, down))
    return;
  if (inject_wtype(vk_code, down))
    return;
  if (inject_xdotool(vk_code, down))
    return;
  inject_xtest(vk_code, down);
}
