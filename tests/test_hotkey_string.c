#include <stdio.h>
#include <string.h>

#include "test.h"

#include "core/hotkey_manager.h"
#include "core/types/keys.h"

TEST(parse_simple_key) {
  HotkeyKey k;
  ASSERT_TRUE(hotkey_string_parse("A", &k));
  ASSERT_EQ_INT(k.vk_code, 'A');
  ASSERT_EQ_INT(k.mods, 0);

  ASSERT_TRUE(hotkey_string_parse("1", &k));
  ASSERT_EQ_INT(k.vk_code, '1');

  ASSERT_TRUE(hotkey_string_parse("Escape", &k));
  ASSERT_EQ_INT(k.vk_code, VK_ESCAPE);
  ASSERT_EQ_INT(k.mods, 0);
}

TEST(parse_modifiers) {
  HotkeyKey k;
  ASSERT_TRUE(hotkey_string_parse("Ctrl+Shift+A", &k));
  ASSERT_EQ_INT(k.vk_code, 'A');
  ASSERT_EQ_INT(k.mods, HOTKEY_MOD_CTRL | HOTKEY_MOD_SHIFT);
}

TEST(parse_modifiers_case_insensitive) {
  HotkeyKey k;
  ASSERT_TRUE(hotkey_string_parse("ctrl+A", &k));
  ASSERT_EQ_INT(k.mods, HOTKEY_MOD_CTRL);

  ASSERT_TRUE(hotkey_string_parse("shift+f5", &k));
  ASSERT_EQ_INT(k.vk_code, VK_F1 + 4);
  ASSERT_EQ_INT(k.mods, HOTKEY_MOD_SHIFT);

  ASSERT_TRUE(hotkey_string_parse("control+a", &k));
  ASSERT_EQ_INT(k.mods, HOTKEY_MOD_CTRL);
}

TEST(parse_alt_and_super) {
  HotkeyKey k;
  ASSERT_TRUE(hotkey_string_parse("Alt+Super+V", &k));
  ASSERT_EQ_INT(k.vk_code, 'V');
  ASSERT_EQ_INT(k.mods, HOTKEY_MOD_ALT | HOTKEY_MOD_SUPER);
}

TEST(parse_modifiers_only_fail) {
  HotkeyKey k;
  ASSERT_FALSE(hotkey_string_parse("Ctrl", &k));
  ASSERT_EQ_INT(k.mods, HOTKEY_MOD_CTRL);

  ASSERT_FALSE(hotkey_string_parse("CtrlW", &k));

  ASSERT_FALSE(hotkey_string_parse("Shift+Alt", &k));
}

TEST(parse_empty_string_fails) {
  HotkeyKey k;
  ASSERT_FALSE(hotkey_string_parse("", &k));
}

TEST(parse_control_plus_plus) {
  HotkeyKey k;
  ASSERT_TRUE(hotkey_string_parse("Ctrl++", &k));
  ASSERT_EQ_INT(k.vk_code, VK_OEM_PLUS);
  ASSERT_EQ_INT(k.mods, HOTKEY_MOD_CTRL);
}

TEST(parse_lshift_is_key_not_modifier) {
  HotkeyKey k;
  ASSERT_TRUE(hotkey_string_parse("LShift", &k));
  ASSERT_EQ_INT(k.vk_code, VK_LSHIFT);
  ASSERT_EQ_INT(k.mods, 0);
}

TEST(parse_mouse_button) {
  HotkeyKey k;
  ASSERT_TRUE(hotkey_string_parse("Mouse3", &k));
  ASSERT_EQ_INT(k.vk_code, MOUSE_VK(3));
}

TEST(parse_function_keys) {
  HotkeyKey k;
  ASSERT_TRUE(hotkey_string_parse("F5", &k));
  ASSERT_EQ_INT(k.vk_code, VK_F1 + 4);

  ASSERT_TRUE(hotkey_string_parse("F12", &k));
  ASSERT_EQ_INT(k.vk_code, VK_F12);
}

TEST(format_ctrl_shift_a) {
  HotkeyKey k;
  memset(&k, 0, sizeof(k));
  k.vk_code = 'A';
  k.mods = HOTKEY_MOD_CTRL | HOTKEY_MOD_SHIFT;
  char out[64];
  ASSERT_TRUE(hotkey_string_format(k, out, sizeof(out)));
  ASSERT_EQ_STR(out, "Ctrl+Shift+A");
}

TEST(format_escape) {
  HotkeyKey k;
  memset(&k, 0, sizeof(k));
  k.vk_code = VK_ESCAPE;
  char out[64];
  ASSERT_TRUE(hotkey_string_format(k, out, sizeof(out)));
  ASSERT_EQ_STR(out, "Escape");
}

TEST(format_ctrl_equals) {
  HotkeyKey k;
  memset(&k, 0, sizeof(k));
  k.vk_code = VK_OEM_PLUS;
  k.mods = HOTKEY_MOD_CTRL;
  char out[64];
  ASSERT_TRUE(hotkey_string_format(k, out, sizeof(out)));
  ASSERT_EQ_STR(out, "Ctrl+=");
}

TEST(format_zero_key_fails) {
  HotkeyKey k;
  memset(&k, 0, sizeof(k));
  char out[64] = "sentinel";
  ASSERT_FALSE(hotkey_string_format(k, out, sizeof(out)));
  ASSERT_EQ_INT(out[0], '\0');
}

TEST(format_roundtrip) {
  HotkeyKey parsed;
  ASSERT_TRUE(hotkey_string_parse("Ctrl+Shift+A", &parsed));
  char out[64];
  ASSERT_TRUE(hotkey_string_format(parsed, out, sizeof(out)));
  ASSERT_EQ_STR(out, "Ctrl+Shift+A");
}

TEST(format_mod_order) {
  HotkeyKey k;
  memset(&k, 0, sizeof(k));
  k.vk_code = 'V';
  k.mods = HOTKEY_MOD_SUPER | HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL;
  char out[64];
  ASSERT_TRUE(hotkey_string_format(k, out, sizeof(out)));
  ASSERT_EQ_STR(out, "Ctrl+Alt+Super+V");
}

int main(void) {
  RUN(parse_simple_key);
  RUN(parse_modifiers);
  RUN(parse_modifiers_case_insensitive);
  RUN(parse_alt_and_super);
  RUN(parse_modifiers_only_fail);
  RUN(parse_empty_string_fails);
  RUN(parse_control_plus_plus);
  RUN(parse_lshift_is_key_not_modifier);
  RUN(parse_mouse_button);
  RUN(parse_function_keys);
  RUN(format_ctrl_shift_a);
  RUN(format_escape);
  RUN(format_ctrl_equals);
  RUN(format_zero_key_fails);
  RUN(format_roundtrip);
  RUN(format_mod_order);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
