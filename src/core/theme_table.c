#include "core/theme.h"
#include "core/types/color.h"

#include <stddef.h>

static const ThemeEntry themes[] = {
    {"rose-pine", RGB(25, 23, 36), RGB(255, 255, 255), RGB(19, 21, 23),
     RGB(82, 79, 103), RGB(88, 86, 114)},
    {"tokyo-night", RGB(26, 27, 38), RGB(192, 202, 245), RGB(16, 17, 28),
     RGB(192, 202, 245), RGB(65, 72, 104)},
    {"gruvbox", RGB(40, 40, 40), RGB(235, 219, 178), RGB(30, 30, 30),
     RGB(235, 219, 178), RGB(102, 92, 84)},
    {"nord", RGB(46, 52, 64), RGB(236, 239, 244), RGB(36, 42, 54),
     RGB(236, 239, 244), RGB(76, 86, 106)},
    {"dracula", RGB(40, 42, 54), RGB(248, 248, 242), RGB(30, 32, 44),
     RGB(248, 248, 242), RGB(98, 114, 164)},
    {"catppuccin-mocha", RGB(30, 30, 46), RGB(205, 214, 244), RGB(24, 24, 37),
     RGB(205, 214, 244), RGB(88, 91, 112)},
    {"catppuccin-macchiato", RGB(36, 39, 58), RGB(202, 211, 245),
     RGB(30, 32, 48), RGB(202, 211, 245), RGB(91, 96, 120)},
    {"catppuccin-frappe", RGB(48, 52, 70), RGB(198, 208, 245), RGB(41, 44, 60),
     RGB(198, 208, 245), RGB(114, 116, 141)},
    {"catppuccin-latte", RGB(239, 241, 245), RGB(76, 79, 105),
     RGB(230, 233, 239), RGB(76, 79, 105), RGB(172, 176, 190)},
    {"onedark", RGB(40, 44, 52), RGB(171, 178, 191), RGB(33, 37, 43),
     RGB(171, 178, 191), RGB(76, 82, 99)},
    {"onedark-pro", RGB(40, 44, 52), RGB(171, 178, 191), RGB(33, 37, 43),
     RGB(255, 255, 255), RGB(76, 82, 99)},
    {"monokai", RGB(39, 40, 34), RGB(248, 248, 242), RGB(30, 31, 28),
     RGB(248, 248, 242), RGB(62, 61, 50)},
    {"solarized-dark", RGB(0, 43, 54), RGB(147, 161, 161), RGB(7, 54, 66),
     RGB(147, 161, 161), RGB(88, 110, 117)},
    {"solarized-light", RGB(253, 246, 227), RGB(101, 123, 131),
     RGB(238, 232, 213), RGB(101, 123, 131), RGB(147, 161, 161)},
    {"everforest-dark", RGB(45, 53, 59), RGB(211, 198, 170), RGB(39, 46, 51),
     RGB(211, 198, 170), RGB(76, 86, 93)},
    {"kanagawa", RGB(31, 31, 40), RGB(220, 215, 186), RGB(22, 22, 29),
     RGB(220, 215, 186), RGB(63, 63, 74)},
    {"rose-pine-moon", RGB(35, 33, 54), RGB(224, 222, 244), RGB(26, 24, 38),
     RGB(224, 222, 244), RGB(86, 82, 110)},
    {"rose-pine-dawn", RGB(250, 244, 237), RGB(87, 82, 121), RGB(242, 233, 225),
     RGB(87, 82, 121), RGB(152, 147, 165)},
    {"nightfox", RGB(19, 22, 30), RGB(205, 207, 213), RGB(14, 16, 22),
     RGB(205, 207, 213), RGB(51, 54, 63)},
    {"cyberpunk", RGB(13, 13, 24), RGB(224, 224, 255), RGB(8, 8, 16),
     RGB(255, 255, 255), RGB(42, 42, 62)},
    {"matrix", RGB(0, 0, 0), RGB(0, 255, 0), RGB(0, 0, 0), RGB(0, 255, 0),
     RGB(0, 120, 0)},
    {"amoled", RGB(0, 0, 0), RGB(255, 255, 255), RGB(10, 10, 10),
     RGB(255, 255, 255), RGB(60, 60, 60)},
    {"github-dark", RGB(13, 17, 23), RGB(201, 209, 217), RGB(22, 27, 34),
     RGB(201, 209, 217), RGB(48, 54, 61)},
    {"github-light", RGB(255, 255, 255), RGB(31, 35, 40), RGB(246, 248, 250),
     RGB(31, 35, 40), RGB(208, 215, 222)},
    {"ayu-dark", RGB(10, 14, 20), RGB(191, 189, 182), RGB(14, 18, 25),
     RGB(191, 189, 182), RGB(56, 61, 67)},
    {"ayu-light", RGB(250, 250, 250), RGB(92, 97, 102), RGB(242, 242, 242),
     RGB(92, 97, 102), RGB(184, 187, 191)},
    {"gruvbox-light", RGB(251, 241, 224), RGB(60, 56, 54), RGB(235, 219, 178),
     RGB(60, 56, 54), RGB(151, 138, 126)},
    {"tokyo-night-storm", RGB(36, 40, 59), RGB(192, 202, 245), RGB(41, 46, 66),
     RGB(192, 202, 245), RGB(86, 95, 137)},
    {"tokyo-night-day", RGB(213, 214, 219), RGB(52, 59, 88), RGB(224, 225, 228),
     RGB(52, 59, 88), RGB(140, 143, 147)},
    {"everforest-light", RGB(239, 243, 239), RGB(64, 68, 61),
     RGB(224, 230, 221), RGB(64, 68, 61), RGB(156, 160, 154)},
    {"material-ocean", RGB(15, 17, 26), RGB(214, 218, 227), RGB(21, 24, 34),
     RGB(214, 218, 227), RGB(60, 64, 78)},
    {"material-palenight", RGB(46, 50, 73), RGB(191, 194, 217), RGB(39, 42, 61),
     RGB(191, 194, 217), RGB(92, 99, 112)},
    {"moonfly", RGB(21, 25, 34), RGB(209, 215, 232), RGB(17, 21, 29),
     RGB(209, 215, 232), RGB(58, 66, 82)},
    {"nord-light", RGB(236, 239, 244), RGB(46, 52, 64), RGB(229, 233, 240),
     RGB(46, 52, 64), RGB(171, 178, 190)},
    {"zenburn", RGB(59, 58, 57), RGB(220, 216, 207), RGB(33, 33, 33),
     RGB(220, 216, 207), RGB(117, 113, 94)},
    {"ocean-dark", RGB(27, 44, 65), RGB(199, 215, 241), RGB(21, 36, 54),
     RGB(199, 215, 241), RGB(57, 80, 108)},
    {"synthwave", RGB(39, 27, 55), RGB(255, 241, 246), RGB(30, 20, 44),
     RGB(255, 241, 246), RGB(86, 66, 108)},
    {"everforest-light-warm", RGB(250, 244, 237), RGB(80, 73, 69),
     RGB(235, 231, 219), RGB(80, 73, 69), RGB(166, 163, 156)},
    {"catppuccin-latte-alt", RGB(237, 242, 244), RGB(74, 78, 104),
     RGB(227, 232, 238), RGB(74, 78, 104), RGB(168, 172, 186)},
    {"neon-green", RGB(0, 12, 0), RGB(175, 255, 175), RGB(0, 26, 0),
     RGB(175, 255, 175), RGB(0, 90, 0)},
    {"hotdog", RGB(60, 20, 10), RGB(255, 244, 200), RGB(45, 15, 8),
     RGB(255, 244, 200), RGB(120, 55, 30)},
    {"pastel", RGB(38, 36, 48), RGB(238, 236, 245), RGB(30, 29, 40),
     RGB(238, 236, 245), RGB(96, 93, 112)},
    {"mono-warm", RGB(45, 40, 38), RGB(226, 216, 206), RGB(32, 29, 27),
     RGB(196, 106, 96), RGB(210, 180, 140)},
    {"modus-operandi", RGB(255, 255, 255), RGB(0, 0, 0), RGB(218, 229, 236),
     RGB(0, 0, 0), RGB(159, 159, 159)},
    {"modus-vivendi", RGB(0, 0, 0), RGB(255, 255, 255), RGB(47, 56, 73),
     RGB(105, 105, 105), RGB(100, 100, 100)},
    {"modus-operandi-tinted", RGB(251, 247, 240), RGB(0, 0, 0),
     RGB(241, 213, 208), RGB(0, 0, 0), RGB(159, 150, 144)},
    {"modus-vivendi-tinted", RGB(13, 14, 28), RGB(255, 255, 255),
     RGB(48, 58, 111), RGB(255, 255, 255), RGB(97, 100, 122)}};

static bool theme_name_eq(const char *a, const char *b) {
  if (!a || !b)
    return false;
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

size_t theme_count(void) { return sizeof(themes) / sizeof(themes[0]); }

const ThemeEntry *theme_entry_at(size_t index) {
  if (index >= theme_count())
    return NULL;
  return &themes[index];
}

const ThemeEntry *theme_find(const char *name) {
  if (!name || !name[0])
    return NULL;
  for (size_t i = 0; i < theme_count(); i++) {
    if (theme_name_eq(name, themes[i].name))
      return &themes[i];
  }
  return NULL;
}
