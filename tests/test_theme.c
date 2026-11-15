#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "test.h"

#include "core/theme.h"

#include "config_values.h"

TEST(find_known_theme_case_insensitive) {
  const ThemeEntry *a = theme_find("cyberpunk");
  const ThemeEntry *b = theme_find("CYBERPUNK");
  ASSERT_TRUE(a != NULL);
  ASSERT_TRUE(a == b);
}

TEST(find_unknown_and_empty) {
  ASSERT_TRUE(theme_find("definitely-not-a-theme") == NULL);
  ASSERT_TRUE(theme_find("") == NULL);
  ASSERT_TRUE(theme_find(NULL) == NULL);
}

TEST(theme_names_are_unique) {
  size_t n = theme_count();
  ASSERT_TRUE(n >= 40);
  for (size_t i = 0; i < n; i++) {
    const ThemeEntry *e = theme_entry_at(i);
    ASSERT_TRUE(e != NULL);
    ASSERT_TRUE(e->name && e->name[0]);
    for (size_t j = i + 1; j < n; j++) {
      ASSERT_TRUE(strcmp(theme_entry_at(i)->name, theme_entry_at(j)->name) !=
                  0);
    }
  }
}

TEST(entry_at_out_of_bounds) {
  ASSERT_TRUE(theme_entry_at(theme_count()) == NULL);
  ASSERT_TRUE(theme_entry_at(SIZE_MAX) == NULL);
}

static const char *PIN_FILE = "build/tests/pin_test.cnf";

TEST(pin_scan_detects_color_overrides) {
  FILE *f = fopen(PIN_FILE, "wb");
  ASSERT_TRUE(f != NULL);
  if (!f)
    return;
  fputs("Theme = \"rose-pine\"\n  COLOR_BG = RGB(1, 2, 3)\n"
        "  COLOR_BORDER = RGB(4, 5, 6)\n"
        "  NOT_COLOR = RGB(0, 0, 0)\n"
        "COLOR_BG_NO_LEADING_SPACE = RGB(7, 8, 9)\n",
        f);
  fclose(f);

  ASSERT_TRUE(theme_pin_scan_file(PIN_FILE));
  ASSERT_TRUE(theme_color_pinned("COLOR_BG"));
  ASSERT_TRUE(theme_color_pinned("COLOR_BORDER"));
  ASSERT_FALSE(theme_color_pinned("COLOR_TEXT"));
  ASSERT_FALSE(theme_color_pinned("COLOR_BG_NO_LEADING_SPACE"));

  theme_reset_pinned();
  theme_reset_pinned();
  ASSERT_TRUE(theme_pin_scan_file(PIN_FILE));
  ASSERT_TRUE(theme_color_pinned("COLOR_BG"));
  ASSERT_TRUE(theme_color_pinned("COLOR_BORDER"));
  theme_reset_pinned();
}

TEST(pin_scan_missing_file_keeps_pins_empty) {
  theme_reset_pinned();
  ASSERT_FALSE(theme_pin_scan_file("build/tests/definitely_not_here.cnf"));
  ASSERT_FALSE(theme_color_pinned("COLOR_BG"));
}

TEST(apply_flow_unknown_theme_keeps_colors) {
  ColorRef before = COLOR_BG;
  const char *saved_theme = Theme;
  Theme = "definitely-not-a-theme";
  theme_apply();
  ASSERT_EQ_INT((long)before, (long)COLOR_BG);
  Theme = saved_theme;
}

int main(void) {
  RUN(find_known_theme_case_insensitive);
  RUN(find_unknown_and_empty);
  RUN(theme_names_are_unique);
  RUN(entry_at_out_of_bounds);
  RUN(pin_scan_detects_color_overrides);
  RUN(pin_scan_missing_file_keeps_pins_empty);
  RUN(apply_flow_unknown_theme_keeps_colors);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
