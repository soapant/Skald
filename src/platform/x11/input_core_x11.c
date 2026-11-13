#define _GNU_SOURCE
#include "helpers/x11/raw_input_core_x11.h"

bool Platform_GetMousePos(int *out_x, int *out_y) {
  int mx = 0, my = 0;
  if (!Wm_GetMousePos(&mx, &my))
    return false;
  if (out_x)
    *out_x = mx;
  if (out_y)
    *out_y = my;
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
