#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/hotkey_capture.h"
#include "core/hotkey_manager.h"
#include "core/types/keys.h"
#include "helpers/helpers_input_state.h"
#include "helpers/helpers_key.h"

typedef struct {
  bool active;
  bool captured;
  bool waiting_release;
  int last_vk;
  char preview[64];
  char last[64];
} HotkeyCapture;

static HotkeyCapture s_capture = {0};

static bool capture_is_modifier_vk(int vk) {
  switch (vk) {
  case VK_SHIFT:
  case VK_CONTROL:
  case VK_MENU:
  case VK_LSHIFT:
  case VK_RSHIFT:
  case VK_LCONTROL:
  case VK_RCONTROL:
  case VK_LMENU:
  case VK_RMENU:
  case VK_LWIN:
  case VK_RWIN:
    return true;
  default:
    return false;
  }
}

static int capture_current_mods(void) {
  int mods = 0;
  if (key_is_ctrl_pressed())
    mods |= HOTKEY_MOD_CTRL;
  if (key_is_shift_pressed())
    mods |= HOTKEY_MOD_SHIFT;
  if (key_is_alt_pressed())
    mods |= HOTKEY_MOD_ALT;
  if (key_is_super_pressed())
    mods |= HOTKEY_MOD_SUPER;
  return mods;
}

static void capture_append_mod(char *buf, size_t cap, const char *mod) {
  size_t len = strlen(buf);
  size_t add = strlen(mod);
  if (len + add + 1 > cap)
    return;
  memcpy(buf + len, mod, add);
  buf[len + add] = '\0';
}

void hotkey_capture_begin(void) {
  int last_vk = s_capture.last_vk;
  bool waiting = s_capture.waiting_release;
  memset(&s_capture, 0, sizeof(s_capture));
  if (last_vk && !input_is_key_down(last_vk))
    waiting = false;
  s_capture.last_vk = last_vk;
  s_capture.waiting_release = waiting;
  s_capture.active = true;
  hotkey_set_suspended(true);
}

void hotkey_capture_end(void) {
  memset(&s_capture, 0, sizeof(s_capture));
  hotkey_set_suspended(false);
}

bool hotkey_capture_active(void) { return s_capture.active; }

const char *hotkey_capture_preview(void) { return s_capture.preview; }

const char *hotkey_capture_last(void) { return s_capture.last; }

bool hotkey_capture_poll(char *out, size_t cap) {
  if (out && cap)
    out[0] = '\0';
  if (!s_capture.active || s_capture.captured)
    return false;

  if (s_capture.waiting_release &&
      (!input_is_key_down(s_capture.last_vk) ||
       input_was_key_released(s_capture.last_vk)))
    s_capture.waiting_release = false;

  int mods = capture_current_mods();
  int key = 0;
  for (int vk = 32; vk < 256; vk++) {
    if (capture_is_modifier_vk(vk))
      continue;
    if (!input_was_key_pressed(vk))
      continue;
    if (vk == s_capture.last_vk && s_capture.waiting_release)
      continue;
    key = vk;
    break;
  }

  if (!key) {
    char buf[64] = "";
    if (mods & HOTKEY_MOD_CTRL)
      capture_append_mod(buf, sizeof(buf), "Ctrl+");
    if (mods & HOTKEY_MOD_SHIFT)
      capture_append_mod(buf, sizeof(buf), "Shift+");
    if (mods & HOTKEY_MOD_ALT)
      capture_append_mod(buf, sizeof(buf), "Alt+");
    if (mods & HOTKEY_MOD_SUPER)
      capture_append_mod(buf, sizeof(buf), "Super+");
    size_t len = strlen(buf);
    if (len && buf[len - 1] == '+')
      buf[len - 1] = '\0';
    snprintf(s_capture.preview, sizeof(s_capture.preview), "%s", buf);
    return false;
  }

  HotkeyKey captured = {.vk_code = key, .mods = mods};
  char formatted[64] = "";
  if (!hotkey_string_format(captured, formatted, (int)sizeof(formatted)))
    return false;

  if (!mods) {
    snprintf(s_capture.preview, sizeof(s_capture.preview), "%s", formatted);
    return false;
  }

  snprintf(s_capture.last, sizeof(s_capture.last), "%s", formatted);
  snprintf(s_capture.preview, sizeof(s_capture.preview), "%s", formatted);
  s_capture.last_vk = key;
  s_capture.waiting_release = true;
  s_capture.captured = true;
  if (out && cap)
    snprintf(out, cap, "%s", formatted);
  return true;
}
