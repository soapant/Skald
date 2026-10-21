#include "core/checkbox.h"
#include "core/timer.h"
#include "core/types.h"
#include "core/listbox_hover.h"
#include "helpers/helpers_ui.h"
#include "platform/draw.h"
#include "platform/input_state.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    char name[128];
    bool checked;
    bool hovered;
    bool was_down;
    void (*on_click)(const char *name, bool checked);
    void (*on_hover)(const char *name, bool hovered);
} FlCheckboxState;

#define FL_CHECKBOX_MAX 1024
#define FL_CHECKBOX_BASE 16
#define FL_CHECKBOX_GLYPH "\xe2\x9c\x93"

#define CHECKBOX_WHITE RGB(255, 255, 255)
#define CHECKBOX_GRAY RGB(128, 128, 128)
#define CHECKBOX_DARK_GRAY RGB(14, 14, 14)

static FlCheckboxState g_checkboxes[FL_CHECKBOX_MAX];
static int g_checkbox_count = 0;

static struct {
    bool valid;
    int mx, my;
    bool down;
} g_cb_mouse;

void Checkbox_FrameBegin(void) {
    PROFILE_START(_pt);
    g_cb_mouse.valid = false;
    PROFILE_END_VAR(_pt, "Checkbox_FrameBegin");
}

static void checkbox_mouse(int *out_x, int *out_y, bool *out_down) {
    PROFILE_START(_pt);
    if (!g_cb_mouse.valid) {
        bool have = ListBox_ClientMousePos(&g_cb_mouse.mx, &g_cb_mouse.my);
        if (!have) {
            g_cb_mouse.mx = 0;
            g_cb_mouse.my = 0;
        }
        g_cb_mouse.down = Platform_MouseStateDown(MOUSE_BTN_LEFT);
        g_cb_mouse.valid = true;
    }
    if (out_x) *out_x = g_cb_mouse.mx;
    if (out_y) *out_y = g_cb_mouse.my;
    if (out_down) *out_down = g_cb_mouse.down;
    PROFILE_END_VAR(_pt, "checkbox_mouse");
}

static FlCheckboxState* checkbox_find(const char* name) {
    PROFILE_START(_pt);
    for (int i = 0; i < g_checkbox_count; i++) {
        if (strcmp(g_checkboxes[i].name, name) == 0) { PROFILE_END_VAR(_pt, "checkbox_find"); return &g_checkboxes[i]; }
    }
    { PROFILE_END_VAR(_pt, "checkbox_find"); return NULL; }
}

static FlCheckboxState* checkbox_intern(const char* name, bool seed) {
    PROFILE_START(_pt);
    FlCheckboxState* cb = checkbox_find(name);
    if (cb) { PROFILE_END_VAR(_pt, "checkbox_intern"); return cb; }
    if (g_checkbox_count >= FL_CHECKBOX_MAX) { PROFILE_END_VAR(_pt, "checkbox_intern"); return NULL; }
    cb = &g_checkboxes[g_checkbox_count++];
    memset(cb, 0, sizeof(*cb));
    snprintf(cb->name, sizeof(cb->name), "%s", name);
    cb->checked = seed;
    { PROFILE_END_VAR(_pt, "checkbox_intern"); return cb; }
}

bool Checkbox_IsChecked(const char* name) {
    PROFILE_START(_pt);
    FlCheckboxState* cb = name ? checkbox_find(name) : NULL;
    bool r = cb ? cb->checked : false;
    PROFILE_END_VAR(_pt, "Checkbox_IsChecked");
    return r;
}

bool Checkbox_IsHovered(const char* name) {
    PROFILE_START(_pt);
    FlCheckboxState* cb = name ? checkbox_find(name) : NULL;
    bool r = cb ? cb->hovered : false;
    PROFILE_END_VAR(_pt, "Checkbox_IsHovered");
    return r;
}

static int checkbox_line_height(int fontsize) {
    PROFILE_START(_pt);
    int lh = 0, rh = 0, lt = 0;
    Platform_GetTextLayoutMetrics(&lh, &rh, &lt);
    int ref_h = lh > 0 ? lh : FontSize;
    int ref_fs = FontSize > 0 ? FontSize : fontsize;
    if (ref_h <= 0 || ref_fs <= 0) { PROFILE_END_VAR(_pt, "checkbox_line_height"); return fontsize; }
    { PROFILE_END_VAR(_pt, "checkbox_line_height"); return (int)((long)ref_h * fontsize / ref_fs); }
}

void Checkbox_Draw(int x, int y, int w, int h, bool checked, bool hovered) {
    PROFILE_START(_pt);
    if (w <= 0 || h <= 0) { PROFILE_END_VAR(_pt, "Checkbox_Draw"); return; }

    unsigned long border = hovered ? CHECKBOX_WHITE : CHECKBOX_GRAY;

    Platform_DrawRectRaw(x, y, w, h, CHECKBOX_DARK_GRAY, 0, false, true, 1, true);
    Platform_DrawRectRaw(x, y, w, h, border, 0, false, true, 2, false);

    if (checked) {
        int fontsize = w * 3 / 4;
        if (fontsize < 1) fontsize = 1;
        int tw = Platform_MeasureTextWidth(FL_CHECKBOX_GLYPH, (int)strlen(FL_CHECKBOX_GLYPH), fontsize);
        int th = checkbox_line_height(fontsize);
        int tx = x + (w - tw) / 2;
        int ty = y + (h - th) / 2;
        Platform_DrawTextRaw(tx, ty - 2, FL_CHECKBOX_GLYPH, rgb_to_hex(CHECKBOX_WHITE), FH_NULL, "LEFT", 0, fontsize);
    }

    PROFILE_END_VAR(_pt, "Checkbox_Draw");
}

bool Checkbox_Place(const FlCheckboxSpec* spec) {
    PROFILE_START(_pt);
    if (!spec || !spec->name || !spec->name[0]) {
        PROFILE_END_VAR(_pt, "Checkbox_Place");
        return false;
    }

    int x = spec->x;
    int y = spec->y;
    int scale = spec->scale > 0 ? spec->scale : 1;
    int side = FL_CHECKBOX_BASE * scale;

    FlCheckboxState* cb = checkbox_intern(spec->name, spec->checked);
    if (!cb) { PROFILE_END_VAR(_pt, "Checkbox_Place"); return false; }
    cb->on_click = spec->on_click;
    cb->on_hover = spec->on_hover;

    int mx = 0, my = 0;
    bool down = false;
    checkbox_mouse(&mx, &my, &down);

    bool hovered = (mx >= x && mx < x + side && my >= y && my < y + side);
    bool hover_changed = (hovered != cb->hovered);
    cb->hovered = hovered;

    bool clicked = false;
    if (down && !cb->was_down && hovered) {
        cb->checked = !cb->checked;
        clicked = true;
    }
    cb->was_down = down;

    Checkbox_Draw(x, y, side, side, cb->checked, cb->hovered);

    if (clicked && cb->on_click) cb->on_click(cb->name, cb->checked);
    if (hover_changed && cb->on_hover) cb->on_hover(cb->name, cb->hovered);

    PROFILE_END_VAR(_pt, "Checkbox_Place");
    return true;
}
