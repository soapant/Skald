#include "platform/draw.h"
#include "config.h"
#include "core/common_ui.h"
#include "core/context.h"

static const char *g_prerender_text = NULL;
static void *g_prerender_rect = NULL;
void Platform_SetPreRenderStateWin32(const char *text, void *rect) { g_prerender_text = text; g_prerender_rect = rect; }

bool Platform_PreRender(void) {
    if (!g_prerender_text || !g_prerender_rect) return false;
    return true;
}

__attribute__((weak)) bool fl_prerender(void) {
    return Platform_PreRender();
}

void Platform_GetItemColors(int idx, int count, unsigned long *num_color, unsigned long *body_color, unsigned long *plain_color) {
    unsigned long col_text = COLOR_TEXT;
    unsigned long col_alt = COLOR_ALTERNATE;
    float t = (count > 1) ? (float)idx / (float)(count - 1) : (float)idx / (float)GradientItems;
    unsigned long col_rainbow = GetRainbowColor(t, 0.85f, GradientBrightness);
    unsigned long n = col_text, b = col_text, p = col_rainbow;
    if (RainbowText) {
        n = b = p = col_rainbow;
    } else if (AlternatingText && idx % 2 == 0) {
        n = b = p = col_alt;
    }
    if (num_color) *num_color = n;
    if (body_color) *body_color = b;
    if (plain_color) *plain_color = p;
}
