#include "platform/draw.h"
#include "core/common_ui.h"
#include "config.h"
#include <windows.h>

void Platform_BeginListDraw(PromptState *state, void *ctx) { (void)state; (void)ctx; }
void Platform_EndListDraw(PromptState *state, void *ctx) { (void)state; (void)ctx; }

void Platform_DrawListBackground(PromptState *state, void *ctx, int y, int h, bool selected) {
    (void)state; (void)ctx; (void)y; (void)h; (void)selected;
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

void Platform_DrawListItem(PromptState *state, void *ctx, ListBoxInfo *lb, int idx, int y, int text_x, const char *icon_path) {
    (void)state; (void)ctx; (void)lb; (void)idx; (void)y; (void)text_x; (void)icon_path;
}

void Platform_BeginEditDraw(PromptState *state, void *ctx) { (void)state; (void)ctx; }
void Platform_EndEditDraw(PromptState *state, void *ctx) { (void)state; (void)ctx; }
void Platform_DrawEditBackground(PromptState *state, void *ctx) { (void)state; (void)ctx; }
void Platform_DrawEditText(PromptState *state, void *ctx) { (void)state; (void)ctx; }
void Platform_DrawEditCursor(PromptState *state, void *ctx) { (void)state; (void)ctx; }
void Platform_DrawSeparators(PromptState *state, void *ctx) { (void)state; (void)ctx; }
