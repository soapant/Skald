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
    float shader_intensity;
    float gradient_brightness;
} ThemeEntry;

static const ThemeEntry g_themes[] = {
    {
        "rose-pine",
        RGB(25,23,36), RGB(255, 255, 255), RGB(57, 255, 20), RGB(10, 11, 13),
        RGB(255, 255, 255), RGB(180, 0, 0), RGB(200, 200, 0), RGB(0, 160, 0),
        RGB(50, 255, 50), RGB(100, 255, 255), RGB(200, 3, 222), RGB(82, 79, 103),
        0.001f, 1.0f,
    },
    {
        "tokyo-night",
        RGB(26, 27, 38), RGB(192, 202, 245), RGB(158, 206, 106), RGB(16, 17, 28),
        RGB(122, 162, 247), RGB(247, 118, 142), RGB(224, 175, 104), RGB(158, 206, 106),
        RGB(158, 206, 106), RGB(122, 162, 247), RGB(187, 154, 247), RGB(192, 202, 245),
        0.001f, 1.0f,
    },
    {
        "gruvbox",
        RGB(40, 40, 40), RGB(235, 219, 178), RGB(184, 187, 38), RGB(30, 30, 30),
        RGB(131, 165, 152), RGB(251, 73, 52), RGB(250, 189, 47), RGB(184, 187, 38),
        RGB(184, 187, 38), RGB(131, 165, 152), RGB(211, 134, 155), RGB(235, 219, 178),
        0.001f, 1.0f,
    },
    {
        "nord",
        RGB(46, 52, 64), RGB(236, 239, 244), RGB(163, 190, 140), RGB(36, 42, 54),
        RGB(136, 192, 208), RGB(191, 97, 106), RGB(235, 203, 139), RGB(163, 190, 140),
        RGB(163, 190, 140), RGB(136, 192, 208), RGB(180, 142, 173), RGB(236, 239, 244),
        0.001f, 1.0f,
    },
    {
        "dracula",
        RGB(40, 42, 54), RGB(248, 248, 242), RGB(80, 250, 123), RGB(30, 32, 44),
        RGB(139, 233, 253), RGB(255, 85, 85), RGB(241, 250, 140), RGB(80, 250, 123),
        RGB(80, 250, 123), RGB(139, 233, 253), RGB(189, 147, 249), RGB(248, 248, 242),
        0.001f, 1.0f,
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
    ShaderIntensity = found->shader_intensity;
    GradientBrightness = found->gradient_brightness;
}
