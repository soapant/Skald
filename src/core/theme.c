#include "core/theme.h"
#include "config.h"
#include "deps/log.h"
#include <string.h>

typedef struct {
    const char* name;
    ColorRef bg;
    ColorRef text;
    ColorRef alternate;
    ColorRef selected_text;
    ColorRef alias_text;
    ColorRef error;
    ColorRef normal;
    ColorRef tip;
    ColorRef insert_mode;
    ColorRef normal_mode;
    ColorRef visual_mode;
    ColorRef cursor;
} ThemeEntry;

static const ThemeEntry g_themes[] = {
    {
        "rose-pine",
        RGB(25,23,36), RGB(255, 255, 255), RGB(57, 255, 20), RGB(19,21,23),
        RGB(255, 255, 255), RGB(180, 0, 0), RGB(200, 200, 0), RGB(0, 160, 0),
        RGB(50, 255, 50), RGB(100, 255, 255), RGB(200, 3, 222), RGB(82, 79, 103),
    },
    {
        "tokyo-night",
        RGB(26, 27, 38), RGB(192, 202, 245), RGB(158, 206, 106), RGB(16, 17, 28),
        RGB(122, 162, 247), RGB(247, 118, 142), RGB(224, 175, 104), RGB(158, 206, 106),
        RGB(158, 206, 106), RGB(122, 162, 247), RGB(187, 154, 247), RGB(192, 202, 245),
    },
    {
        "gruvbox",
        RGB(40, 40, 40), RGB(235, 219, 178), RGB(184, 187, 38), RGB(30, 30, 30),
        RGB(131, 165, 152), RGB(251, 73, 52), RGB(250, 189, 47), RGB(184, 187, 38),
        RGB(184, 187, 38), RGB(131, 165, 152), RGB(211, 134, 155), RGB(235, 219, 178),
    },
    {
        "nord",
        RGB(46, 52, 64), RGB(236, 239, 244), RGB(163, 190, 140), RGB(36, 42, 54),
        RGB(136, 192, 208), RGB(191, 97, 106), RGB(235, 203, 139), RGB(163, 190, 140),
        RGB(163, 190, 140), RGB(136, 192, 208), RGB(180, 142, 173), RGB(236, 239, 244),
    },
    {
        "dracula",
        RGB(40, 42, 54), RGB(248, 248, 242), RGB(80, 250, 123), RGB(30, 32, 44),
        RGB(139, 233, 253), RGB(255, 85, 85), RGB(241, 250, 140), RGB(80, 250, 123),
        RGB(80, 250, 123), RGB(139, 233, 253), RGB(189, 147, 249), RGB(248, 248, 242),
    },
    {
        "catppuccin-mocha",
        RGB(30, 30, 46), RGB(205, 214, 244), RGB(166, 227, 161), RGB(24, 24, 37),
        RGB(137, 180, 250), RGB(243, 139, 168), RGB(249, 226, 175), RGB(166, 227, 161),
        RGB(166, 227, 161), RGB(137, 180, 250), RGB(203, 166, 247), RGB(205, 214, 244),
    },
    {
        "catppuccin-macchiato",
        RGB(36, 39, 58), RGB(202, 211, 245), RGB(166, 218, 149), RGB(30, 32, 48),
        RGB(138, 173, 244), RGB(237, 135, 150), RGB(238, 212, 159), RGB(166, 218, 149),
        RGB(166, 218, 149), RGB(138, 173, 244), RGB(198, 160, 246), RGB(202, 211, 245),
    },
    {
        "catppuccin-frappe",
        RGB(48, 52, 70), RGB(198, 208, 245), RGB(166, 209, 137), RGB(41, 44, 60),
        RGB(140, 170, 238), RGB(231, 130, 132), RGB(229, 200, 144), RGB(166, 209, 137),
        RGB(166, 209, 137), RGB(140, 170, 238), RGB(202, 158, 230), RGB(198, 208, 245),
    },
    {
        "catppuccin-latte",
        RGB(239, 241, 245), RGB(76, 79, 105), RGB(64, 160, 43), RGB(230, 233, 239),
        RGB(30, 102, 245), RGB(210, 15, 57), RGB(223, 142, 29), RGB(64, 160, 43),
        RGB(64, 160, 43), RGB(30, 102, 245), RGB(136, 57, 239), RGB(76, 79, 105),
    },
    {
        "onedark",
        RGB(40, 44, 52), RGB(171, 178, 191), RGB(152, 195, 121), RGB(33, 37, 43),
        RGB(97, 175, 239), RGB(224, 108, 117), RGB(229, 192, 123), RGB(152, 195, 121),
        RGB(152, 195, 121), RGB(97, 175, 239), RGB(198, 120, 221), RGB(171, 178, 191),
    },
    {
        "onedark-pro",
        RGB(40, 44, 52), RGB(171, 178, 191), RGB(152, 195, 121), RGB(33, 37, 43),
        RGB(97, 175, 239), RGB(224, 108, 117), RGB(229, 192, 123), RGB(152, 195, 121),
        RGB(152, 195, 121), RGB(86, 182, 194), RGB(198, 120, 221), RGB(255, 255, 255),
    },
    {
        "monokai",
        RGB(39, 40, 34), RGB(248, 248, 242), RGB(166, 226, 46), RGB(30, 31, 28),
        RGB(102, 217, 239), RGB(249, 38, 114), RGB(230, 219, 116), RGB(166, 226, 46),
        RGB(166, 226, 46), RGB(102, 217, 239), RGB(174, 129, 255), RGB(248, 248, 242),
    },
    {
        "solarized-dark",
        RGB(0, 43, 54), RGB(147, 161, 161), RGB(133, 153, 0), RGB(7, 54, 66),
        RGB(38, 139, 210), RGB(220, 50, 47), RGB(181, 137, 0), RGB(133, 153, 0),
        RGB(133, 153, 0), RGB(38, 139, 210), RGB(211, 54, 130), RGB(147, 161, 161),
    },
    {
        "solarized-light",
        RGB(253, 246, 227), RGB(101, 123, 131), RGB(133, 153, 0), RGB(238, 232, 213),
        RGB(38, 139, 210), RGB(220, 50, 47), RGB(181, 137, 0), RGB(133, 153, 0),
        RGB(133, 153, 0), RGB(38, 139, 210), RGB(211, 54, 130), RGB(101, 123, 131),
    },
    {
        "everforest-dark",
        RGB(45, 53, 59), RGB(211, 198, 170), RGB(167, 192, 128), RGB(39, 46, 51),
        RGB(127, 187, 179), RGB(230, 126, 128), RGB(219, 183, 113), RGB(167, 192, 128),
        RGB(167, 192, 128), RGB(127, 187, 179), RGB(214, 153, 225), RGB(211, 198, 170),
    },
    {
        "kanagawa",
        RGB(31, 31, 40), RGB(220, 215, 186), RGB(135, 169, 135), RGB(22, 22, 29),
        RGB(126, 156, 216), RGB(228, 104, 118), RGB(220, 165, 97), RGB(135, 169, 135),
        RGB(135, 169, 135), RGB(126, 156, 216), RGB(149, 127, 184), RGB(220, 215, 186),
    },
    {
        "rose-pine-moon",
        RGB(35, 33, 54), RGB(224, 222, 244), RGB(62, 143, 176), RGB(26, 24, 38),
        RGB(156, 207, 216), RGB(235, 111, 146), RGB(246, 193, 119), RGB(62, 143, 176),
        RGB(62, 143, 176), RGB(156, 207, 216), RGB(196, 167, 231), RGB(224, 222, 244),
    },
    {
        "rose-pine-dawn",
        RGB(250, 244, 237), RGB(87, 82, 121), RGB(40, 105, 131), RGB(242, 233, 225),
        RGB(144, 122, 169), RGB(180, 99, 122), RGB(234, 157, 52), RGB(40, 105, 131),
        RGB(40, 105, 131), RGB(144, 122, 169), RGB(144, 122, 169), RGB(87, 82, 121),
    },
    {
        "nightfox",
        RGB(19, 22, 30), RGB(205, 207, 213), RGB(167, 195, 129), RGB(14, 16, 22),
        RGB(113, 153, 209), RGB(214, 112, 112), RGB(224, 175, 104), RGB(167, 195, 129),
        RGB(167, 195, 129), RGB(113, 153, 209), RGB(157, 134, 187), RGB(205, 207, 213),
    },
    {
        "cyberpunk",
        RGB(13, 13, 24), RGB(224, 224, 255), RGB(0, 255, 200), RGB(8, 8, 16),
        RGB(255, 0, 128), RGB(255, 60, 60), RGB(255, 230, 60), RGB(0, 255, 200),
        RGB(0, 255, 200), RGB(255, 0, 128), RGB(180, 0, 255), RGB(255, 255, 255),
    },
    {
        "matrix",
        RGB(0, 0, 0), RGB(0, 255, 0), RGB(0, 180, 0), RGB(0, 0, 0),
        RGB(0, 255, 0), RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 220, 0),
        RGB(0, 255, 0), RGB(0, 180, 0), RGB(0, 255, 0), RGB(0, 255, 0),
    },
    {
        "amoled",
        RGB(0, 0, 0), RGB(255, 255, 255), RGB(0, 200, 255), RGB(10, 10, 10),
        RGB(0, 200, 255), RGB(255, 60, 60), RGB(255, 200, 0), RGB(0, 200, 255),
        RGB(0, 200, 255), RGB(0, 255, 128), RGB(200, 100, 255), RGB(255, 255, 255),
    },
    {
        "github-dark",
        RGB(13, 17, 23), RGB(201, 209, 217), RGB(63, 185, 80), RGB(22, 27, 34),
        RGB(88, 166, 255), RGB(248, 81, 73), RGB(210, 153, 34), RGB(63, 185, 80),
        RGB(63, 185, 80), RGB(88, 166, 255), RGB(188, 140, 255), RGB(201, 209, 217),
    },
    {
        "github-light",
        RGB(255, 255, 255), RGB(31, 35, 40), RGB(26, 127, 55), RGB(246, 248, 250),
        RGB(9, 105, 218), RGB(207, 34, 46), RGB(154, 103, 0), RGB(26, 127, 55),
        RGB(26, 127, 55), RGB(9, 105, 218), RGB(130, 80, 223), RGB(31, 35, 40),
    },
    {
        "ayu-dark",
        RGB(10, 14, 20), RGB(191, 189, 182), RGB(170, 217, 76), RGB(14, 18, 25),
        RGB(89, 194, 255), RGB(242, 113, 113), RGB(255, 180, 84), RGB(170, 217, 76),
        RGB(170, 217, 76), RGB(89, 194, 255), RGB(209, 148, 255), RGB(191, 189, 182),
    },
    {
        "ayu-light",
        RGB(250, 250, 250), RGB(92, 97, 102), RGB(134, 179, 0), RGB(242, 242, 242),
        RGB(54, 163, 247), RGB(240, 113, 120), RGB(242, 149, 0), RGB(134, 179, 0),
        RGB(134, 179, 0), RGB(54, 163, 247), RGB(163, 122, 204), RGB(92, 97, 102),
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

void Theme_Apply(void) {
    if (!Theme || !Theme[0]) return;
    const ThemeEntry* found = NULL;
    for (size_t i = 0; i < sizeof(g_themes) / sizeof(g_themes[0]); i++) {
        if (theme_name_eq(Theme, g_themes[i].name)) { found = &g_themes[i]; break; }
    }
    if (!found) { log_warn("unknown Theme \"%s\", keeping current colors", Theme); return; }
    COLOR_BG = found->bg;
    COLOR_TEXT = found->text;
    COLOR_ALTERNATE = found->alternate;
    COLOR_SELECTED_TEXT = found->selected_text;
    COLOR_ALIAS_TEXT = found->alias_text;
    COLOR_ERROR = found->error;
    COLOR_NORMAL = found->normal;
    COLOR_TIP = found->tip;
    COLOR_INSERT_MODE = found->insert_mode;
    COLOR_NORMAL_MODE = found->normal_mode;
    COLOR_VISUAL_MODE = found->visual_mode;
    COLOR_CURSOR = found->cursor;
}

