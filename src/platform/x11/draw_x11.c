#include "platform/draw.h"
#include "core/common_ui.h"
#include "config.h"
#include "platform/x11/drawing.h"
#include "platform/x11/display.h"
#include "helpers/ui.h"
#include "core/timer.h"
#include "core/context.h"
#include "core/window_manager.h"
#include <X11/Xft/Xft.h>
#include <stdlib.h>

static PromptState *g_prerender_state = NULL;
void Platform_SetPreRenderState(PromptState *state) { g_prerender_state = state; }

bool Platform_PreRender(void) {
    PromptState *state = g_prerender_state;
    if (!state) return false;
    return true;
}

__attribute__((weak)) bool fl_prerender(void) {
    return Platform_PreRender();
}

static Display* g_draw_dpy = NULL;
static XftDraw* g_draw_xd = NULL;
static Visual* g_draw_vis = NULL;
static Colormap g_draw_cmap = 0;
void Platform_SetDrawContext(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap) {
    g_draw_dpy = dpy; g_draw_xd = xd; g_draw_vis = vis; g_draw_cmap = cmap;
}
void Platform_GetDrawContext(Display** dpy, XftDraw** xd, Visual** vis, Colormap* cmap) {
    if (dpy) *dpy = g_draw_dpy;
    if (xd) *xd = g_draw_xd;
    if (vis) *vis = g_draw_vis;
    if (cmap) *cmap = g_draw_cmap;
}
bool Platform_DrawTextRaw(int x, int y, const char *text, unsigned long color, FontHandle font, const char *align, int pad,int fontsize) {
    if (!text || !g_draw_dpy || !g_draw_xd) return false;
    FontHandle fh = font;
    bool needDestroy = false;
    if (!FH_TO_XFT(fh)) {
        if (fontsize) {
          fh = Platform_CreateFontOnDisplay(g_draw_dpy, fontsize, UsedFont);
        } else {
        fh = Platform_CreateFontOnDisplay(g_draw_dpy, FontSize, UsedFont);
        }
        needDestroy = true;
    }
    if (color == 0) color = rgb_to_hex(COLOR_TEXT);
    const char *a = align ? align : "CENTER";
    XftColor col;
    Linux_XftAllocColor(g_draw_dpy, g_draw_vis, g_draw_cmap, color, 0xFFFF, &col);
    if (strcmp(a, "CENTER")==0 || strcmp(a, "LEFT")==0 || strcmp(a, "RIGHT")==0) {
        draw_wrapped_text(g_draw_dpy, g_draw_xd, &col, fh, text, y, (char*)a, pad ? pad : x);
    } else {
        XftFont *xf = FH_TO_XFT(fh);
        if (xf) XftDrawStringUtf8(g_draw_xd, &col, xf, x, y + (xf->ascent), (FcChar8*)text, strlen(text));
    }
    XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &col);
    if (needDestroy) Platform_DestroyFont(fh);
    return true;
}

void Platform_DrawListBackground(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, int y, int h, bool selected) {
    if (selected) {
        Linux_DrawSelectionBackground(dpy, xd, vis, cmap, 0, y, state->width, h, rgb_to_hex(COLOR_SELECTED_TEXT));
    } else {
        if (!EnableShaders || ShaderPreset == 0) {
            XftColor bgc;
            Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_BG), OpacityAlpha16(), &bgc);
            XftDrawRect(xd, &bgc, 0, y, state->width, h);
            XftColorFree(dpy, vis, cmap, &bgc);
        }
    }
}

void Platform_GetItemColors(int idx, int count, unsigned long *num_color, unsigned long *body_color, unsigned long *plain_color) {
    unsigned long col_text = rgb_to_hex(COLOR_TEXT);
    unsigned long col_alt = rgb_to_hex(COLOR_ALTERNATE);
    float t = (count > 1) ? (float)idx / (float)(count - 1) : (float)idx / (float)GradientItems;
    unsigned long col_rainbow = rgb_to_hex(GetRainbowColor(t, 0.55f, GradientBrightness));
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

void Platform_DrawListItem(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font, ListBoxInfo *lb, int idx, int y, int text_x, const char *icon_path) {
    (void)state;
    if (!lb) return;
    unsigned long ncol, bcol, pcol;
    Platform_GetItemColors(idx, lb->populated_count, &ncol, &bcol, &pcol);
    XftColor ncolor, bcolor, pcolor;
    Linux_XftAllocColor(dpy, vis, cmap, ncol, 0xFFFF, &ncolor);
    Linux_XftAllocColor(dpy, vis, cmap, bcol, 0xFFFF, &bcolor);
    Linux_XftAllocColor(dpy, vis, cmap, pcol, 0xFFFF, &pcolor);
    DrawListBoxItemText(dpy, xd, vis, cmap, font, &ncolor, &bcolor, &pcolor, lb,(char*)icon_path, idx, y, text_x);
    XftColorFree(dpy, vis, cmap, &ncolor);
    XftColorFree(dpy, vis, cmap, &bcolor);
    XftColorFree(dpy, vis, cmap, &pcolor);
}

void Platform_DrawEditBackground(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font) {
    unsigned long window_title_text_color_map;
    eval_window_title_color(state,&window_title_text_color_map);
    Platform_SetDrawContext(dpy, xd, vis, cmap);
    if (WindowTitleText) Platform_DrawTextRaw(5, state->height - 40, state->window_title, window_title_text_color_map, font, "LEFT", 5,FontSize);
    if (VimModeText) Platform_DrawTextRaw(5, state->height - 40, g_vim_mode, VimModeTextColor(), font, "RIGHT", 5,FontSize);
}
void Platform_DrawEditText(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font) {
    XftFont *xf = FH_TO_XFT(font);
    XftColor text_color;
    Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_TEXT), 0xFFFF, &text_color);
    int cursor_x = 0;
    int text_len = strlen(state->filter);
    int scroll = Linux_UpdateEditScrollOffset(dpy, xf, state->filter, text_len, state->cursor, state->width, state->scroll_offset, &cursor_x);
    state->scroll_offset = scroll;
    int draw_x = 14 - scroll;
    int draw_y = 20 + (xf ? xf->ascent : FontSize);
    if (xf && text_len > 0) {
        XftDrawStringUtf8(xd, &text_color, xf, draw_x, draw_y, (FcChar8*)state->filter, text_len);
    }
    XftColorFree(dpy, vis, cmap, &text_color);
}
void Platform_DrawEditCursor(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font) {
    XftFont *xf = FH_TO_XFT(font);
    XftColor text_color;
    Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_TEXT), 0xFFFF, &text_color);
    int cursor_x = 0;
    Linux_UpdateEditScrollOffset(dpy, xf, state->filter, strlen(state->filter), state->cursor, state->width, state->scroll_offset, &cursor_x);
    Linux_DrawEditCursor(xd, &text_color, cursor_x, state->scroll_offset, state->width);
    XftColorFree(dpy, vis, cmap, &text_color);
}
