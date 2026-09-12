#include "platform/draw.h"
#include "core/common_ui.h"
#include "config.h"
#include "platform/x11/drawing.h"
#include "platform/x11/display.h"
#include "helpers/ui.h"
#include "core/timer.h"
#include <X11/Xft/Xft.h>
#include <stdlib.h>

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
    DrawListBoxItemText(dpy, xd, vis, cmap, font, &ncolor, &bcolor, &pcolor, lb, (char*)icon_path, idx, y, text_x);
    XftColorFree(dpy, vis, cmap, &ncolor);
    XftColorFree(dpy, vis, cmap, &bcolor);
    XftColorFree(dpy, vis, cmap, &pcolor);
}

void Platform_DrawEditBackground(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font) {
    unsigned long window_title_text_color_map;
    eval_window_title_color(state,&window_title_text_color_map);
    XftColor text_color, window_title_text_color, vim_text_color;
    Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_TEXT), 0xFFFF, &text_color);
    Linux_XftAllocColor(dpy, vis, cmap, window_title_text_color_map, 0xFFFF, &window_title_text_color);
    Linux_XftAllocColor(dpy, vis, cmap, VimModeTextColor(), 0xFFFF, &vim_text_color);
    draw_wrapped_text(dpy, xd, &window_title_text_color, font,state->window_title,state->height - 40,"LEFT",5);
    draw_wrapped_text(dpy, xd, &vim_text_color,font,g_vim_mode,state->height - 40,"RIGHT",5);
    XftColorFree(dpy, vis, cmap, &text_color);
    XftColorFree(dpy, vis, cmap, &vim_text_color);
    XftColorFree(dpy, vis, cmap, &window_title_text_color);
}
void Platform_DrawEditText(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font) {
    XftFont *xf = FH_TO_XFT(font);
    XftColor text_color, seperator_color;
    Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_TEXT), 0xFFFF, &text_color);
    Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_SEPERATOR), 0xFFFF, &seperator_color);
    int cursor_x = 0;
    int text_len = strlen(state->filter);
    int scroll = Linux_UpdateEditScrollOffset(dpy, xf, state->filter, text_len, state->cursor, state->width, state->scroll_offset, &cursor_x);
    state->scroll_offset = scroll;
    int draw_x = 20 - scroll;
    int draw_y = 20 + (xf ? xf->ascent : FontSize);
    if (xf && text_len > 0) {
        XftDrawStringUtf8(xd, &text_color, xf, draw_x, draw_y, (FcChar8*)state->filter, text_len);
    }
    XftColorFree(dpy, vis, cmap, &text_color);
    XftColorFree(dpy, vis, cmap, &seperator_color);
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
void Platform_DrawSeparators(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap) {
    (void)state;
    XftColor seperator_color;
    Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_SEPERATOR), 0xFFFF, &seperator_color);
    XftDrawRect(xd, &seperator_color,0,60,state->width,5);
    XftDrawRect(xd, &seperator_color,0,state->height - 60,state->width,5);
    XftColorFree(dpy, vis, cmap, &seperator_color);
}
