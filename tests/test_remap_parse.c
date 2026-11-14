#include <stdio.h>
#include <string.h>

#include "test.h"

#include "core/remap_manager.h"
#include "core/types/keys.h"

static int g_count;
static int mods;
static int vks[REMAP_PARSE_MAX_VKS];

static bool parse(const char *str) {
  memset(vks, 0, sizeof(vks));
  mods = -1;
  g_count = -1;
  return remap_parse_triggers(str, &mods, vks, &g_count);
}

TEST(parse_ctrl_a) {
  ASSERT_TRUE(parse("Ctrl+A"));
  ASSERT_EQ_INT(mods, HOTKEY_MOD_CTRL);
  ASSERT_EQ_INT(g_count, 1);
  ASSERT_EQ_INT(vks[0], 'A');
}

TEST(parse_ctrl_shift_k) {
  ASSERT_TRUE(parse("Ctrl+Shift+K"));
  ASSERT_EQ_INT(mods, HOTKEY_MOD_CTRL | HOTKEY_MOD_SHIFT);
  ASSERT_EQ_INT(g_count, 1);
  ASSERT_EQ_INT(vks[0], 'K');
}

TEST(parse_left_right_specific_are_vks_not_mods) {
  ASSERT_TRUE(parse("LCtrl+A"));
  ASSERT_EQ_INT(mods, 0);
  ASSERT_EQ_INT(g_count, 2);
  ASSERT_EQ_INT(vks[0], VK_LCONTROL);
  ASSERT_EQ_INT(vks[1], 'A');
}

TEST(parse_whitespace_trimmed) {
  ASSERT_TRUE(parse("Ctrl + A"));
  ASSERT_EQ_INT(mods, HOTKEY_MOD_CTRL);
  ASSERT_EQ_INT(g_count, 1);
  ASSERT_EQ_INT(vks[0], 'A');
}

TEST(parse_shifted_digit_chars) {
  ASSERT_TRUE(parse("!"));
  ASSERT_EQ_INT(mods, HOTKEY_MOD_SHIFT);
  ASSERT_EQ_INT(g_count, 1);
  ASSERT_EQ_INT(vks[0], '1');

  ASSERT_TRUE(parse("}"));
  ASSERT_EQ_INT(mods, HOTKEY_MOD_SHIFT);
  ASSERT_EQ_INT(vks[0], VK_OEM_6);

  ASSERT_TRUE(parse("\\"));
  ASSERT_EQ_INT(mods, 0);
  ASSERT_EQ_INT(vks[0], 0xDC);
}

TEST(parse_named_keys) {
  ASSERT_TRUE(parse("Escape"));
  ASSERT_EQ_INT(vks[0], VK_ESCAPE);

  ASSERT_TRUE(parse("Grave"));
  ASSERT_EQ_INT(vks[0], VK_OEM_3);

  ASSERT_TRUE(parse("F5"));
  ASSERT_EQ_INT(vks[0], VK_F1 + 4);

  ASSERT_TRUE(parse("F12"));
  ASSERT_EQ_INT(vks[0], VK_F1 + 11);
}

TEST(parse_unknown_token_yields_no_vk) {
  ASSERT_TRUE(parse("ZZZ"));
  ASSERT_EQ_INT(mods, 0);
  ASSERT_EQ_INT(g_count, 0);
}

TEST(parse_empty_string_yields_no_vk) {
  ASSERT_TRUE(parse(""));
  ASSERT_EQ_INT(g_count, 0);
}

TEST(parse_null_returns_false) {
  mods = -1;
  ASSERT_FALSE(remap_parse_triggers(NULL, &mods, vks, &g_count));
}

TEST(parse_super_aliases) {
  ASSERT_TRUE(parse("Win+K"));
  ASSERT_EQ_INT(mods, HOTKEY_MOD_SUPER);
  ASSERT_TRUE(parse("Super+K"));
  ASSERT_EQ_INT(mods, HOTKEY_MOD_SUPER);
  ASSERT_TRUE(parse("Meta+K"));
  ASSERT_EQ_INT(mods, HOTKEY_MOD_SUPER);
}

int main(void) {
  RUN(parse_ctrl_a);
  RUN(parse_ctrl_shift_k);
  RUN(parse_left_right_specific_are_vks_not_mods);
  RUN(parse_whitespace_trimmed);
  RUN(parse_shifted_digit_chars);
  RUN(parse_named_keys);
  RUN(parse_unknown_token_yields_no_vk);
  RUN(parse_empty_string_yields_no_vk);
  RUN(parse_null_returns_false);
  RUN(parse_super_aliases);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
