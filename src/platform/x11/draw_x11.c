#include "platform/draw.h"
#include "platform/platform.h"
#include "platform/ui.h"
#include "config.h"
#include "core/prompt_geometry.h"
#include "helpers/helpers_ui.h"
#include "core/timer.h"
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "platform/x11/drawing_x11.h"
#include <stdlib.h>
#include "core/modal.h"
#include "bindings/vim_state.h"

static __thread PromptState *g_prerender_state = NULL;

static __thread unsigned long g_draw_bg_override = 0;
static __thread bool g_transparent_window = false;

void Platform_SetTransparentWindow(bool on) {
        PROFILE_START(_pt);
g_transparent_window = on;
}
bool Platform_GetTransparentWindow(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_GetTransparentWindow"); return g_transparent_window; }
}

void Platform_SetDrawBgColor(unsigned long hex) {
        PROFILE_START(_pt);
g_draw_bg_override = hex;
}
unsigned long Platform_GetDrawBgColor(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_GetDrawBgColor"); return g_draw_bg_override; }
}

static __thread bool g_measuring = false;
static __thread int g_meas_x0 = 0, g_meas_y0 = 0, g_meas_x1 = 0, g_meas_y1 = 0;
static __thread int g_meas_origin_x = 0, g_meas_origin_y = 0;

static void measure_reset(void) {
        PROFILE_START(_pt);
g_measuring = false;
    g_meas_x0 = 0; g_meas_y0 = 0; g_meas_x1 = 0; g_meas_y1 = 0;
}
void Platform_BeginMeasure(void) {
        PROFILE_START(_pt);
g_measuring = true;
    g_meas_x0 = 1 << 30; g_meas_y0 = 1 << 30;
    g_meas_x1 = -(1 << 30); g_meas_y1 = -(1 << 30);
}
void Platform_EndMeasure(int *out_w, int *out_h) {
        PROFILE_START(_pt);
int rw = 0, rh = 0;
    if (g_meas_x1 > g_meas_x0) rw = g_meas_x1 - g_meas_x0;
    if (g_meas_y1 > g_meas_y0) rh = g_meas_y1 - g_meas_y0;
    if (out_w) *out_w = rw;
    if (out_h) *out_h = rh;
    g_meas_origin_x = (rw > 0 || rh > 0) ? g_meas_x0 : 0;
    g_meas_origin_y = (rw > 0 || rh > 0) ? g_meas_y0 : 0;
    measure_reset();
}
void Platform_GetLastMeasureOrigin(int *out_x, int *out_y) {
        PROFILE_START(_pt);
if (out_x) *out_x = g_meas_origin_x;
    if (out_y) *out_y = g_meas_origin_y;
}
static __thread int g_draw_off_x = 0, g_draw_off_y = 0;
void Platform_SetDrawOffset(int dx, int dy) {
        PROFILE_START(_pt);
g_draw_off_x = dx;
    g_draw_off_y = dy;
}
void Platform_GetDrawOffset(int *out_x, int *out_y) {
    if (out_x) *out_x = g_draw_off_x;
    if (out_y) *out_y = g_draw_off_y;
}
bool Platform_WindowToRoot(WindowHandle h, int *out_x, int *out_y) {
    Display* dpy = get_display();
    if (!dpy) return false;
    Window w = (Window)WH_TO_HWND(h);
    if (w == None) return false;
    Window child = None;
    int ax = 0, ay = 0;
    if (!XTranslateCoordinates(dpy, w, DefaultRootWindow(dpy), 0, 0, &ax, &ay, &child)) return false;
    if (out_x) *out_x = ax;
    if (out_y) *out_y = ay;
    return true;
}
static void measure_track(int x, int y, int w, int h) {
        PROFILE_START(_pt);
if (!g_measuring) { PROFILE_END_VAR(_pt, "measure_track"); return ; }
    if (x < g_meas_x0) g_meas_x0 = x;
    if (y < g_meas_y0) g_meas_y0 = y;
    if (x + w > g_meas_x1) g_meas_x1 = x + w;
    if (y + h > g_meas_y1) g_meas_y1 = y + h;
}

void Platform_SetPreRenderState(PromptState *state) {
    PROFILE_START(_pt);
g_prerender_state = state; 
}

bool Platform_PreRender(void) {
        PROFILE_START(_pt);
PromptState *state = g_prerender_state;
    if (!state) { PROFILE_END_VAR(_pt, "Platform_PreRender"); return false; }
    { PROFILE_END_VAR(_pt, "Platform_PreRender"); return true; }
}
PromptState* Platform_GetPreRenderStatePtr(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_GetPreRenderStatePtr"); return g_prerender_state; }
}

__attribute__((weak)) bool fl_prerender(void) {
    return Platform_PreRender();
}

static __thread Display* g_draw_dpy = NULL;
static __thread XftDraw* g_draw_xd = NULL;
static __thread Visual* g_draw_vis = NULL;
static __thread Colormap g_draw_cmap = 0;
#define RAW_LINE_CAP 4096
typedef struct { char ch; unsigned long color; } RawCell;
static __thread RawCell g_raw_cells[RAW_LINE_CAP];
static __thread char g_raw_text[RAW_LINE_CAP + 1];
static __thread int g_raw_map_x[RAW_LINE_CAP];
static __thread int g_raw_map_top[RAW_LINE_CAP];
static __thread int g_raw_count = 0;
static __thread int g_raw_y = 0;
static __thread int g_raw_x0 = 0;
static __thread int g_raw_pitch = 0;
static __thread int g_raw_fontsize = 0;
static __thread FontHandle g_raw_font;
static __thread bool g_raw_font_owned = false;
static __thread Display* g_raw_dpy = NULL;
static __thread XftDraw* g_raw_xd = NULL;
static __thread Visual* g_raw_vis = NULL;
static __thread Colormap g_raw_cmap = 0;
static __thread int g_raw_map_count = 0;
static __thread int g_raw_map_y = 0;
static __thread int g_raw_map_x0 = 0;
static __thread int g_raw_map_pitch = 0;
static __thread XftDraw* g_raw_map_xd = NULL;
static int RawMeasure(Display* dpy, XftFont* xf, const char* s, int n) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "RawMeasure"); return x11_measure_utf8_fallback(dpy, xf, s, n, 0); }
}
static void RawEmitLine(Display* dpy, XftDraw* xd, XftFont* xf, XftColor* xcols, unsigned long* ucols, int nu, int ls, int le, int drawX, int baseline, int rowTop) {
        PROFILE_START(_pt);
int cx = drawX;
    int i = ls;
    while (i < le) {
        int j = i + 1;
        while (j < le && g_raw_cells[j].color == g_raw_cells[i].color) j++;
        int w = RawMeasure(dpy, xf, g_raw_text + i, j - i);
        int k = 0;
        while (k < nu && ucols[k] != g_raw_cells[i].color) k++;
        if (k >= nu) k = 0;
        x11_draw_utf8_fallback(dpy, xd, &xcols[k], cx, baseline, xf, g_raw_text + i, j - i, 0);
        for (int m = i; m < j; m++) { g_raw_map_x[m] = cx; g_raw_map_top[m] = rowTop; }
        cx += w;
        i = j;
    }
}
static void RawLine_Flush(void) {
        PROFILE_START(_pt);
int count = g_raw_count;
    g_raw_count = 0;
    if (count <= 0) { PROFILE_END_VAR(_pt, "RawLine_Flush"); return ; }
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
    for (int k = 0; k < nu; k++) x11_xft_alloc_color(dpy, vis, cmap, ucols[k], 0xFFFF, &xcols[k]);
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
        PROFILE_START(_pt);
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
    if (g_raw_count >= RAW_LINE_CAP) { PROFILE_END_VAR(_pt, "RawLine_Push"); return false; }
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
    { PROFILE_END_VAR(_pt, "RawLine_Push"); return true; }
}
void Platform_SetDrawContext(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap) {
        PROFILE_START(_pt);
RawLine_Flush();
    g_draw_dpy = dpy; g_draw_xd = xd; g_draw_vis = vis; g_draw_cmap = cmap;
}
void Platform_FlushDraw(void) {
        PROFILE_START(_pt);
RawLine_Flush();
}
bool Platform_DrawImageAt(const char *path, int x, int y, int w, int h) {
        PROFILE_START(_pt);
if (!path || !path[0] || !g_draw_dpy || !g_draw_xd) { PROFILE_END_VAR(_pt, "Platform_DrawImageAt"); return false; }
    x += g_draw_off_x;
    y += g_draw_off_y;
    if (w <= 0) w = IconSize;
    if (h <= 0) h = IconSize;
    measure_track(x, y, w, h);
    { PROFILE_END_VAR(_pt, "Platform_DrawImageAt"); return Platform_DrawImageScaled(g_draw_dpy, g_draw_xd, g_draw_vis, path, x, y, w, h); }
}
void Platform_GetDrawContext(Display** dpy, XftDraw** xd, Visual** vis, Colormap* cmap) {
        PROFILE_START(_pt);
if (dpy) *dpy = g_draw_dpy;
    if (xd) *xd = g_draw_xd;
    if (vis) *vis = g_draw_vis;
    if (cmap) *cmap = g_draw_cmap;
}
bool Platform_DrawTextRaw(int x, int y, const char *text, unsigned long color, FontHandle font, const char *align, int pad,int fontsize) {
        PROFILE_START(_pt);
if (!text || !g_draw_dpy || !g_draw_xd) { PROFILE_END_VAR(_pt, "Platform_DrawTextRaw"); return false; }
    x += g_draw_off_x;
    y += g_draw_off_y;
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
    if (g_measuring) {
        XftFont *mf = FH_TO_XFT(fh);
        int tw = 0;
        if (mf) {
            tw = x11_measure_utf8_fallback(g_draw_dpy, mf, text, (int)strlen(text), fontsize > 0 ? fontsize : FontSize);
        }
        int th = mf ? (mf->ascent + mf->descent) : (fontsize > 0 ? fontsize : FontSize);
        int bx = x;
        if (Platform_StringContainsCaseInsensitive(a, "CENTER")) bx = x - tw / 2;
        else if (Platform_StringContainsCaseInsensitive(a, "RIGHT")) bx = x - tw;
        measure_track(bx, y, tw, th);
    }
    XftColor col;
    x11_xft_alloc_color(g_draw_dpy, g_draw_vis, g_draw_cmap, color, 0xFFFF, &col);
    if (Platform_StringContainsCaseInsensitive(a, "CENTER") || Platform_StringContainsCaseInsensitive(a, "LEFT") || Platform_StringContainsCaseInsensitive(a, "RIGHT")) {
        int box = 0;
        if (pad == 0 && !Platform_StringContainsCaseInsensitive(a, "LEFT")) box = x;
        draw_wrapped_text(g_draw_dpy, g_draw_xd, &col, fh, text, y, (char*)a, pad ? pad : x, box);
    } else {
        XftFont *xf = FH_TO_XFT(fh);
        int len = (int)strlen(text);
        if (len == 1 && xf && RawLine_Push(x, y, text[0], color, fh, needDestroy, fontsize)) {
            needDestroy = false;
        } else {
            RawLine_Flush();
            if (xf) x11_draw_utf8_fallback(g_draw_dpy, g_draw_xd, &col, x, y + (xf->ascent), xf, text, len, fontsize > 0 ? fontsize : FontSize);
        }
    }
    XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &col);
    if (needDestroy) Platform_DestroyFont(fh);
    { PROFILE_END_VAR(_pt, "Platform_DrawTextRaw"); return true; }
}

static Display* g_line_dpy = NULL;
static int g_line_size = 0;
static XftFont* g_line_font = NULL;
static XftFont* LineFont(Display* dpy, int size) {
        PROFILE_START(_pt);
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
    { PROFILE_END_VAR(_pt, "LineFont"); return g_line_font; }
}

int Platform_MeasureTextWidth(const char* text, int len, int fontsize) {
        PROFILE_START(_pt);
if (!text || len <= 0) { PROFILE_END_VAR(_pt, "Platform_MeasureTextWidth"); return 0; }
    if (!g_draw_dpy) { PROFILE_END_VAR(_pt, "Platform_MeasureTextWidth"); return len * (fontsize > 0 ? fontsize : FontSize) / 2; }
    XftFont* xf = LineFont(g_draw_dpy, fontsize);
    if (!xf) { PROFILE_END_VAR(_pt, "Platform_MeasureTextWidth"); return len * (fontsize > 0 ? fontsize : FontSize) / 2; }
    { PROFILE_END_VAR(_pt, "Platform_MeasureTextWidth"); return x11_measure_utf8_fallback(g_draw_dpy, xf, text, len, fontsize > 0 ? fontsize : FontSize); }
}

bool Platform_DrawTextLine(int x, int y, const char* text, int len, unsigned long color, int fontsize) {
        PROFILE_START(_pt);
if (!text || len <= 0 || !g_draw_dpy || !g_draw_xd) { PROFILE_END_VAR(_pt, "Platform_DrawTextLine"); return false; }
    x += g_draw_off_x;
    y += g_draw_off_y;
    XftFont* xf = LineFont(g_draw_dpy, fontsize);
    if (!xf) { PROFILE_END_VAR(_pt, "Platform_DrawTextLine"); return false; }
    if (color == 0) color = rgb_to_hex(COLOR_TEXT);
    XftColor col;
    x11_xft_alloc_color(g_draw_dpy, g_draw_vis, g_draw_cmap, color, 0xFFFF, &col);
    x11_draw_utf8_fallback(g_draw_dpy, g_draw_xd, &col, x, y + xf->ascent, xf, text, len, fontsize > 0 ? fontsize : FontSize);
    XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &col);
    { PROFILE_END_VAR(_pt, "Platform_DrawTextLine"); return true; }
}

void Platform_DrawListBackground(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, int y, int h, bool selected) {
        PROFILE_START(_pt);
if (selected) {
        x11_draw_selection_background(dpy, xd, vis, cmap, 0, y, state->width, h, rgb_to_hex(COLOR_SELECTED_TEXT));
    }
}

void Platform_GetItemColors(int idx, int count, unsigned long *num_color, unsigned long *body_color, unsigned long *plain_color) {
        PROFILE_START(_pt);
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
        PROFILE_START(_pt);
(void)state;
    if (!lb) { PROFILE_END_VAR(_pt, "Platform_DrawListItem"); return ; }
    unsigned long ncol, bcol, pcol;
    Platform_GetItemColors(idx, lb->populated_count, &ncol, &bcol, &pcol);
    XftColor ncolor, bcolor, pcolor;
    x11_xft_alloc_color(dpy, vis, cmap, ncol, 0xFFFF, &ncolor);
    x11_xft_alloc_color(dpy, vis, cmap, bcol, 0xFFFF, &bcolor);
    x11_xft_alloc_color(dpy, vis, cmap, pcol, 0xFFFF, &pcolor);
    DrawListBoxItemText(dpy, xd, vis, cmap, font, &ncolor, &bcolor, &pcolor, lb,(char*)icon_path, idx, y, text_x);
    XftColorFree(dpy, vis, cmap, &ncolor);
    XftColorFree(dpy, vis, cmap, &bcolor);
    XftColorFree(dpy, vis, cmap, &pcolor);
}

void Platform_DrawEditBackground(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font) {
        PROFILE_START(_pt);
unsigned long window_title_text_color_map;
    eval_window_title_color(state,&window_title_text_color_map);
    Platform_SetDrawContext(dpy, xd, vis, cmap);
    if (WindowTitleText) Platform_DrawTextRaw(PROMPT_FOOTER_X, state->height - PROMPT_FOOTER_Y_FROM_BOTTOM, state->window_title, window_title_text_color_map, font, "LEFT", PROMPT_FOOTER_PAD, FontSize);
    if (VimModeText) Platform_DrawTextRaw(PROMPT_FOOTER_X, state->height - PROMPT_FOOTER_Y_FROM_BOTTOM, g_vim_mode, VimModeTextColor(), font, "RIGHT", PROMPT_FOOTER_PAD, FontSize);
}
void Platform_DrawEditText(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font) {
        PROFILE_START(_pt);
XftFont *xf = FH_TO_XFT(font);
    int cursor_x = 0;
    int text_len = strlen(state->filter);
    int scroll = x11_update_edit_scroll_offset(dpy, xf, state->filter, text_len, state->cursor, state->width, state->scroll_offset, &cursor_x);
    state->scroll_offset = scroll;
    int draw_x = PROMPT_EDIT_INSET - scroll;
    Platform_DrawEditTint(state, dpy, xd, vis, cmap, font);
    if (text_len > 0) {
        Platform_SetDrawContext(dpy, xd, vis, cmap);
        Platform_DrawTextRaw(draw_x, PROMPT_EDIT_INSET, state->filter, rgb_to_hex(COLOR_TEXT), font, "", 0, 0);
    }
}
static int x11_char_x(Display *dpy, XftFont *xf, PromptState *state, int index, int *out_scroll) {
        PROFILE_START(_pt);
int text_len = (int)strlen(state->filter);
    int cursor_x = 0;
    int scroll = x11_update_edit_scroll_offset(dpy, xf, state->filter, text_len, index, state->width, state->scroll_offset, &cursor_x);
    if (out_scroll) *out_scroll = scroll;
    { PROFILE_END_VAR(_pt, "x11_char_x"); return PROMPT_EDIT_INSET + cursor_x - scroll; }
}

void Platform_DrawEditTint(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font) {
        PROFILE_START(_pt);
if (!state) { PROFILE_END_VAR(_pt, "Platform_DrawEditTint"); return ; }
    if (IsInsertMode()) { PROFILE_END_VAR(_pt, "Platform_DrawEditTint"); return ; }
    XftFont *xf = FH_TO_XFT(font);
    int text_len = (int)strlen(state->filter);
    int sel_start = 0, sel_end = 0;
    bool has_sel = VimState_GetSelection(&sel_start, &sel_end) && text_len > 0;

    Platform_SetDrawContext(dpy, xd, vis, cmap);
    int tint_y = x11_font_cursor_y_of(font, PROMPT_EDIT_INSET);
    unsigned long tint = Color_Lerp(rgb_to_hex(COLOR_BG), rgb_to_hex(Modal_CursorColor()), 0.72f);

    if (has_sel) {
        if (sel_start < 0) sel_start = 0;
        if (sel_end >= text_len) sel_end = text_len - 1;
        if (sel_start > sel_end) return;
        int x0 = x11_char_x(dpy, xf, state, sel_start, NULL);
        int x1 = x11_char_x(dpy, xf, state, sel_end, NULL);
        int caret_x = x11_char_x(dpy, xf, state, state->cursor, NULL);
        if (x0 < PROMPT_CARET_LO_CLAMP) x0 = PROMPT_CARET_LO_CLAMP;
        if (x1 > state->width - PROMPT_CARET_HI_CLAMP) x1 = state->width - PROMPT_CARET_HI_CLAMP;
        if (caret_x < x0) caret_x = x0;
        if (caret_x > x1) caret_x = x1;
        int w = x1 - x0 + PromptNormalCaretWidth();
        if (w <= 0) return;
        Platform_DrawRectRaw(x0, tint_y, w, FontSize,tint,0,false,true,1,true);
        return;
    }

    int tint_x = x11_char_x(dpy, xf, state, state->cursor, NULL);
    if (tint_x < PROMPT_CARET_LO_CLAMP) tint_x = PROMPT_CARET_LO_CLAMP;
    if (tint_x > state->width - PROMPT_CARET_HI_CLAMP) tint_x = state->width - PROMPT_CARET_HI_CLAMP;
    Platform_DrawRectRaw(tint_x, tint_y, PromptNormalCaretWidth(),FontSize,tint,0,false,true,1,true);
}

void Platform_DrawEditCursor(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font) {
        PROFILE_START(_pt);
XftFont *xf = FH_TO_XFT(font);
    int cursor_x = 0;
    x11_update_edit_scroll_offset(dpy, xf, state->filter, strlen(state->filter), state->cursor, state->width, state->scroll_offset, &cursor_x);
    Platform_SetDrawContext(dpy, xd, vis, cmap);
    int cursor_draw_x = PROMPT_EDIT_INSET + cursor_x - state->scroll_offset;
    if (cursor_draw_x < PROMPT_CARET_LO_CLAMP) cursor_draw_x = PROMPT_CARET_LO_CLAMP;
    if (cursor_draw_x > state->width - PROMPT_CARET_HI_CLAMP) cursor_draw_x = state->width - PROMPT_CARET_HI_CLAMP;
    int cursor_y = x11_font_cursor_y_of(font, PROMPT_EDIT_INSET);
    if (!IsInsertMode()) { PROFILE_END_VAR(_pt, "Platform_DrawEditCursor"); return ; }
    Platform_DrawRectRaw(cursor_draw_x, cursor_y, PROMPT_CARET_W_INSERT, FontSize,rgb_to_hex(Modal_CursorColor()),0,false,true,1,true);
}

static GC raw_gc_for(Display* dpy, Drawable d);

static int raw_thickness_px(float thickness) {
        PROFILE_START(_pt);
if (thickness <= 0.0f) { PROFILE_END_VAR(_pt, "raw_thickness_px"); return 0; }
    int px = (int)(thickness + 0.5f);
    if (px < 1) px = 1;
    if (px > 255) px = 255;
    { PROFILE_END_VAR(_pt, "raw_thickness_px"); return px; }
}

static bool raw_can_draw(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "raw_can_draw"); return g_draw_dpy != NULL && g_draw_xd != NULL; }
}

static GC raw_gc_for(Display* dpy, Drawable d) {
        PROFILE_START(_pt);
static GC gc = NULL;
    static Drawable gc_owner = 0;
    if (gc != NULL && gc_owner == (Drawable)(uintptr_t)d) { PROFILE_END_VAR(_pt, "raw_gc_for"); return gc; }
    if (gc != NULL) XFreeGC(dpy, gc);
    gc = XCreateGC(dpy, d, 0, NULL);
    if (!gc) { PROFILE_END_VAR(_pt, "raw_gc_for"); return NULL; }
    gc_owner = (Drawable)(uintptr_t)d;
    { PROFILE_END_VAR(_pt, "raw_gc_for"); return gc; }
}

static GC raw_prepare(unsigned long color, int thickness) {
        PROFILE_START(_pt);
if (!raw_can_draw()) { PROFILE_END_VAR(_pt, "raw_prepare"); return NULL; }
    Drawable d = XftDrawDrawable(g_draw_xd);
    GC gc = raw_gc_for(g_draw_dpy, d);
    if (!gc) { PROFILE_END_VAR(_pt, "raw_prepare"); return NULL; }
    XftColor col;
    x11_xft_alloc_color(g_draw_dpy, g_draw_vis, g_draw_cmap, color, OpacityAlpha16(), &col);
    XSetForeground(g_draw_dpy, gc, col.pixel);
    XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &col);
    XSetLineAttributes(g_draw_dpy, gc, thickness > 0 ? thickness : 1, LineSolid, CapButt, JoinMiter);
    { PROFILE_END_VAR(_pt, "raw_prepare"); return gc; }
}

bool Platform_DrawRectRaw(int x, int y, int w, int h, unsigned long color, int radius, bool rounded, bool opaque,
                             float thickness, bool filled) {
    PROFILE_START(_pt);

    if (!raw_can_draw()) {
        PROFILE_END_VAR(_pt, "Platform_DrawRectRaw");
        return false;
    }
    if (w <= 0 || h <= 0) {
        PROFILE_END_VAR(_pt, "Platform_DrawRectRaw");
        return false;
    }

    x += g_draw_off_x;
    y += g_draw_off_y;

    int t = raw_thickness_px(thickness);
    if (t <= 0) t = 1;

    bool want_round = rounded && radius > 0;
    int r = 0;
    if (want_round) {
        r = radius;
        if (r * 2 > w) r = w / 2;
        if (r * 2 > h) r = h / 2;
        if (r < 0) r = 0;
    }

    if (filled) {
        measure_track(x, y, w, h);
    } else {
        measure_track(x - t, y - t, w + 2 * t, h + 2 * t);
    }

    unsigned short target_alpha = opaque ? 0xFFFFu : OpacityAlpha16();

    if (filled) {
        if (want_round && r > 0) {
            x11_fill_rounded_rect(g_draw_dpy, g_draw_xd, g_draw_vis, g_draw_cmap,
                                  color, target_alpha, x, y, w, h, r, w, h);
        } else {
            XftColor col;
            x11_xft_alloc_color(g_draw_dpy, g_draw_vis, g_draw_cmap, color, target_alpha, &col);
            XftDrawRect(g_draw_xd, &col, x, y, w, h);
            XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &col);
        }
        PROFILE_END_VAR(_pt, "Platform_DrawRectRaw");
        return true;
    }

    Drawable d = XftDrawDrawable(g_draw_xd);
    GC gc = raw_gc_for(g_draw_dpy, d);
    if (!gc) {
        PROFILE_END_VAR(_pt, "Platform_DrawRectRaw");
        return false;
    }
    XftColor col;
    x11_xft_alloc_color(g_draw_dpy, g_draw_vis, g_draw_cmap, color, target_alpha, &col);
    XSetForeground(g_draw_dpy, gc, col.pixel);
    XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &col);
    XSetLineAttributes(g_draw_dpy, gc, t, LineSolid, CapButt, JoinMiter);

    if (want_round) {
        if (r > 0) {
            XArc arcs[4] = {
                { (short)x,           (short)y,           (unsigned short)(r*2), (unsigned short)(r*2),  90*64,  90*64 },
                { (short)(x + w - r*2), (short)y,           (unsigned short)(r*2), (unsigned short)(r*2),   0*64,  90*64 },
                { (short)(x + w - r*2), (short)(y + h - r*2), (unsigned short)(r*2), (unsigned short)(r*2), 270*64,  90*64 },
                { (short)x,           (short)(y + h - r*2), (unsigned short)(r*2), (unsigned short)(r*2), 180*64,  90*64 }
            };
            XDrawArcs(g_draw_dpy, d, gc, arcs, 4);

            XSegment segments[4] = {
                { x + r,     y,         x + w - r, y         },
                { x + w,     y + r,     x + w,     y + h - r },
                { x + r,     y + h,     x + w - r, y + h     },
                { x,         y + r,     x,         y + h - r }
            };
            XDrawSegments(g_draw_dpy, d, gc, segments, 4);
        } else {
            XDrawRectangle(g_draw_dpy, d, gc, x, y, w, h);
        }
    } else {
        XftColor border_col;
        x11_xft_alloc_color(g_draw_dpy, g_draw_vis, g_draw_cmap, color, target_alpha, &border_col);
        
        XftDrawRect(g_draw_xd, &border_col, x,         y,         w, t); 
        XftDrawRect(g_draw_xd, &border_col, x,         y + h - t, w, t); 
        XftDrawRect(g_draw_xd, &border_col, x,         y,         t, h); 
        XftDrawRect(g_draw_xd, &border_col, x + w - t, y,         t, h); 
        
        XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &border_col);
    }

    PROFILE_END_VAR(_pt, "Platform_DrawRectRaw");
    return true;
}

bool Platform_DrawLineRaw(int x1, int y1, int x2, int y2, unsigned long color, float thickness) {
        PROFILE_START(_pt);
if (!raw_can_draw()) { PROFILE_END_VAR(_pt, "Platform_DrawLineRaw"); return false; }
    int t = raw_thickness_px(thickness);
    if (t < 1) t = 1;
    x1 += g_draw_off_x;
    y1 += g_draw_off_y;
    x2 += g_draw_off_x;
    y2 += g_draw_off_y;
    int bx = x1 < x2 ? x1 : x2;
    int by = y1 < y2 ? y1 : y2;
    int bw = (x1 > x2 ? x1 - x2 : x2 - x1) + t;
    int bh = (y1 > y2 ? y1 - y2 : y2 - y1) + t;
    measure_track(bx - t / 2, by - t / 2, bw, bh);
    GC gc = raw_prepare(color, t);
    if (!gc) { PROFILE_END_VAR(_pt, "Platform_DrawLineRaw"); return false; }
    Drawable d = XftDrawDrawable(g_draw_xd);
    XDrawLine(g_draw_dpy, d, gc, x1, y1, x2, y2);
    { PROFILE_END_VAR(_pt, "Platform_DrawLineRaw"); return true; }
}

bool Platform_DrawCircleRaw(int cx, int cy, int radius, unsigned long color, float thickness) {
        PROFILE_START(_pt);
if (!raw_can_draw() || radius <= 0) { PROFILE_END_VAR(_pt, "Platform_DrawCircleRaw"); return false; }
    int t = raw_thickness_px(thickness);
    cx += g_draw_off_x;
    cy += g_draw_off_y;
    measure_track(cx - radius, cy - radius, radius * 2, radius * 2);
    GC gc = raw_prepare(color, t);
    if (!gc) { PROFILE_END_VAR(_pt, "Platform_DrawCircleRaw"); return false; }
    Drawable d = XftDrawDrawable(g_draw_xd);
    unsigned dd = (unsigned)(radius * 2);
    if (t > 0) XDrawArc(g_draw_dpy, d, gc, cx - radius, cy - radius, dd, dd, 0, 360 * 64);
    else XFillArc(g_draw_dpy, d, gc, cx - radius, cy - radius, dd, dd, 0, 360 * 64);
    { PROFILE_END_VAR(_pt, "Platform_DrawCircleRaw"); return true; }
}

void Platform_DrawEditBox(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (!state) { PROFILE_END_VAR(timer, "draw_x11:Platform_DrawEditBox"); return; }
    FlintContext_Get()->window.editbox_placed = true;
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    Platform_DrawEditBackground(state, dpy, xd, vis, cmap, font);
    Platform_DrawEditText(state, dpy, xd, vis, cmap, font);
    Platform_DrawEditCursor(state, dpy, xd, vis, cmap, font);
    Platform_DestroyFont(font);
    PROFILE_END_VAR(timer, "draw_x11:Platform_DrawEditBox");
}

void Platform_GetTextLayoutMetrics(int* line_h, int* row_h, int* list_top) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    Display* dpy = get_display();
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    int lh = FontSize;
    int rh = x11_font_row_height_of(font);
    int lt = x11_font_list_top_of(font);
    Platform_DestroyFont(font);
    if (line_h) *line_h = lh;
    if (row_h) *row_h = rh;
    if (list_top) *list_top = lt;
    PROFILE_END_VAR(timer, "draw_x11:Platform_GetTextLayoutMetrics");
}

void Platform_DrawListBox(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (!state || !state->has_listbox) { PROFILE_END_VAR(timer, "draw_x11:Platform_DrawListBox"); return; }
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    ListBoxInfo *lb = &state->listbox;
    if (!lb->populated_count) { Platform_DestroyFont(font); PROFILE_END_VAR(timer, "draw_x11:Platform_DrawListBox"); return; }

    int itemH = x11_font_row_height_of(font);
    int y0 = x11_font_list_top_of(font);
    int yOffset = PROMPT_LIST_BOTTOM_TITLE;
    if (!WindowTitleText && !VimModeText) yOffset = 0;
    int visible = (state->height - y0 - yOffset) / itemH;
    if (visible < 1) visible = 1;
    if (visible > lb->populated_count) visible = lb->populated_count;
    UI_LB_SetVisibleCount(visible);

    if (lb->selected_index >= 0) {
        if (lb->selected_index < lb->view_offset + PaddingForScrolling)
            lb->view_offset = lb->selected_index - PaddingForScrolling;
        else if (lb->selected_index >= lb->view_offset + visible - PaddingForScrolling)
            lb->view_offset = lb->selected_index - visible + PaddingForScrolling + 1;
    }
    if (lb->view_offset < 0) lb->view_offset = 0;
    if (lb->view_offset + visible > lb->populated_count)
        lb->view_offset = lb->populated_count - visible;
    if (lb->view_offset < 0) lb->view_offset = 0;

    for (int i = 0; i < visible; i++) {
        int idx = lb->view_offset + i;
        int y = y0 + i * itemH;
        bool sel = (idx == lb->selected_index);
        if (RoundedCorners && CornerRadius>0) {
            int R=PROMPT_SEL_RADIUS; int t=PROMPT_SEL_INSET;
            int bx=PROMPT_SEL_X, bw=state->width-PROMPT_SEL_W_SHRINK;
            if (sel) {
                x11_fill_rounded_rect(dpy, xd, vis, cmap, rgb_to_hex(COLOR_SELECTED_TEXT), 0xFFFF, bx+t, y+PROMPT_SEL_TOP_INSET+t, bw-2*t, itemH-2-2*t, R-1, state->width, state->height);
            }
        } else {
            Platform_DrawListBackground(state, dpy, xd, vis, cmap, y, itemH, sel);
        }
        //temporary
        const char *icon_path = NULL;
        Platform_DrawListItem(state, dpy, xd, vis, cmap, font, lb, idx, y, PROMPT_ITEM_TEXT_X, icon_path);
    }

    Platform_DestroyFont(font);
    PROFILE_END_VAR(timer, "draw_x11:Platform_DrawListBox");
}
