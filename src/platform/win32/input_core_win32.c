#include <windows.h>
#include <stdbool.h>
#include <string.h>
#include "core/types/keys.h"
#include "core/types/window.h"
#include "config_values.h"
#include "deps/log.h"
#include "core/context.h"
#include "core/hotkey_manager.h"
#include "helpers/helpers_hotkey.h"
#include "platform/platform.h"
#include "core/window_manager.h"
#include "core/snippet_manager.h"
#include "core/remap_manager.h"
#include "helpers/helpers_input_state.h"
#include "helpers/win32/input_core_win32.h"
#include "core/hotkey_pending.h"


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
  char mb[16];
  int m = WideCharToMultiByte(CP_UTF8, 0, buf, 1, mb, sizeof(mb), NULL, NULL);
  if (m <= 0)
    return 0;
  return (int)(unsigned char)mb[0];
}
