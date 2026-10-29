#include "core/theme.h"
#include "config.h"
#include "deps/log.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#ifndef CONFIG_SRC
#define CONFIG_SRC "config/config.c"
#endif

typedef struct {
    const char* name;
    ColorRef bg;
    ColorRef text;
    ColorRef alternate;
    ColorRef selected_text;
    ColorRef error;
    ColorRef normal;
    ColorRef tip;
    ColorRef insert_mode;
    ColorRef normal_mode;
    ColorRef visual_mode;
    ColorRef cursor;
    ColorRef visual_cursor;
    ColorRef border;
} ThemeEntry;

static const ThemeEntry g_themes[] = {
    {"rose-pine",
        RGB(25,23,36), RGB(255, 255, 255), RGB(57, 255, 20), RGB(19,21,23),
        RGB(180, 0, 0), RGB(200, 200, 0), RGB(0, 160, 0), RGB(50, 255, 50),
        RGB(100, 255, 255), RGB(200, 3, 222), RGB(82, 79, 103), RGB(235, 111, 146),
        RGB(88, 86, 114)
    },
    {"tokyo-night",
        RGB(26, 27, 38), RGB(192, 202, 245), RGB(158, 206, 106), RGB(16, 17, 28),
        RGB(247, 118, 142), RGB(224, 175, 104), RGB(158, 206, 106), RGB(158, 206, 106),
        RGB(122, 162, 247), RGB(187, 154, 247), RGB(192, 202, 245), RGB(187, 154, 247),
        RGB(65, 72, 104)
    },
    {"gruvbox",
        RGB(40, 40, 40), RGB(235, 219, 178), RGB(184, 187, 38), RGB(30, 30, 30),
        RGB(251, 73, 52), RGB(250, 189, 47), RGB(184, 187, 38), RGB(184, 187, 38),
        RGB(131, 165, 152), RGB(211, 134, 155), RGB(235, 219, 178), RGB(254, 128, 25),
        RGB(102, 92, 84)
    },
    {"nord",
        RGB(46, 52, 64), RGB(236, 239, 244), RGB(163, 190, 140), RGB(36, 42, 54),
        RGB(191, 97, 106), RGB(235, 203, 139), RGB(163, 190, 140), RGB(163, 190, 140),
        RGB(136, 192, 208), RGB(180, 142, 173), RGB(236, 239, 244), RGB(180, 142, 173),
        RGB(76, 86, 106)
    },
    {"dracula",
        RGB(40, 42, 54), RGB(248, 248, 242), RGB(80, 250, 123), RGB(30, 32, 44),
        RGB(255, 85, 85), RGB(241, 250, 140), RGB(80, 250, 123), RGB(80, 250, 123),
        RGB(139, 233, 253), RGB(189, 147, 249), RGB(248, 248, 242), RGB(255, 121, 198),
        RGB(98, 114, 164)
    },
    {"catppuccin-mocha",
        RGB(30, 30, 46), RGB(205, 214, 244), RGB(166, 227, 161), RGB(24, 24, 37),
        RGB(243, 139, 168), RGB(249, 226, 175), RGB(166, 227, 161), RGB(166, 227, 161),
        RGB(137, 180, 250), RGB(203, 166, 247), RGB(205, 214, 244), RGB(243, 139, 168),
        RGB(88, 91, 112)
    },
    {"catppuccin-macchiato",
        RGB(36, 39, 58), RGB(202, 211, 245), RGB(166, 218, 149), RGB(30, 32, 48),
        RGB(237, 135, 150), RGB(238, 212, 159), RGB(166, 218, 149), RGB(166, 218, 149),
        RGB(138, 173, 244), RGB(198, 160, 246), RGB(202, 211, 245), RGB(237, 135, 150),
        RGB(91, 96, 120)
    },
    {"catppuccin-frappe",
        RGB(48, 52, 70), RGB(198, 208, 245), RGB(166, 209, 137), RGB(41, 44, 60),
        RGB(231, 130, 132), RGB(229, 200, 144), RGB(166, 209, 137), RGB(166, 209, 137),
        RGB(140, 170, 238), RGB(202, 158, 230), RGB(198, 208, 245), RGB(231, 130, 132),
        RGB(114, 116, 141)
    },
    {"catppuccin-latte",
        RGB(239, 241, 245), RGB(76, 79, 105), RGB(64, 160, 43), RGB(230, 233, 239),
        RGB(210, 15, 57), RGB(223, 142, 29), RGB(64, 160, 43), RGB(64, 160, 43),
        RGB(30, 102, 245), RGB(136, 57, 239), RGB(76, 79, 105), RGB(210, 15, 57),
        RGB(172, 176, 190)
    },
    {"onedark",
        RGB(40, 44, 52), RGB(171, 178, 191), RGB(152, 195, 121), RGB(33, 37, 43),
        RGB(224, 108, 117), RGB(229, 192, 123), RGB(152, 195, 121), RGB(152, 195, 121),
        RGB(97, 175, 239), RGB(198, 120, 221), RGB(171, 178, 191), RGB(198, 120, 221),
        RGB(76, 82, 99)
    },
    {"onedark-pro",
        RGB(40, 44, 52), RGB(171, 178, 191), RGB(152, 195, 121), RGB(33, 37, 43),
        RGB(224, 108, 117), RGB(229, 192, 123), RGB(152, 195, 121), RGB(152, 195, 121),
        RGB(86, 182, 194), RGB(198, 120, 221), RGB(255, 255, 255), RGB(198, 120, 221),
        RGB(76, 82, 99)
    },
    {"monokai",
        RGB(39, 40, 34), RGB(248, 248, 242), RGB(166, 226, 46), RGB(30, 31, 28),
        RGB(249, 38, 114), RGB(230, 219, 116), RGB(166, 226, 46), RGB(166, 226, 46),
        RGB(102, 217, 239), RGB(174, 129, 255), RGB(248, 248, 242), RGB(249, 38, 114),
        RGB(62, 61, 50)
    },
    {"solarized-dark",
        RGB(0, 43, 54), RGB(147, 161, 161), RGB(133, 153, 0), RGB(7, 54, 66),
        RGB(220, 50, 47), RGB(181, 137, 0), RGB(133, 153, 0), RGB(133, 153, 0),
        RGB(38, 139, 210), RGB(211, 54, 130), RGB(147, 161, 161), RGB(211, 54, 130),
        RGB(88, 110, 117)
    },
    {"solarized-light",
        RGB(253, 246, 227), RGB(101, 123, 131), RGB(133, 153, 0), RGB(238, 232, 213),
        RGB(220, 50, 47), RGB(181, 137, 0), RGB(133, 153, 0), RGB(133, 153, 0),
        RGB(38, 139, 210), RGB(211, 54, 130), RGB(101, 123, 131), RGB(211, 54, 130),
        RGB(147, 161, 161)
    },
    {"everforest-dark",
        RGB(45, 53, 59), RGB(211, 198, 170), RGB(167, 192, 128), RGB(39, 46, 51),
        RGB(230, 126, 128), RGB(219, 183, 113), RGB(167, 192, 128), RGB(167, 192, 128),
        RGB(127, 187, 179), RGB(214, 153, 225), RGB(211, 198, 170), RGB(214, 153, 225),
        RGB(76, 86, 93)
    },
    {"kanagawa",
        RGB(31, 31, 40), RGB(220, 215, 186), RGB(135, 169, 135), RGB(22, 22, 29),
        RGB(228, 104, 118), RGB(220, 165, 97), RGB(135, 169, 135), RGB(135, 169, 135),
        RGB(126, 156, 216), RGB(149, 127, 184), RGB(220, 215, 186), RGB(187, 154, 247),
        RGB(63, 63, 74)
    },
    {"rose-pine-moon",
        RGB(35, 33, 54), RGB(224, 222, 244), RGB(62, 143, 176), RGB(26, 24, 38),
        RGB(235, 111, 146), RGB(246, 193, 119), RGB(62, 143, 176), RGB(62, 143, 176),
        RGB(156, 207, 216), RGB(196, 167, 231), RGB(224, 222, 244), RGB(235, 111, 146),
        RGB(86, 82, 110)
    },
    {"rose-pine-dawn",
        RGB(250, 244, 237), RGB(87, 82, 121), RGB(40, 105, 131), RGB(242, 233, 225),
        RGB(180, 99, 122), RGB(234, 157, 52), RGB(40, 105, 131), RGB(40, 105, 131),
        RGB(144, 122, 169), RGB(144, 122, 169), RGB(87, 82, 121), RGB(180, 99, 122),
        RGB(152, 147, 165)
    },
    {"nightfox",
        RGB(19, 22, 30), RGB(205, 207, 213), RGB(167, 195, 129), RGB(14, 16, 22),
        RGB(214, 112, 112), RGB(224, 175, 104), RGB(167, 195, 129), RGB(167, 195, 129),
        RGB(113, 153, 209), RGB(157, 134, 187), RGB(205, 207, 213), RGB(195, 232, 141),
        RGB(51, 54, 63)
    },
    {"cyberpunk",
        RGB(13, 13, 24), RGB(224, 224, 255), RGB(0, 255, 200), RGB(8, 8, 16),
        RGB(255, 60, 60), RGB(255, 230, 60), RGB(0, 255, 200), RGB(0, 255, 200),
        RGB(255, 0, 128), RGB(180, 0, 255), RGB(255, 255, 255), RGB(255, 60, 60),
        RGB(42, 42, 62)
    },
    {"matrix",
        RGB(0, 0, 0), RGB(0, 255, 0), RGB(0, 180, 0), RGB(0, 0, 0),
        RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 220, 0), RGB(0, 255, 0),
        RGB(0, 180, 0), RGB(0, 255, 0), RGB(0, 255, 0), RGB(255, 100, 0),
        RGB(0, 120, 0)
    },
    {"amoled",
        RGB(0, 0, 0), RGB(255, 255, 255), RGB(0, 200, 255), RGB(10, 10, 10),
        RGB(255, 60, 60), RGB(255, 200, 0), RGB(0, 200, 255), RGB(0, 200, 255),
        RGB(0, 255, 128), RGB(200, 100, 255), RGB(255, 255, 255), RGB(255, 60, 255),
        RGB(60, 60, 60)
    },
    {"github-dark",
        RGB(13, 17, 23), RGB(201, 209, 217), RGB(63, 185, 80), RGB(22, 27, 34),
        RGB(248, 81, 73), RGB(210, 153, 34), RGB(63, 185, 80), RGB(63, 185, 80),
        RGB(88, 166, 255), RGB(188, 140, 255), RGB(201, 209, 217), RGB(255, 123, 114),
        RGB(48, 54, 61)
    },
    {"github-light",
        RGB(255, 255, 255), RGB(31, 35, 40), RGB(26, 127, 55), RGB(246, 248, 250),
        RGB(207, 34, 46), RGB(154, 103, 0), RGB(26, 127, 55), RGB(26, 127, 55),
        RGB(9, 105, 218), RGB(130, 80, 223), RGB(31, 35, 40), RGB(207, 34, 46),
        RGB(208, 215, 222)
    },
    {"ayu-dark",
        RGB(10, 14, 20), RGB(191, 189, 182), RGB(170, 217, 76), RGB(14, 18, 25),
        RGB(242, 113, 113), RGB(255, 180, 84), RGB(170, 217, 76), RGB(170, 217, 76),
        RGB(89, 194, 255), RGB(209, 148, 255), RGB(191, 189, 182), RGB(242, 113, 120),
        RGB(56, 61, 67)
    },
    {"ayu-light",
        RGB(250, 250, 250), RGB(92, 97, 102), RGB(134, 179, 0), RGB(242, 242, 242),
        RGB(240, 113, 120), RGB(242, 149, 0), RGB(134, 179, 0), RGB(134, 179, 0),
        RGB(54, 163, 247), RGB(163, 122, 204), RGB(92, 97, 102), RGB(242, 113, 120),
        RGB(184, 187, 191)
    },
    {"gruvbox-light",
        RGB(251, 241, 224), RGB(60, 56, 54), RGB(121, 116, 14), RGB(235, 219, 178),
        RGB(157, 0, 6), RGB(181, 118, 20), RGB(121, 116, 14), RGB(121, 116, 14),
        RGB(7, 102, 120), RGB(143, 63, 113), RGB(60, 56, 54), RGB(175, 58, 3),
        RGB(151, 138, 126)
    },
    {"tokyo-night-storm",
        RGB(36, 40, 59), RGB(192, 202, 245), RGB(158, 206, 106), RGB(41, 46, 66),
        RGB(247, 118, 142), RGB(224, 175, 104), RGB(158, 206, 106), RGB(158, 206, 106),
        RGB(125, 207, 255), RGB(187, 154, 247), RGB(192, 202, 245), RGB(187, 154, 247),
        RGB(86, 95, 137)
    },
    {"tokyo-night-day",
        RGB(213, 214, 219), RGB(52, 59, 88), RGB(143, 153, 0), RGB(224, 225, 228),
        RGB(203, 66, 53), RGB(152, 159, 0), RGB(143, 153, 0), RGB(143, 153, 0),
        RGB(52, 84, 138), RGB(150, 76, 230), RGB(52, 59, 88), RGB(168, 121, 255),
        RGB(140, 143, 147)
    },
    {"everforest-light",
        RGB(239, 243, 239), RGB(64, 68, 61), RGB(134, 149, 90), RGB(224, 230, 221),
        RGB(193, 66, 62), RGB(209, 154, 63), RGB(134, 149, 90), RGB(134, 149, 90),
        RGB(6, 132, 149), RGB(148, 137, 194), RGB(64, 68, 61), RGB(207, 89, 148),
        RGB(156, 160, 154)
    },
    {"material-ocean",
        RGB(15, 17, 26), RGB(214, 218, 227), RGB(152, 195, 121), RGB(21, 24, 34),
        RGB(240, 113, 120), RGB(255, 203, 107), RGB(128, 203, 196), RGB(128, 203, 196),
        RGB(130, 170, 255), RGB(178, 159, 197), RGB(214, 218, 227), RGB(240, 113, 120),
        RGB(60, 64, 78)
    },
    {"material-palenight",
        RGB(46, 50, 73), RGB(191, 194, 217), RGB(152, 195, 121), RGB(39, 42, 61),
        RGB(240, 113, 120), RGB(249, 158, 88), RGB(128, 203, 196), RGB(128, 203, 196),
        RGB(130, 170, 255), RGB(184, 187, 224), RGB(191, 194, 217), RGB(240, 113, 120),
        RGB(92, 99, 112)
    },
    {"moonfly",
        RGB(21, 25, 34), RGB(209, 215, 232), RGB(178, 229, 171), RGB(17, 21, 29),
        RGB(240, 136, 147), RGB(249, 226, 175), RGB(150, 235, 216), RGB(150, 235, 216),
        RGB(102, 217, 239), RGB(198, 167, 255), RGB(209, 215, 232), RGB(240, 136, 147),
        RGB(58, 66, 82)
    },
    {"nord-light",
        RGB(236, 239, 244), RGB(46, 52, 64), RGB(163, 190, 140), RGB(229, 233, 240),
        RGB(180, 65, 72), RGB(208, 135, 112), RGB(120, 154, 159), RGB(163, 190, 140),
        RGB(94, 129, 172), RGB(129, 161, 193), RGB(46, 52, 64), RGB(180, 65, 72),
        RGB(171, 178, 190)
    },
    {"zenburn",
        RGB(59, 58, 57), RGB(220, 216, 207), RGB(142, 192, 124), RGB(33, 33, 33),
        RGB(204, 90, 88), RGB(223, 179, 96), RGB(142, 192, 124), RGB(142, 192, 124),
        RGB(115, 175, 209), RGB(187, 154, 247), RGB(220, 216, 207), RGB(238, 232, 213),
        RGB(117, 113, 94)
    },
    {"ocean-dark",
        RGB(27, 44, 65), RGB(199, 215, 241), RGB(127, 203, 170), RGB(21, 36, 54),
        RGB(255, 88, 91), RGB(255, 198, 88), RGB(127, 203, 170), RGB(127, 203, 170),
        RGB(1, 171, 199), RGB(198, 167, 255), RGB(199, 215, 241), RGB(255, 88, 91),
        RGB(57, 80, 108)
    },
    {"synthwave",
        RGB(39, 27, 55), RGB(255, 241, 246), RGB(255, 107, 129), RGB(30, 20, 44),
        RGB(255, 71, 115), RGB(255, 207, 51), RGB(114, 241, 244), RGB(114, 241, 244),
        RGB(255, 71, 115), RGB(178, 98, 255), RGB(255, 241, 246), RGB(255, 71, 115),
        RGB(86, 66, 108)
    },
    {"everforest-light-warm",
        RGB(250, 244, 237), RGB(80, 73, 69), RGB(150, 157, 108), RGB(235, 231, 219),
        RGB(197, 82, 77), RGB(217, 164, 78), RGB(150, 157, 108), RGB(150, 157, 108),
        RGB(0, 138, 155), RGB(160, 146, 199), RGB(80, 73, 69), RGB(207, 107, 160),
        RGB(166, 163, 156)
    },
    {"catppuccin-latte-alt",
        RGB(237, 242, 244), RGB(74, 78, 104), RGB(94, 169, 39), RGB(227, 232, 238),
        RGB(210, 15, 57), RGB(223, 142, 29), RGB(64, 160, 43), RGB(64, 160, 43),
        RGB(30, 102, 245), RGB(136, 57, 239), RGB(74, 78, 104), RGB(210, 15, 57),
        RGB(168, 172, 186)
    },
    {"neon-green",
        RGB(0, 12, 0), RGB(175, 255, 175), RGB(0, 220, 0), RGB(0, 26, 0),
        RGB(255, 40, 40), RGB(255, 220, 0), RGB(0, 255, 140), RGB(0, 255, 140),
        RGB(0, 200, 255), RGB(180, 0, 255), RGB(175, 255, 175), RGB(255, 0, 180),
        RGB(0, 90, 0)
    },
    {"hotdog",
        RGB(60, 20, 10), RGB(255, 244, 200), RGB(255, 165, 0), RGB(45, 15, 8),
        RGB(220, 40, 20), RGB(255, 220, 90), RGB(255, 165, 0), RGB(255, 165, 0),
        RGB(255, 200, 40), RGB(200, 60, 180), RGB(255, 244, 200), RGB(255, 60, 90),
        RGB(120, 55, 30)
    },
    {"pastel",
        RGB(38, 36, 48), RGB(238, 236, 245), RGB(196, 227, 161), RGB(30, 29, 40),
        RGB(247, 118, 142), RGB(255, 213, 153), RGB(196, 227, 161), RGB(196, 227, 161),
        RGB(137, 180, 250), RGB(203, 166, 247), RGB(238, 236, 245), RGB(255, 150, 175),
        RGB(96, 93, 112)
    },
    {"mono-warm",
        RGB(45, 40, 38), RGB(226, 216, 206), RGB(180, 172, 165), RGB(32, 29, 27),
        RGB(196, 106, 96), RGB(210, 180, 140), RGB(180, 172, 165), RGB(180, 172, 165),
        RGB(190, 150, 130), RGB(170, 150, 190), RGB(226, 216, 206), RGB(214, 140, 130),
        RGB(88, 80, 76)
    },
};

static bool theme_name_eq(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == *b;
}

/* Maps each theme slot to the config global it drives, so a per-colour
   config value can win over the theme. Order must match ThemeEntry. */
static const struct { const char* name; size_t off; } g_theme_slots[] = {
    { "COLOR_BG",            offsetof(ThemeEntry, bg) },
    { "COLOR_TEXT",          offsetof(ThemeEntry, text) },
    { "COLOR_ALTERNATE",     offsetof(ThemeEntry, alternate) },
    { "COLOR_SELECTED_TEXT", offsetof(ThemeEntry, selected_text) },
    { "COLOR_ERROR",         offsetof(ThemeEntry, error) },
    { "COLOR_NORMAL",        offsetof(ThemeEntry, normal) },
    { "COLOR_TIP",           offsetof(ThemeEntry, tip) },
    { "COLOR_INSERT_MODE",   offsetof(ThemeEntry, insert_mode) },
    { "COLOR_NORMAL_MODE",   offsetof(ThemeEntry, normal_mode) },
    { "COLOR_VISUAL_MODE",   offsetof(ThemeEntry, visual_mode) },
    { "COLOR_CURSOR",        offsetof(ThemeEntry, cursor) },
    { "COLOR_VISUAL_CURSOR", offsetof(ThemeEntry, visual_cursor) },
    { "COLOR_BORDER",        offsetof(ThemeEntry, border) },
};
static ColorRef* g_theme_targets[] = {
    &COLOR_BG, &COLOR_TEXT, &COLOR_ALTERNATE, &COLOR_SELECTED_TEXT,
    &COLOR_ERROR, &COLOR_NORMAL, &COLOR_TIP,
    &COLOR_INSERT_MODE, &COLOR_NORMAL_MODE, &COLOR_VISUAL_MODE, &COLOR_CURSOR,
    &COLOR_VISUAL_CURSOR, &COLOR_BORDER,
};
#define THEME_SLOT_COUNT (sizeof(g_theme_slots) / sizeof(g_theme_slots[0]))

void Theme_Apply(void) {
    if (!Theme || !Theme[0]) return;
    const ThemeEntry* found = NULL;
    for (size_t i = 0; i < sizeof(g_themes) / sizeof(g_themes[0]); i++) {
        if (theme_name_eq(Theme, g_themes[i].name)) { found = &g_themes[i]; break; }
    }
    if (!found) { log_warn("unknown Theme \"%s\", keeping current colors", Theme); return; }
    ColorRef saved[THEME_SLOT_COUNT];
    for (size_t i = 0; i < THEME_SLOT_COUNT; i++) saved[i] = *g_theme_targets[i];
    for (size_t i = 0; i < THEME_SLOT_COUNT; i++)
        *g_theme_targets[i] = *(ColorRef*)((const char*)found + g_theme_slots[i].off);
    for (size_t i = 0; i < THEME_SLOT_COUNT; i++) {
        if (saved[i] != *g_theme_targets[i] && Theme_ColorPinned(g_theme_slots[i].name))
            *g_theme_targets[i] = saved[i];
    }
}

#define THEME_MAX_PINNED 32
static char g_theme_pinned[THEME_MAX_PINNED][64];
static int g_theme_pinned_count = 0;
static bool g_theme_pinned_scanned = false;

static void theme_scan_config(void) {
    if (g_theme_pinned_scanned) return;
    g_theme_pinned_scanned = true;
    FILE *f = fopen(CONFIG_SRC, "rb");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == ' ' || line[0] == '\t') {
            char name[64];
            if (sscanf(line, " %63[A-Z_] =", name) == 1 &&
                strncmp(name, "COLOR_", 6) == 0) {
                bool known = false;
                for (int i = 0; i < g_theme_pinned_count; i++) {
                    if (strcmp(g_theme_pinned[i], name) == 0) { known = true; break; }
                }
                if (!known && g_theme_pinned_count < THEME_MAX_PINNED) {
                    snprintf(g_theme_pinned[g_theme_pinned_count], sizeof(g_theme_pinned[0]), "%s", name);
                    g_theme_pinned_count++;
                }
            }
        }
    }
    fclose(f);
}

bool Theme_ColorPinned(const char *name) {
    if (!name || !name[0]) return false;
    theme_scan_config();
    for (int i = 0; i < g_theme_pinned_count; i++) {
        if (strcmp(g_theme_pinned[i], name) == 0) return true;
    }
    return false;
}
