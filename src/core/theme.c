#include "core/theme.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "deps/log.h"
#include "config_values.h"
#include "core/types/color.h"

#ifndef CONFIG_SRC
#define CONFIG_SRC "config/config.c"
#endif

typedef struct {
  const char *name;
  ColorRef bg;
  ColorRef text;
  ColorRef alternate;
  ColorRef selected_text;
  ColorRef cursor;
  ColorRef border;
} ThemeEntry;

static const ThemeEntry g_themes[] = {
    {"rose-pine", RGB(25, 23, 36), RGB(255, 255, 255), RGB(57, 255, 20), RGB(19, 21, 23), RGB(82, 79, 103), RGB(88, 86, 114)},
    {"tokyo-night", RGB(26, 27, 38), RGB(192, 202, 245), RGB(158, 206, 106), RGB(16, 17, 28), RGB(192, 202, 245), RGB(65, 72, 104)},
    {"gruvbox", RGB(40, 40, 40), RGB(235, 219, 178), RGB(184, 187, 38), RGB(30, 30, 30), RGB(235, 219, 178), RGB(102, 92, 84)},
    {"nord", RGB(46, 52, 64), RGB(236, 239, 244), RGB(163, 190, 140), RGB(36, 42, 54), RGB(236, 239, 244), RGB(76, 86, 106)},
    {"dracula", RGB(40, 42, 54), RGB(248, 248, 242), RGB(80, 250, 123), RGB(30, 32, 44), RGB(248, 248, 242), RGB(98, 114, 164)},
    {"catppuccin-mocha", RGB(30, 30, 46), RGB(205, 214, 244), RGB(166, 227, 161), RGB(24, 24, 37), RGB(205, 214, 244), RGB(88, 91, 112)},
    {"catppuccin-macchiato", RGB(36, 39, 58), RGB(202, 211, 245), RGB(166, 218, 149), RGB(30, 32, 48), RGB(202, 211, 245), RGB(91, 96, 120)},
    {"catppuccin-frappe", RGB(48, 52, 70), RGB(198, 208, 245), RGB(166, 209, 137), RGB(41, 44, 60), RGB(198, 208, 245), RGB(114, 116, 141)},
    {"catppuccin-latte", RGB(239, 241, 245), RGB(76, 79, 105), RGB(64, 160, 43), RGB(230, 233, 239), RGB(76, 79, 105), RGB(172, 176, 190)},
    {"onedark", RGB(40, 44, 52), RGB(171, 178, 191), RGB(152, 195, 121), RGB(33, 37, 43), RGB(171, 178, 191), RGB(76, 82, 99)},
    {"onedark-pro", RGB(40, 44, 52), RGB(171, 178, 191), RGB(152, 195, 121), RGB(33, 37, 43), RGB(255, 255, 255), RGB(76, 82, 99)},
    {"monokai", RGB(39, 40, 34), RGB(248, 248, 242), RGB(166, 226, 46), RGB(30, 31, 28), RGB(248, 248, 242), RGB(62, 61, 50)},
    {"solarized-dark", RGB(0, 43, 54), RGB(147, 161, 161), RGB(133, 153, 0), RGB(7, 54, 66), RGB(147, 161, 161), RGB(88, 110, 117)},
    {"solarized-light", RGB(253, 246, 227), RGB(101, 123, 131), RGB(133, 153, 0), RGB(238, 232, 213), RGB(101, 123, 131), RGB(147, 161, 161)},
    {"everforest-dark", RGB(45, 53, 59), RGB(211, 198, 170), RGB(167, 192, 128), RGB(39, 46, 51), RGB(211, 198, 170), RGB(76, 86, 93)},
    {"kanagawa", RGB(31, 31, 40), RGB(220, 215, 186), RGB(135, 169, 135), RGB(22, 22, 29), RGB(220, 215, 186), RGB(63, 63, 74)},
    {"rose-pine-moon", RGB(35, 33, 54), RGB(224, 222, 244), RGB(62, 143, 176), RGB(26, 24, 38), RGB(224, 222, 244), RGB(86, 82, 110)},
    {"rose-pine-dawn", RGB(250, 244, 237), RGB(87, 82, 121), RGB(40, 105, 131), RGB(242, 233, 225), RGB(87, 82, 121), RGB(152, 147, 165)},
    {"nightfox", RGB(19, 22, 30), RGB(205, 207, 213), RGB(167, 195, 129), RGB(14, 16, 22), RGB(205, 207, 213), RGB(51, 54, 63)},
    {"cyberpunk", RGB(13, 13, 24), RGB(224, 224, 255), RGB(0, 255, 200), RGB(8, 8, 16), RGB(255, 255, 255), RGB(42, 42, 62)},
    {"matrix", RGB(0, 0, 0), RGB(0, 255, 0), RGB(0, 180, 0), RGB(0, 0, 0), RGB(0, 255, 0), RGB(0, 120, 0)},
    {"amoled", RGB(0, 0, 0), RGB(255, 255, 255), RGB(0, 200, 255), RGB(10, 10, 10), RGB(255, 255, 255), RGB(60, 60, 60)},
    {"github-dark", RGB(13, 17, 23), RGB(201, 209, 217), RGB(63, 185, 80), RGB(22, 27, 34), RGB(201, 209, 217), RGB(48, 54, 61)},
    {"github-light", RGB(255, 255, 255), RGB(31, 35, 40), RGB(26, 127, 55), RGB(246, 248, 250), RGB(31, 35, 40), RGB(208, 215, 222)},
    {"ayu-dark", RGB(10, 14, 20), RGB(191, 189, 182), RGB(170, 217, 76), RGB(14, 18, 25), RGB(191, 189, 182), RGB(56, 61, 67)},
    {"ayu-light", RGB(250, 250, 250), RGB(92, 97, 102), RGB(134, 179, 0), RGB(242, 242, 242), RGB(92, 97, 102), RGB(184, 187, 191)},
    {"gruvbox-light", RGB(251, 241, 224), RGB(60, 56, 54), RGB(121, 116, 14), RGB(235, 219, 178), RGB(60, 56, 54), RGB(151, 138, 126)},
    {"tokyo-night-storm", RGB(36, 40, 59), RGB(192, 202, 245), RGB(158, 206, 106), RGB(41, 46, 66), RGB(192, 202, 245), RGB(86, 95, 137)},
    {"tokyo-night-day", RGB(213, 214, 219), RGB(52, 59, 88), RGB(143, 153, 0), RGB(224, 225, 228), RGB(52, 59, 88), RGB(140, 143, 147)},
    {"everforest-light", RGB(239, 243, 239), RGB(64, 68, 61), RGB(134, 149, 90), RGB(224, 230, 221), RGB(64, 68, 61), RGB(156, 160, 154)},
    {"material-ocean", RGB(15, 17, 26), RGB(214, 218, 227), RGB(152, 195, 121), RGB(21, 24, 34), RGB(214, 218, 227), RGB(60, 64, 78)},
    {"material-palenight", RGB(46, 50, 73), RGB(191, 194, 217), RGB(152, 195, 121), RGB(39, 42, 61), RGB(191, 194, 217), RGB(92, 99, 112)},
    {"moonfly", RGB(21, 25, 34), RGB(209, 215, 232), RGB(178, 229, 171), RGB(17, 21, 29), RGB(209, 215, 232), RGB(58, 66, 82)},
    {"nord-light", RGB(236, 239, 244), RGB(46, 52, 64), RGB(163, 190, 140), RGB(229, 233, 240), RGB(46, 52, 64), RGB(171, 178, 190)},
    {"zenburn", RGB(59, 58, 57), RGB(220, 216, 207), RGB(142, 192, 124), RGB(33, 33, 33), RGB(220, 216, 207), RGB(117, 113, 94)},
    {"ocean-dark", RGB(27, 44, 65), RGB(199, 215, 241), RGB(127, 203, 170), RGB(21, 36, 54), RGB(199, 215, 241), RGB(57, 80, 108)},
    {"synthwave", RGB(39, 27, 55), RGB(255, 241, 246), RGB(255, 107, 129), RGB(30, 20, 44), RGB(255, 241, 246), RGB(86, 66, 108)},
    {"everforest-light-warm", RGB(250, 244, 237), RGB(80, 73, 69), RGB(150, 157, 108), RGB(235, 231, 219), RGB(80, 73, 69), RGB(166, 163, 156)},
    {"catppuccin-latte-alt", RGB(237, 242, 244), RGB(74, 78, 104), RGB(94, 169, 39), RGB(227, 232, 238), RGB(74, 78, 104), RGB(168, 172, 186)},
    {"neon-green", RGB(0, 12, 0), RGB(175, 255, 175), RGB(0, 220, 0), RGB(0, 26, 0), RGB(175, 255, 175), RGB(0, 90, 0)},
    {"hotdog", RGB(60, 20, 10), RGB(255, 244, 200), RGB(255, 165, 0), RGB(45, 15, 8), RGB(255, 244, 200), RGB(120, 55, 30)},
    {"pastel", RGB(38, 36, 48), RGB(238, 236, 245), RGB(196, 227, 161), RGB(30, 29, 40), RGB(238, 236, 245), RGB(96, 93, 112)},
    {"mono-warm", RGB(45, 40, 38), RGB(226, 216, 206), RGB(180, 172, 165), RGB(32, 29, 27), RGB(196, 106, 96), RGB(210, 180, 140)}
};

static bool theme_name_eq(const char *a, const char *b) {
  while (*a && *b) {
    char ca = *a, cb = *b;
    if (ca >= 'A' && ca <= 'Z')
      ca += 'a' - 'A';
    if (cb >= 'A' && cb <= 'Z')
      cb += 'a' - 'A';
    if (ca != cb)
      return false;
    a++;
    b++;
  }
  return *a == *b;
}

static const struct {
  const char *name;
  size_t off;
} g_theme_slots[] = {
    {"COLOR_BG", offsetof(ThemeEntry, bg)},
    {"COLOR_TEXT", offsetof(ThemeEntry, text)},
    {"COLOR_ALTERNATE", offsetof(ThemeEntry, alternate)},
    {"COLOR_SELECTED_TEXT", offsetof(ThemeEntry, selected_text)},
    {"COLOR_CURSOR", offsetof(ThemeEntry, cursor)},
    {"COLOR_BORDER", offsetof(ThemeEntry, border)},
};
static ColorRef *g_theme_targets[] = {
    &COLOR_BG,          &COLOR_TEXT,
    &COLOR_ALTERNATE,   &COLOR_SELECTED_TEXT,
    &COLOR_CURSOR,
    &COLOR_BORDER,
};
#define THEME_SLOT_COUNT (sizeof(g_theme_slots) / sizeof(g_theme_slots[0]))

void Theme_Apply(void) {
  if (!Theme || !Theme[0])
    return;
  const ThemeEntry *found = NULL;
  for (size_t i = 0; i < sizeof(g_themes) / sizeof(g_themes[0]); i++) {
    if (theme_name_eq(Theme, g_themes[i].name)) {
      found = &g_themes[i];
      break;
    }
  }
  if (!found) {
    log_warn("unknown Theme \"%s\", keeping current colors", Theme);
    return;
  }
  ColorRef saved[THEME_SLOT_COUNT];
  for (size_t i = 0; i < THEME_SLOT_COUNT; i++)
    saved[i] = *g_theme_targets[i];
  for (size_t i = 0; i < THEME_SLOT_COUNT; i++)
    *g_theme_targets[i] =
        *(ColorRef *)((const char *)found + g_theme_slots[i].off);
  for (size_t i = 0; i < THEME_SLOT_COUNT; i++) {
    if (saved[i] != *g_theme_targets[i] &&
        Theme_ColorPinned(g_theme_slots[i].name))
      *g_theme_targets[i] = saved[i];
  }
}

#define THEME_MAX_PINNED 32
static char g_theme_pinned[THEME_MAX_PINNED][64];
static int g_theme_pinned_count = 0;
static bool g_theme_pinned_scanned = false;

static void theme_scan_config(void) {
  if (g_theme_pinned_scanned)
    return;
  g_theme_pinned_scanned = true;
  FILE *f = fopen(CONFIG_SRC, "rb");
  if (!f)
    return;
  char line[512];
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == ' ' || line[0] == '\t') {
      char name[64];
      if (sscanf(line, " %63[A-Z_] =", name) == 1 &&
          strncmp(name, "COLOR_", 6) == 0) {
        bool known = false;
        for (int i = 0; i < g_theme_pinned_count; i++) {
          if (strcmp(g_theme_pinned[i], name) == 0) {
            known = true;
            break;
          }
        }
        if (!known && g_theme_pinned_count < THEME_MAX_PINNED) {
          snprintf(g_theme_pinned[g_theme_pinned_count],
                   sizeof(g_theme_pinned[0]), "%s", name);
          g_theme_pinned_count++;
        }
      }
    }
  }
  fclose(f);
}

bool Theme_ColorPinned(const char *name) {
  if (!name || !name[0])
    return false;
  theme_scan_config();
  for (int i = 0; i < g_theme_pinned_count; i++) {
    if (strcmp(g_theme_pinned[i], name) == 0)
      return true;
  }
  return false;
}
