#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "test.h"

#include "core/theme.h"

#include "config_values.h"

TEST(find_known_theme_case_insensitive) {
  const ThemeEntry *a = Theme_Find("cyberpunk");
  const ThemeEntry *b = Theme_Find("CYBERPUNK");
  ASSERT_TRUE(a != NULL);
  ASSERT_TRUE(a == b);
}

TEST(find_unknown_and_empty) {
  ASSERT_TRUE(Theme_Find("definitely-not-a-theme") == NULL);
  ASSERT_TRUE(Theme_Find("") == NULL);
  ASSERT_TRUE(Theme_Find(NULL) == NULL);
}

TEST(theme_names_are_unique) {
  size_t n = Theme_Count();
  ASSERT_TRUE(n >= 40);
  for (size_t i = 0; i < n; i++) {
    const ThemeEntry *e = Theme_EntryAt(i);
    ASSERT_TRUE(e != NULL);
    ASSERT_TRUE(e->name && e->name[0]);
    for (size_t j = i + 1; j < n; j++) {
      ASSERT_TRUE(strcmp(Theme_EntryAt(i)->name, Theme_EntryAt(j)->name) != 0);
    }
  }
}

TEST(entry_at_out_of_bounds) {
  ASSERT_TRUE(Theme_EntryAt(Theme_Count()) == NULL);
  ASSERT_TRUE(Theme_EntryAt(SIZE_MAX) == NULL);
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

  ASSERT_TRUE(Theme_PinScanFile(PIN_FILE));
  ASSERT_TRUE(Theme_ColorPinned("COLOR_BG"));
  ASSERT_TRUE(Theme_ColorPinned("COLOR_BORDER"));
  ASSERT_FALSE(Theme_ColorPinned("COLOR_TEXT"));
  ASSERT_FALSE(Theme_ColorPinned("COLOR_BG_NO_LEADING_SPACE"));

  Theme_ResetPinned();
  Theme_ResetPinned();
  ASSERT_TRUE(Theme_PinScanFile(PIN_FILE));
  ASSERT_TRUE(Theme_ColorPinned("COLOR_BG"));
  ASSERT_TRUE(Theme_ColorPinned("COLOR_BORDER"));
  Theme_ResetPinned();
}

TEST(pin_scan_missing_file_keeps_pins_empty) {
  Theme_ResetPinned();
  ASSERT_FALSE(Theme_PinScanFile("build/tests/definitely_not_here.cnf"));
  ASSERT_FALSE(Theme_ColorPinned("COLOR_BG"));
}

TEST(apply_flow_unknown_theme_keeps_colors) {
  ColorRef before = COLOR_BG;
  const char *saved_theme = Theme;
  Theme = "definitely-not-a-theme";
  Theme_Apply();
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
