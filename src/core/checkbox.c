#include "core/checkbox.h"
#include "core/window_manager.h"
#include "core/timer.h"
#include "core/types.h"
#include "core/listbox_hover.h"
#include "helpers/helpers_ui.h"
#include "platform/draw.h"
#include "platform/input_state.h"
#include "platform/platform.h"
#include "deps/log.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

#define FL_CHECKBOX_MAX 1024
#define FL_CHECKBOX_BASE 16
#define CHECKBOX_GLYPH "\xe2\x9c\x93"

#define CHECKBOX_WHITE RGB(155, 155, 155)
#define CHECKBOX_GRAY RGB(68, 68, 68)
#define CHECKBOX_DARK_GRAY RGB(14, 14, 14)

static CheckboxSpec g_checkboxes[FL_CHECKBOX_MAX];
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
        int mx = 0, my = 0;
        if (Platform_GetMousePos(&mx, &my)) {
            int wx = 0, wy = 0;
            WindowManager* wm = WindowManager_Get();
            Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
            bool have_origin = false;
            if (top) have_origin = Platform_WindowToRoot(top->handle, &wx, &wy);
            if (!have_origin) {
                have_origin = Platform_WindowToRoot(FlintContext_Get()->window.hPopupWnd, &wx, &wy);
            }
            if (have_origin) {
                mx -= wx;
                my -= wy;
            }
            g_cb_mouse.mx = mx;
            g_cb_mouse.my = my;
        } else {
            g_cb_mouse.mx = -9999;
            g_cb_mouse.my = -9999;
        }
        g_cb_mouse.down = Platform_MouseStateDown(MOUSE_BTN_LEFT);
        g_cb_mouse.valid = true;
    }
    if (out_x) *out_x = g_cb_mouse.mx;
    if (out_y) *out_y = g_cb_mouse.my;
    if (out_down) *out_down = g_cb_mouse.down;
    PROFILE_END_VAR(_pt, "checkbox_mouse");
}

static CheckboxSpec* checkbox_find(const char* name) {
    PROFILE_START(_pt);
    for (int i = 0; i < g_checkbox_count; i++) {
        if (strcmp(g_checkboxes[i].name, name) == 0) { PROFILE_END_VAR(_pt, "checkbox_find"); return &g_checkboxes[i]; }
    }
    { PROFILE_END_VAR(_pt, "checkbox_find"); return NULL; }
}

static CheckboxSpec* checkbox_intern(const char* name, bool seed) {
    PROFILE_START(_pt);
    CheckboxSpec* cb = checkbox_find(name);
    if (cb) { PROFILE_END_VAR(_pt, "checkbox_intern"); return cb; }
    if (g_checkbox_count >= FL_CHECKBOX_MAX) { PROFILE_END_VAR(_pt, "checkbox_intern"); return NULL; }
    cb = &g_checkboxes[g_checkbox_count++];
    memset(cb, 0, sizeof(*cb));
    cb->name = name;
    cb->checked = seed;
    cb->thickness = 1;
    cb->border = true;
    cb->name = strdup(name);
    { PROFILE_END_VAR(_pt, "checkbox_intern"); return cb; }
}

bool Checkbox_IsChecked(const char* name) {
    PROFILE_START(_pt);
    CheckboxSpec* cb = name ? checkbox_find(name) : NULL;
    bool r = cb ? cb->checked : false;
    PROFILE_END_VAR(_pt, "Checkbox_IsChecked");
    return r;
}

bool Checkbox_IsHovered(const char* name) {
    PROFILE_START(_pt);
    CheckboxSpec* cb = name ? checkbox_find(name) : NULL;
    bool r = cb ? cb->hovered : false;
    PROFILE_END_VAR(_pt, "Checkbox_IsHovered");
    return r;
}

void Checkbox_ResetAll(void) {
    PROFILE_START(_pt);
    for (int i = 0; i < g_checkbox_count; i++) {
        g_checkboxes[i].checked = false;
        g_checkboxes[i].clicked = false;
        g_checkboxes[i].hovered = false;
        g_checkboxes[i].was_down = false;
    }
    PROFILE_END_VAR(_pt, "Checkbox_ResetAll");
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

FontHandle s_font;

void Checkbox_Draw(const CheckboxSpec* spec) {
    PROFILE_START(_pt);
    if (spec->render) { PlRect _r = { spec->x, spec->y, spec->w > 0 ? spec->w : (spec->button ? 100 : 16), spec->h > 0 ? spec->h : (spec->button ? 24 : 16) }; spec->render(_r); PROFILE_END_VAR(_pt, "Checkbox_Draw"); return; }
    unsigned long def_cb_border = spec->hovered ? CHECKBOX_WHITE : CHECKBOX_GRAY;
    unsigned long def_btn_border = (spec->checked || spec->clicked || spec->hovered) ? CHECKBOX_WHITE : CHECKBOX_GRAY;
    unsigned long def_cb_body = spec->hovered ? CHECKBOX_GRAY : COLOR_BG;
    unsigned long def_btn_body = (spec->checked || spec->clicked || spec->hovered) ? CHECKBOX_GRAY : COLOR_BG;

    unsigned long checkbox_border = spec->hovered_color ? spec->hovered_color : def_cb_border;
    unsigned long button_border = spec->clicked_color ? spec->clicked_color : def_btn_border;
    unsigned long checkbox_body = spec->hovered_color ? spec->hovered_color : def_cb_body;
    unsigned long button_body = spec->clicked_color ? spec->clicked_color : def_btn_body;

    int thick = spec->thickness > 0 ? spec->thickness : 1;

    if (spec->checkbox) {
        int w = spec->w > 0 ? spec->w : 16;
        int h = spec->h > 0 ? spec->h : 16;
        if (!spec->no_bg) {
            Platform_DrawRectRaw(spec->x, spec->y, w, h, checkbox_body, 0, false, true, thick, true);
            if (spec->border) {
                Platform_DrawRectRaw(spec->x, spec->y, w, h, checkbox_border, CornerRadius / 3, true, true, thick, false);
            }
        }

        if (spec->checked) {
            int fontsize = w * 3 / 4;
            if (fontsize < 1) fontsize = 1;
            int tw = Platform_MeasureTextWidth(CHECKBOX_GLYPH, (int)strlen(CHECKBOX_GLYPH), fontsize);
            int th = checkbox_line_height(fontsize);
            int tx = spec->x + (w - tw) / 2;
            int ty = spec->y + (h - th) / 2;
            Platform_DrawTextRaw(tx, ty - 2, CHECKBOX_GLYPH, rgb_to_hex(CHECKBOX_WHITE), FH_NULL, "LEFT", 0, fontsize);
        }
    } else if (spec->button) {
        int w = spec->w > 0 ? spec->w : 100;
        int h = spec->h > 0 ? spec->h : 24;
        if (!spec->no_bg) {
            Platform_DrawRectRaw(spec->x, spec->y, w, h, button_body, CornerRadius / 2, true, true, thick, true);
            if (spec->border) {
                Platform_DrawRectRaw(spec->x, spec->y, w, h, button_border, CornerRadius / 2, true, true, thick, false);
            }
        }
        if (spec->text) {
            int fontsize = spec->fontsize > 0 ? spec->fontsize : FontSize;
            if (!Platform_FontInitalized(s_font)) s_font = Platform_CreateFont(fontsize, UsedFont);
            Platform_DrawTextCentered(spec->x, spec->y, w, h, spec->text, spec->text_color, s_font, fontsize);
        }
    }

    PROFILE_END_VAR(_pt, "Checkbox_Draw");
}

bool Checkbox_Place(const CheckboxSpec* spec) {
    PROFILE_START(_pt);
    if (!spec || !spec->name || !spec->name[0]) {
        PROFILE_END_VAR(_pt, "Checkbox_Place");
        return false;
    }

    int w, h;
    int x = spec->x, y = spec->y;
    
    if (spec->button) {
        w = spec->w > 0 ? spec->w : 100;
        h = spec->h > 0 ? spec->h : 24;
    } else {
        w = spec->w > 0 ? spec->w : FL_CHECKBOX_BASE;
        h = spec->h > 0 ? spec->h : FL_CHECKBOX_BASE;
    }

    CheckboxSpec* cb = checkbox_intern(spec->name, spec->checked);
    if (!cb) { PROFILE_END_VAR(_pt, "Checkbox_Place"); return false; }
    cb->on_click = spec->on_click;
    cb->on_hover = spec->on_hover;
    cb->checkbox = spec->checkbox;
    cb->button = spec->button;
    cb->thickness = spec->thickness;
    cb->hovered_color = spec->hovered_color;
    cb->clicked_color = spec->clicked_color;
    cb->border = spec->border;
    cb->no_bg = spec->no_bg;
    cb->w = w;       
    cb->h = h;        
    cb->x = spec->x;
    cb->y = spec->y;
    cb->text = spec->text;
    cb->fontsize = spec->fontsize;
    cb->text_color = spec->text_color;
    cb->render = spec->render;

    int mx = 0, my = 0;
    bool down = false;
    checkbox_mouse(&mx, &my, &down);

    bool hovered = (mx >= x && mx < x + w && my >= y && my < y + h);
    bool hover_changed = (hovered != cb->hovered);
    cb->hovered = hovered;

    bool clicked = false;
    
    if (cb->button) {
        cb->clicked = false;
    }

    if (down && !cb->was_down) {
        if (hovered) {
            if (cb->button) cb->checked = !cb->checked;
            else cb->checked = true;
            cb->clicked = true;
            clicked = true;
        }
    }
    cb->was_down = down;

    Checkbox_Draw(cb);

    if (clicked && cb->on_click) cb->on_click();
    if (hover_changed && cb->on_hover) cb->on_hover();

    PROFILE_END_VAR(_pt, "Checkbox_Place");
    return true;
}
