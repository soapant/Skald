#include "helpers/win32/raw_input_core_win32.h"

bool Platform_GetMousePos(int *out_x, int *out_y) {
  POINT pt;
  if (!GetCursorPos(&pt))
    return false;
  if (out_x)
    *out_x = (int)pt.x;
  if (out_y)
    *out_y = (int)pt.y;
  return true;
}

int Platform_CharForKey(int vk, int scancode, int mods) {
  BYTE state[256];
  memset(state, 0, sizeof(state));
  if (mods & HOTKEY_MOD_SHIFT)
    state[VK_SHIFT] = 0x80;
  if (mods & HOTKEY_MOD_CTRL)
    state[VK_CONTROL] = 0x80;
  if (mods & HOTKEY_MOD_ALT)
    state[VK_MENU] = 0x80;
  HKL layout = GetKeyboardLayout(0);
  wchar_t buf[8];
  int n = ToUnicodeEx((UINT)vk, (UINT)scancode, state, buf, 8, 0, layout);
  if (n <= 0)
    return 0;
  if (n == 1)
    return (int)buf[0];
  if (n == 2 && buf[0] >= 0xD800 && buf[0] <= 0xDBFF && buf[1] >= 0xDC00 &&
      buf[1] <= 0xDFFF)
    return (int)(0x10000 + (((buf[0] - 0xD800) << 10) | (buf[1] - 0xDC00)));
  return (int)buf[0];
}
