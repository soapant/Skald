#include "platform/draw.h"
#include "platform/ui.h"
#include "config.h"
#include "platform/x11/drawing_x11.h"
#include "platform/x11/display_x11.h"
#include "helpers/helpers_ui.h"
#include "core/timer.h"
#include "core/context.h"
#include "core/window_manager.h"
#include <X11/Xft/Xft.h>
#include <stdlib.h>

static PromptState *g_prerender_state = NULL;
void Platform_SetPreRenderState(PromptState *state) {
  g_prerender_state = state; 
}

bool Platform_PreRender(void) {
    PromptState *state = g_prerender_state;
    if (!state) return false;
    return true;
}
PromptState* Platform_GetPreRenderStatePtr(void) {
    return g_prerender_state;
}

__attribute__((weak)) bool fl_prerender(void) {
    return Platform_PreRender();
}

static Display* g_draw_dpy = NULL;
static XftDraw* g_draw_xd = NULL;
static Visual* g_draw_vis = NULL;
static Colormap g_draw_cmap = 0;
#define RAW_LINE_CAP 4096
typedef struct { char ch; unsigned long color; } RawCell;
static RawCell g_raw_cells[RAW_LINE_CAP];
static char g_raw_text[RAW_LINE_CAP + 1];
static int g_raw_map_x[RAW_LINE_CAP];
static int g_raw_map_top[RAW_LINE_CAP];
static int g_raw_count = 0;
static int g_raw_y = 0;
static int g_raw_x0 = 0;
static int g_raw_pitch = 0;
static int g_raw_fontsize = 0;
static FontHandle g_raw_font;
static bool g_raw_font_owned = false;
static Display* g_raw_dpy = NULL;
static XftDraw* g_raw_xd = NULL;
static Visual* g_raw_vis = NULL;
static Colormap g_raw_cmap = 0;
static int g_raw_map_count = 0;
static int g_raw_map_y = 0;
static int g_raw_map_x0 = 0;
static int g_raw_map_pitch = 0;
static XftDraw* g_raw_map_xd = NULL;
static int RawMeasure(Display* dpy, XftFont* xf, const char* s, int n) {
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, xf, (FcChar8*)s, n, &ext);
    return ext.xOff;
}
static void RawEmitLine(Display* dpy, XftDraw* xd, XftFont* xf, XftColor* xcols, unsigned long* ucols, int nu, int ls, int le, int drawX, int baseline, int rowTop) {
    int cx = drawX;
    int i = ls;
    while (i < le) {
        int j = i + 1;
        while (j < le && g_raw_cells[j].color == g_raw_cells[i].color) j++;
        int w = RawMeasure(dpy, xf, g_raw_text + i, j - i);
        int k = 0;
        while (k < nu && ucols[k] != g_raw_cells[i].color) k++;
        if (k >= nu) k = 0;
        XftDrawStringUtf8(xd, &xcols[k], xf, cx, baseline, (FcChar8*)(g_raw_text + i), j - i);
        for (int m = i; m < j; m++) { g_raw_map_x[m] = cx; g_raw_map_top[m] = rowTop; }
        cx += w;
        i = j;
    }
}
static void RawLine_Flush(void) {
    int count = g_raw_count;
    g_raw_count = 0;
    if (count <= 0) return;
    Display* dpy = g_raw_dpy;
    XftDraw* xd = g_raw_xd;
    Visual* vis = g_raw_vis;
    Colormap cmap = g_raw_cmap;
    XftFont* xf = FH_TO_XFT(g_raw_font);
    int x0 = g_raw_x0;
    int y = g_raw_y;
    int pitch = g_raw_pitch;
    bool owned = g_raw_font_owned;
    g_raw_font_owned = false;
    if (!dpy || !xd || !xf) {
        if (owned) Platform_DestroyFont(g_raw_font);
        return;
    }
    for (int i = 0; i < count; i++) g_raw_text[i] = g_raw_cells[i].ch;
    g_raw_text[count] = '\0';
    int avail = 900 - x0 - 10;
    if (avail < 40) avail = 40;
    int ascent = xf->ascent;
    int lineH = ascent + xf->descent;
    int spaceW = RawMeasure(dpy, xf, " ", 1);
    unsigned long ucols[64];
    int nu = 0;
    for (int i = 0; i < count && nu < 64; i++) {
        int k = 0;
        while (k < nu && ucols[k] != g_raw_cells[i].color) k++;
        if (k >= nu) ucols[nu++] = g_raw_cells[i].color;
    }
    XftColor xcols[64];
    for (int k = 0; k < nu; k++) Linux_XftAllocColor(dpy, vis, cmap, ucols[k], 0xFFFF, &xcols[k]);
    int row = 0;
    int p = 0;
    int ls = 0;
    int le = 0;
    int lw = 0;
    while (p < count) {
        int ws = p;
        while (ws < count && g_raw_text[ws] == ' ') ws++;
        if (ws >= count) {
            for (int m = p; m < count; m++) { g_raw_map_x[m] = x0 + lw; g_raw_map_top[m] = y + row * lineH; }
            break;
        }
        for (int m = p; m < ws; m++) { g_raw_map_x[m] = x0; g_raw_map_top[m] = y + row * lineH; }
        int we = ws;
        while (we < count && g_raw_text[we] != ' ') we++;
        int ww = RawMeasure(dpy, xf, g_raw_text + ws, we - ws);
        if (ww > avail) {
            if (le > ls) {
                RawEmitLine(dpy, xd, xf, xcols, ucols, nu, ls, le, x0, y + ascent + row * lineH, y + row * lineH);
                row++;
            }
            int c = ws;
            while (c < we) {
                int lo = 0, hi = we - c;
                while (lo < hi) {
                    int mid = (lo + hi + 1) / 2;
                    if (RawMeasure(dpy, xf, g_raw_text + c, mid) <= avail) lo = mid;
                    else hi = mid - 1;
                }
                int n = lo;
                while (n > 0 && (((unsigned char)g_raw_text[c + n] & 0xC0) == 0x80)) n--;
                if (n <= 0) {
                    unsigned char lc = (unsigned char)g_raw_text[c];
                    if ((lc & 0x80) == 0) n = 1;
                    else if ((lc & 0xE0) == 0xC0) n = 2;
                    else if ((lc & 0xF0) == 0xE0) n = 3;
                    else if ((lc & 0xF8) == 0xF0) n = 4;
                    else n = 1;
                    if (n > we - c) n = we - c;
                }
                if (c + n >= we) {
                    ls = c; le = we; lw = RawMeasure(dpy, xf, g_raw_text + c, we - c);
                    c = we;
                    break;
                }
                RawEmitLine(dpy, xd, xf, xcols, ucols, nu, c, c + n, x0, y + ascent + row * lineH, y + row * lineH);
                row++;
                c += n;
            }
            p = we;
            continue;
        }
        int add = ww + (le > ls ? spaceW : 0);
        if (le > ls && lw + add > avail) {
            RawEmitLine(dpy, xd, xf, xcols, ucols, nu, ls, le, x0, y + ascent + row * lineH, y + row * lineH);
            row++;
            ls = ws; le = we; lw = ww;
        } else {
            if (le <= ls) ls = ws;
            le = we; lw += add;
        }
        p = we;
    }
    if (le > ls) {
        RawEmitLine(dpy, xd, xf, xcols, ucols, nu, ls, le, x0, y + ascent + row * lineH, y + row * lineH);
    }
    g_raw_map_count = count;
    g_raw_map_y = y;
    g_raw_map_x0 = x0;
    g_raw_map_pitch = pitch;
    g_raw_map_xd = xd;
    for (int k = 0; k < nu; k++) XftColorFree(dpy, vis, cmap, &xcols[k]);
    if (owned) Platform_DestroyFont(g_raw_font);
}
static bool RawLine_Push(int x, int y, char ch, unsigned long color, FontHandle fh, bool owned, int fontsize) {
    if (g_raw_count > 0) {
        bool same = (y == g_raw_y) && (fontsize == g_raw_fontsize) && (g_raw_xd == g_draw_xd);
        if (same && g_raw_count >= 2) same = (x == g_raw_x0 + g_raw_count * g_raw_pitch);
        if (same && g_raw_count == 1) {
            int p = x - g_raw_x0;
            if (p <= 0) same = false;
            else g_raw_pitch = p;
        }
        if (same && owned && !g_raw_font_owned) same = false;
        if (same && !owned) same = (FH_TO_XFT(fh) == FH_TO_XFT(g_raw_font));
        if (!same) RawLine_Flush();
    }
    if (g_raw_count >= RAW_LINE_CAP) return false;
    if (g_raw_count == 0) {
        g_raw_x0 = x; g_raw_y = y; g_raw_pitch = 0; g_raw_fontsize = fontsize;
        g_raw_font = fh; g_raw_font_owned = owned;
        g_raw_dpy = g_draw_dpy; g_raw_xd = g_draw_xd;
        g_raw_vis = g_draw_vis; g_raw_cmap = g_draw_cmap;
    } else if (owned) {
        Platform_DestroyFont(fh);
    }
    g_raw_cells[g_raw_count].ch = ch;
    g_raw_cells[g_raw_count].color = color;
    g_raw_count++;
    return true;
}
static bool RawRect_Map(int x, int y, int w, int h, unsigned long color) {
    RawLine_Flush();
    if (g_raw_map_count <= 0 || g_raw_map_pitch <= 0) return false;
    if (y != g_raw_map_y || g_raw_map_xd != g_draw_xd || !g_draw_xd) return false;
    if (x < g_raw_map_x0 || x >= g_raw_map_x0 + g_raw_map_count * g_raw_map_pitch) return false;
    int idx = (x - g_raw_map_x0 + g_raw_map_pitch / 2) / g_raw_map_pitch;
    if (idx < 0) idx = 0;
    if (idx >= g_raw_map_count) idx = g_raw_map_count - 1;
    XftColor rc;
    Linux_XftAllocColor(g_draw_dpy, g_draw_vis, g_draw_cmap, color, 0xFFFF, &rc);
    XftDrawRect(g_draw_xd, &rc, g_raw_map_x[idx] + (x - (g_raw_map_x0 + idx * g_raw_map_pitch)), g_raw_map_top[idx] + (y - g_raw_map_y), w, h);
    XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &rc);
    return true;
}
void Platform_SetDrawContext(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap) {
    RawLine_Flush();
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
        int len = (int)strlen(text);
        if (len == 1 && xf && RawLine_Push(x, y, text[0], color, fh, needDestroy, fontsize)) {
            needDestroy = false;
        } else {
            RawLine_Flush();
            if (xf) XftDrawStringUtf8(g_draw_xd, &col, xf, x, y + (xf->ascent), (FcChar8*)text, len);
        }
    }
    XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &col);
    if (needDestroy) Platform_DestroyFont(fh);
    return true;
}

static Display* g_line_dpy = NULL;
static int g_line_size = 0;
static XftFont* g_line_font = NULL;
static XftFont* LineFont(Display* dpy, int size) {
    if (size <= 0) size = FontSize;
    if (!g_line_font || g_line_dpy != dpy || g_line_size != size) {
        if (g_line_font && g_line_dpy) XftFontClose(g_line_dpy, g_line_font);
        g_line_font = NULL;
        g_line_dpy = NULL;
        g_line_size = 0;
        if (!dpy) return NULL;
        int screen = DefaultScreen(dpy);
        XftFont* xf = XftFontOpen(dpy, screen, FC_FAMILY, FcTypeString, UsedFont, FC_PIXEL_SIZE, FcTypeDouble, (double)size, NULL);
        if (!xf) xf = XftFontOpen(dpy, screen, FC_FAMILY, FcTypeString, "sans", FC_PIXEL_SIZE, FcTypeDouble, (double)size, NULL);
        if (!xf) xf = XftFontOpenName(dpy, screen, "fixed");
        if (!xf) return NULL;
        g_line_font = xf;
        g_line_dpy = dpy;
        g_line_size = size;
    }
    return g_line_font;
}

int Platform_MeasureTextWidth(const char* text, int len, int fontsize) {
    if (!text || len <= 0) return 0;
    if (!g_draw_dpy) return len * (fontsize > 0 ? fontsize : FontSize) / 2;
    XftFont* xf = LineFont(g_draw_dpy, fontsize);
    if (!xf) return len * (fontsize > 0 ? fontsize : FontSize) / 2;
    XGlyphInfo ext;
    XftTextExtentsUtf8(g_draw_dpy, xf, (FcChar8*)text, len, &ext);
    return ext.xOff;
}

int Platform_MeasureLineHeight(int fontsize) {
    if (fontsize <= 0) fontsize = FontSize;
    if (g_draw_dpy) {
        XftFont* xf = LineFont(g_draw_dpy, fontsize);
        if (xf) return xf->ascent + xf->descent;
    }
    return fontsize + fontsize / 3;
}

bool Platform_DrawTextLine(int x, int y, const char* text, int len, unsigned long color, int fontsize) {
    if (!text || len <= 0 || !g_draw_dpy || !g_draw_xd) return false;
    XftFont* xf = LineFont(g_draw_dpy, fontsize);
    if (!xf) return false;
    if (color == 0) color = rgb_to_hex(COLOR_TEXT);
    XftColor col;
    Linux_XftAllocColor(g_draw_dpy, g_draw_vis, g_draw_cmap, color, 0xFFFF, &col);
    XftDrawStringUtf8(g_draw_xd, &col, xf, x, y + xf->ascent, (FcChar8*)text, len);
    XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &col);
    return true;
}

void Platform_DrawListBackground(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, int y, int h, bool selected) {
    if (selected) {
        Linux_DrawSelectionBackground(dpy, xd, vis, cmap, 0, y, state->width, h, rgb_to_hex(COLOR_SELECTED_TEXT));
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
    int cursor_x = 0;
    int text_len = strlen(state->filter);
    int scroll = Linux_UpdateEditScrollOffset(dpy, xf, state->filter, text_len, state->cursor, state->width, state->scroll_offset, &cursor_x);
    state->scroll_offset = scroll;
    int draw_x = 14 - scroll;
    if (text_len > 0) {
        Platform_SetDrawContext(dpy, xd, vis, cmap);
        Platform_DrawTextRaw(draw_x, 20, state->filter, rgb_to_hex(COLOR_TEXT), font, "", 0, 0);
    }
}
void Platform_DrawEditCursor(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font) {
    XftFont *xf = FH_TO_XFT(font);
    int cursor_x = 0;
    Linux_UpdateEditScrollOffset(dpy, xf, state->filter, strlen(state->filter), state->cursor, state->width, state->scroll_offset, &cursor_x);
    Platform_SetDrawContext(dpy, xd, vis, cmap);
    int cursor_draw_x = 14 + cursor_x - state->scroll_offset;
    if (cursor_draw_x < 2) cursor_draw_x = 2;
    if (cursor_draw_x > state->width - 4) cursor_draw_x = state->width - 4;
    int textH = xf ? xf->ascent + xf->descent : FontSize;
    int cursor_y = 20 + (textH - FontSize)/2;
    if (cursor_y < 20) cursor_y = 20;
    if (IsInsertMode()) {
        Platform_DrawRectRaw(cursor_draw_x, cursor_y, 2, FontSize, rgb_to_hex(COLOR_TEXT));
    } else {
        Platform_DrawRectRaw(cursor_draw_x, cursor_y, 12, FontSize, rgb_to_hex(COLOR_TEXT));
    }
}
bool Platform_DrawRectRaw(int x, int y, int w, int h, unsigned long color) {
    if (RawRect_Map(x, y, w, h, color)) return true;
    XftColor col;
    Linux_XftAllocColor(g_draw_dpy, g_draw_vis, g_draw_cmap, color, OpacityAlpha16(), &col);
    XftDrawRect(g_draw_xd, &col, x, y, w, h);
    XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &col);
    return true;
}
