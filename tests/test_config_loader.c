#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "test.h"
#include "test_tmp.h"

#include "config.h"

static char PATH[512];

static void loader_cleanup(void) {
  config_unload();
  file_remove_path(PATH);
}

static void seed(const char *contents) {
  FILE *f = fopen(PATH, "wb");
  if (!f)
    return;
  fputs(contents, f);
  fclose(f);
}

static const char *valid_config(void) {
  return "# <skald-config-generated>\n"
         "FontSize = 33\n"
         "WindowOpacity = 1.25e-1\n"
         "UsedFont = \"Test Font\"\n"
         "ControlHotkey = \"Alt+T\"\n"
         "\n[color]\n"
         "BG = \"#112233\"\n"
         "TEXT = \"#FFFFFF\"\n"
         "# </skald-config-generated>\n";
}

static int font_size(void) {
  return atomic_load_explicit(&FontSize, memory_order_acquire);
}

static float opacity(void) {
  return atomic_load_explicit(&WindowOpacity, memory_order_acquire);
}

static const char *used_font(void) {
  return atomic_load_explicit(&UsedFont, memory_order_acquire);
}

TEST(load_applies_core_and_color_values) {
  seed(valid_config());
  ASSERT_TRUE(config_load_path(PATH));
  ASSERT_EQ_INT(font_size(), 33);
  ASSERT_TRUE(fabsf(opacity() - 0.125f) < 0.0001f);
  ASSERT_EQ_STR(used_font(), "Test Font");
  ASSERT_EQ_STR(atomic_load_explicit(&ControlHotkey, memory_order_acquire),
                "Alt+T");
  ASSERT_EQ_INT((int)atomic_load_explicit(&COLOR_BG, memory_order_acquire),
                (int)RGB(0x11, 0x22, 0x33));
}

TEST(save_reload_roundtrip_keeps_exponent_floats) {
  seed(valid_config());
  ASSERT_TRUE(config_load_path(PATH));
  ASSERT_TRUE(config_save_path(PATH));
  ASSERT_TRUE(config_load_path(PATH));
  ASSERT_TRUE(fabsf(opacity() - 0.125f) < 0.0001f);
  ASSERT_EQ_INT(font_size(), 33);
  ASSERT_EQ_STR(used_font(), "Test Font");
}

TEST(bad_value_keeps_previous_config) {
  seed(valid_config());
  ASSERT_TRUE(config_load_path(PATH));
  seed("# <skald-config-generated>\nFontSize = nope\n"
       "# </skald-config-generated>\n");
  ASSERT_FALSE(config_load_path(PATH));
  ASSERT_EQ_INT(font_size(), 33);
}

TEST(unknown_keys_are_ignored) {
  seed("# <skald-config-generated>\nBogusKey = 7\nFontSize = 41\n"
       "# </skald-config-generated>\n");
  ASSERT_TRUE(config_load_path(PATH));
  ASSERT_EQ_INT(font_size(), 41);
}

TEST(save_without_markers_fails) {
  FILE *f = fopen(PATH, "wb");
  ASSERT_TRUE(f != NULL);
  if (f) {
    fputs("FontSize = 12\n", f);
    fclose(f);
  }
  ASSERT_FALSE(config_save_path(PATH));
}

TEST(unload_restores_defaults) {
  seed(valid_config());
  ASSERT_TRUE(config_load_path(PATH));
  ASSERT_EQ_INT(font_size(), 33);
  config_unload();
  ASSERT_EQ_INT(font_size(), 20);
  ASSERT_EQ_STR(used_font(), "0xProto Nerd Font");
  ASSERT_TRUE(fabsf(opacity() - 235.0f) < 0.0001f);
}

int main(void) {
  test_tmp_path(PATH, sizeof(PATH), "test_config_loader.toml");
  TEST_SUITE_TEARDOWN(loader_cleanup);
  RUN(load_applies_core_and_color_values);
  RUN(save_reload_roundtrip_keeps_exponent_floats);
  RUN(bad_value_keeps_previous_config);
  RUN(unknown_keys_are_ignored);
  RUN(save_without_markers_fails);
  RUN(unload_restores_defaults);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
