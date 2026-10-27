#include "platform/draw.h"
#include "core/prompt_geometry.h"
#include "core/editbox.h"
#include "core/modal.h"
#include "bindings/vim_state.h"
#include "platform/platform.h"
#include "platform/win32/win32_state.h"
#include "platform/win32/common_ui_win32.h"
#include "config.h"
#include "platform/ui.h"
#include "core/context.h"
#include "helpers/helpers_ui.h"
#include "core/timer.h"
#include "core/draw_state.h"
#include "deps/log.h"
#include <stdio.h>
#include <string.h>
#include <objidl.h>
#include <gdiplus/gdiplus.h>
#include <shlwapi.h>
#include <shellapi.h>

#define WIN32_SIZE_FONT_SLOTS 8
static __thread HFONT g_sizeFonts[WIN32_SIZE_FONT_SLOTS] = {NULL};
static __thread int g_sizeFontSizes[WIN32_SIZE_FONT_SLOTS] = {0};

static wchar_t* win32_widen(const char* in, int in_len, int* out_len) {
        PROFILE_START(_pt);
if (out_len) *out_len = 0;
    if (!in || in_len <= 0) { PROFILE_END_VAR(_pt, "win32_widen"); return NULL; }
    int n = MultiByteToWideChar(CP_UTF8, 0, in, in_len, NULL, 0);
    if (n <= 0) { PROFILE_END_VAR(_pt, "win32_widen"); return NULL; }
    wchar_t* w = (wchar_t*)malloc((size_t)n * sizeof(wchar_t) + sizeof(wchar_t));
    if (!w) { PROFILE_END_VAR(_pt, "win32_widen"); return NULL; }
    if (MultiByteToWideChar(CP_UTF8, 0, in, in_len, w, n) <= 0) { free(w); return NULL; }
    w[n] = 0;
    if (out_len) *out_len = n;
    { PROFILE_END_VAR(_pt, "win32_widen"); return w; }
}

static __thread HDC g_draw_hdc = NULL;

static HFONT win32_sized_font(int size);

static bool win32_text_is_symbolic(const wchar_t* wide, int len) {
        PROFILE_START(_pt);
for (int i = 0; i < len; i++) {
        unsigned int c = (unsigned int)wide[i];
        if (c == 0xFE0F || c == 0xFE0E) continue;
        if (c > 0x7F) return true;
    }
    { PROFILE_END_VAR(_pt, "win32_text_is_symbolic"); return false; }
}

static bool win32_font_covers(HFONT f, const wchar_t* wide, int len) {
        PROFILE_START(_pt);
if (!f || !wide || len <= 0) { PROFILE_END_VAR(_pt, "win32_font_covers"); return false; }
    HFONT old = (HFONT)SelectObject(g_draw_hdc, f);
    bool ok = true;
    for (int i = 0; i < len; i++) {
        if (wide[i] < 0x80) continue;
        if (wide[i] >= 0xD800 && wide[i] <= 0xDBFF) {
            if (i + 1 < len && wide[i+1] >= 0xDC00 && wide[i+1] <= 0xDFFF) { i++; continue; }
        }
        WORD gi = 0xFFFF;
        DWORD r = GetGlyphIndicesW(g_draw_hdc, wide + i, 1, &gi, 0);
        if (r == GDI_ERROR || gi == 0xFFFF || gi == 0) { ok = false; break; }
    }
    SelectObject(g_draw_hdc, old);
    { PROFILE_END_VAR(_pt, "win32_font_covers"); return ok; }
}

#define WIN32_SYM_SLOTS 4
static HFONT g_sym_fonts[WIN32_SYM_SLOTS] = {NULL};
static int g_sym_sizes[WIN32_SYM_SLOTS] = {0};
static wchar_t g_sym_cp[WIN32_SYM_SLOTS][8] = {{0}};
static int g_sym_len[WIN32_SYM_SLOTS] = {0};
static ULONGLONG g_sym_tick = 0;

static HFONT win32_symbol_font(int size, const wchar_t* wide, int len) {
        PROFILE_START(_pt);
if (len > 8) len = 8;
    for (int i = 0; i < WIN32_SYM_SLOTS; i++) {
        if (g_sym_fonts[i] && g_sym_sizes[i] == size && g_sym_len[i] == len &&
            memcmp(g_sym_cp[i], wide, (size_t)len * sizeof(wchar_t)) == 0)
            return g_sym_fonts[i];
    }

    static const char* faces[] = {"Segoe UI Symbol", "Segoe UI Emoji", "Malgun Gothic",
                                  "Segoe UI", "Arial Unicode MS", NULL};
    HFONT found = NULL;
    for (int i = 0; faces[i]; i++) {
        HFONT f = FH_TO_HFONT(Platform_CreateFont(size, faces[i]));
        if (!f) continue;
        if (win32_font_covers(f, wide, len)) { found = f; break; }
        DeleteObject(f);
    }
    if (!found) { PROFILE_END_VAR(_pt, "win32_symbol_font"); return NULL; }

    int slot = -1;
    ULONGLONG oldest = ~(ULONGLONG)0;
    for (int i = 0; i < WIN32_SYM_SLOTS; i++) {
        if (!g_sym_fonts[i]) { slot = i; break; }
    }
    if (slot < 0) {
        for (int i = 0; i < WIN32_SYM_SLOTS; i++) {
            if (g_sym_sizes[i] == 0) { slot = i; break; }
        }
    }
    if (slot < 0) {
        for (int i = 0; i < WIN32_SYM_SLOTS; i++) {
            ULONGLONG t = (ULONGLONG)g_sym_sizes[i];
            if (t <= oldest) { oldest = t; slot = i; }
        }
    }
    if (g_sym_fonts[slot]) DeleteObject(g_sym_fonts[slot]);
    g_sym_fonts[slot] = found;
    g_sym_sizes[slot] = size;
    g_sym_len[slot] = len;
    for (int k = 0; k < len; k++) g_sym_cp[slot][k] = wide[k];
    g_sym_tick++;
    { PROFILE_END_VAR(_pt, "win32_symbol_font"); return found; }
}

#define WIN32_SYMBOL_SIZE_NUM 5
#define WIN32_SYMBOL_SIZE_DEN 4

static bool win32_text_needs_boost(const wchar_t* wide, int len) {
        PROFILE_START(_pt);
for (int i = 0; i < len; i++) {
        unsigned int c = (unsigned int)wide[i];
        if (c == 0xFE0F || c == 0xFE0E) continue;
        if (c >= 0x1F000) return true;
        if (c >= 0xD800 && c <= 0xDFFF) return true;
        if (c >= 0xE000 && c <= 0xF8FF) return true;
    }
    { PROFILE_END_VAR(_pt, "win32_text_needs_boost"); return false; }
}

static HFONT win32_font_for_text(const wchar_t* wide, int len, int fontsize, HFONT* out_prev, int* out_dy) {
        PROFILE_START(_pt);
HFONT prev = NULL;
    if (out_dy) *out_dy = 0;
    if (win32_text_is_symbolic(wide, len)) {
        bool boost = win32_text_needs_boost(wide, len);
        int want = boost ? (fontsize * WIN32_SYMBOL_SIZE_NUM / WIN32_SYMBOL_SIZE_DEN) : fontsize;
        HFONT sf = win32_symbol_font(want, wide, len);
        if (sf) {
            HFONT nf = win32_sized_font(fontsize);
            if (!nf) nf = g_win32_hMainFont;
            HFONT tmp = nf ? (HFONT)SelectObject(g_draw_hdc, nf) : NULL;
            TEXTMETRICW ts;
            ts.tmAscent = 0;
            if (nf) GetTextMetricsW(g_draw_hdc, &ts);
            if (tmp) SelectObject(g_draw_hdc, tmp);
            prev = (HFONT)SelectObject(g_draw_hdc, sf);
            if (boost) {
                TEXTMETRICW tb;
                GetTextMetricsW(g_draw_hdc, &tb);
                if (out_dy) *out_dy = (ts.tmAscent - tb.tmAscent);
            }
            if (out_prev) *out_prev = prev;
            return sf;
        }
    }
    HFONT hfont = win32_sized_font(fontsize);
    if (!hfont) hfont = g_win32_hMainFont;
    if (hfont) prev = (HFONT)SelectObject(g_draw_hdc, hfont);
    if (out_prev) *out_prev = prev;
    { PROFILE_END_VAR(_pt, "win32_font_for_text"); return hfont; }
}

static HFONT win32_sized_font(int size) {
    PROFILE_START(_pt);
if (size <= 0) size = FontSize;
  for (int i = 0; i < WIN32_SIZE_FONT_SLOTS; i++) {
    if (g_sizeFonts[i] && g_sizeFontSizes[i] == size) return g_sizeFonts[i];
  }
  int slot = -1;
  for (int i = 0; i < WIN32_SIZE_FONT_SLOTS; i++) {
    if (!g_sizeFonts[i]) { slot = i; break; }
  }
  if (slot < 0) {
    slot = 0;
    for (int i = 1; i < WIN32_SIZE_FONT_SLOTS; i++) {
      if (g_sizeFontSizes[i] < g_sizeFontSizes[slot]) slot = i;
    }
  }
  if (g_sizeFonts[slot]) DeleteObject(g_sizeFonts[slot]);
  g_sizeFonts[slot] = FH_TO_HFONT(Platform_CreateFont(size, UsedFont));
  g_sizeFontSizes[slot] = size;
  { PROFILE_END_VAR(_pt, "win32_sized_font"); return g_sizeFonts[slot]; }
}


void Platform_SetTransparentWindow(bool on) {
        PROFILE_START(_pt);
PlDrawState_Get()->transparent = on;
}
bool Platform_GetTransparentWindow(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_GetTransparentWindow"); return PlDrawState_Get()->transparent; }
}

void Platform_SetDrawBgColor(unsigned long hex) {
        PROFILE_START(_pt);
PlDrawState_Get()->bg_override = hex;
}
unsigned long Platform_GetDrawBgColor(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_GetDrawBgColor"); return PlDrawState_Get()->bg_override; }
}


void Platform_BeginMeasure(void) {
        PROFILE_START(_pt);
    PlDrawState_BeginMeasure();
    PROFILE_END_VAR(_pt, "Platform_BeginMeasure");
}
void Platform_EndMeasure(int *out_w, int *out_h) {
        PROFILE_START(_pt);
    PlDrawState_EndMeasure(out_w, out_h);
    PROFILE_END_VAR(_pt, "Platform_EndMeasure");
}
void Platform_GetLastMeasureOrigin(int *out_x, int *out_y) {
        PROFILE_START(_pt);
if (out_x) *out_x = PlDrawState_Get()->meas_origin_x;
    if (out_y) *out_y = PlDrawState_Get()->meas_origin_y;
}
void Platform_SetDrawOffset(int dx, int dy) {
        PROFILE_START(_pt);
PlDrawState_Get()->off_x = dx;
    PlDrawState_Get()->off_y = dy;
}
void Platform_GetDrawOffset(int *out_x, int *out_y) {
    if (out_x) *out_x = PlDrawState_Get()->off_x;
    if (out_y) *out_y = PlDrawState_Get()->off_y;
}
bool Platform_WindowToRoot(WindowHandle h, int *out_x, int *out_y) {
    HWND w = WH_TO_HWND(h);
    if (!w || !IsWindow(w)) return false;
    POINT pt;
    pt.x = 0;
    pt.y = 0;
    if (!ClientToScreen(w, &pt)) return false;
    if (out_x) *out_x = (int)pt.x;
    if (out_y) *out_y = (int)pt.y;
    return true;
}
static void measure_track(int x, int y, int w, int h) {
        PROFILE_START(_pt);
    PlDrawState_TrackMeasure(x, y, w, h);
    PROFILE_END_VAR(_pt, "measure_track");
}
void Platform_SetDrawContextWin32(HDC hdc) {
    PROFILE_START(_pt);
g_draw_hdc = hdc; 
}
void Platform_GetDrawContextWin32(HDC *out) {
    PROFILE_START(_pt);
if (out) *out = g_draw_hdc; 
}
bool Platform_DrawTextRaw(int x, int y, const char *text, unsigned long color, FontHandle font, const char *align, int pad,int fontsize) {
        PROFILE_START(_pt);
if (!text || !g_draw_hdc) { PROFILE_END_VAR(_pt, "Platform_DrawTextRaw"); return false; }
    int _ox = PlDrawState_Get()->off_x;
    int _oy = PlDrawState_Get()->off_y;
    int _iy = y;
    x += _ox;
    y += _oy;
    log_info("[IMGDBG][win32-text] in=%d,%d off=%d,%d final=%d,%d", x-_ox, _iy, _ox, _oy, x, y);
    if (color != 0) color = rgb_to_hex(color);
    if (color == 0) color = COLOR_TEXT;
    const char *a = align ? align : "CENTER";
    HFONT old = NULL;
    int tlen = (int)strlen(text);
    int wlen = 0;
    wchar_t* wide = win32_widen(text, tlen, &wlen);
    if (!wide) { PROFILE_END_VAR(_pt, "Platform_DrawTextRaw"); return false; }
    int dy = 0;
    HFONT hfont = font.hfont ? font.hfont : win32_font_for_text(wide, wlen, fontsize, &old, &dy);
    if (!hfont) hfont = win32_sized_font(fontsize);
    if (!hfont) hfont = g_win32_hMainFont;
    if (!old && hfont) old = (HFONT)SelectObject(g_draw_hdc, hfont);
    TEXTMETRICW tm = {0};
    GetTextMetricsW(g_draw_hdc, &tm);

    int y_draw = y + dy + (tm.tmHeight - tm.tmAscent) / 2;
    log_info("[IMGDBG][win32-text] tmHeight=%d tmAscent=%d dy=%d y_draw=%d", tm.tmHeight, tm.tmAscent, dy, y_draw);
    SetBkMode(g_draw_hdc, TRANSPARENT);
    SetTextColor(g_draw_hdc, color);
    bool centered = Platform_StringContainsCaseInsensitive(a,"CENTER");
    bool right = !centered && Platform_StringContainsCaseInsensitive(a,"RIGHT");
    bool measuring = PlDrawState_Get()->measuring;
    SIZE msz;
    msz.cx = 0;
    msz.cy = 0;
    if (measuring || centered || right) {
        if (dy != 0) {
            HFONT nf = win32_sized_font(fontsize);
            if (!nf) nf = g_win32_hMainFont;
            if (nf) {
                HFONT t2 = (HFONT)SelectObject(g_draw_hdc, nf);
                GetTextExtentPoint32W(g_draw_hdc, wide, wlen, &msz);
                SelectObject(g_draw_hdc, t2);
            } else {
                GetTextExtentPoint32W(g_draw_hdc, wide, wlen, &msz);
            }
        } else {
            GetTextExtentPoint32W(g_draw_hdc, wide, wlen, &msz);
        }
    }
    if (measuring) {
        int bx = x;
        if (centered) bx = x - msz.cx / 2;
        else if (right) bx = x - msz.cx;
        measure_track(bx, y, msz.cx, msz.cy);
    }
    if (!centered && !right) {
        int x0 = pad ? pad : x;
        if (measuring) measure_track(x0, y, msz.cx, msz.cy);
        TextOutW(g_draw_hdc, x0, y_draw, wide, wlen);
        free(wide);
        if (old) SelectObject(g_draw_hdc, old);
        return true;
    }
    int box = (pad == 0) ? x : 0;
    int x0;
    if (centered) {
        x0 = box > 0 ? (box - msz.cx) / 2 : pad + (PROMPT_WRAP_BOX_CENTER - msz.cx) / 2;
    } else {
        x0 = box > 0 ? box - msz.cx : PROMPT_WRAP_BOX_RIGHT - msz.cx - pad;
    }
    TextOutW(g_draw_hdc, x0, y_draw, wide, wlen);
    free(wide);
    if (old) SelectObject(g_draw_hdc, old);
    { PROFILE_END_VAR(_pt, "Platform_DrawTextRaw"); return true; }
}

void Platform_DrawTextCentered(int x, int y, int w, int h, const char *text, unsigned long color, FontHandle font, int fontsize) {
    PROFILE_START(_pt);
    if (!text || !g_draw_hdc) { PROFILE_END_VAR(_pt, "Platform_DrawTextCentered"); return; }
    HFONT hfont = font.hfont ? font.hfont : win32_sized_font(fontsize);
    HFONT old = hfont ? (HFONT)SelectObject(g_draw_hdc, hfont) : NULL;
    TEXTMETRICW tm = {0};
    GetTextMetricsW(g_draw_hdc, &tm);
    int ascent = tm.tmAscent;
    int descent = tm.tmDescent;
    int lineHeight = ascent + descent;
    int tw = Platform_MeasureTextWidth(text, (int)strlen(text), fontsize);
    int tx = x + (w - tw) / 2;
    int ty = y + (h - lineHeight) / 2;
    Platform_DrawTextRaw(tx, ty, text, color, font, "LEFT", 0, fontsize);
    if (old) SelectObject(g_draw_hdc, old);
    { PROFILE_END_VAR(_pt, "Platform_DrawTextCentered"); }
}

int Platform_MeasureTextWidth(const char* text, int len, int fontsize) {
        PROFILE_START(_pt);
if (!text || len <= 0) { PROFILE_END_VAR(_pt, "Platform_MeasureTextWidth"); return 0; }
    if (!g_draw_hdc) { PROFILE_END_VAR(_pt, "Platform_MeasureTextWidth"); return len * (fontsize > 0 ? fontsize : 16) / 2; }
    int wlen = 0;
    wchar_t* wide = win32_widen(text, len, &wlen);
    if (!wide) { PROFILE_END_VAR(_pt, "Platform_MeasureTextWidth"); return 0; }
    HFONT old = NULL;
    HFONT hfont = win32_font_for_text(wide, wlen, fontsize, &old, NULL);
    if (!hfont) hfont = win32_sized_font(fontsize);
    SIZE sz;
    sz.cx = 0;
    GetTextExtentPoint32W(g_draw_hdc, wide, wlen, &sz);
    if (old) SelectObject(g_draw_hdc, old);
    free(wide);
    { PROFILE_END_VAR(_pt, "Platform_MeasureTextWidth"); return sz.cx; }
}

bool Platform_DrawTextLine(int x, int y, const char* text, int len, unsigned long color, int fontsize) {
        PROFILE_START(_pt);
if (!text || len <= 0 || !g_draw_hdc) { PROFILE_END_VAR(_pt, "Platform_DrawTextLine"); return false; }
    int _ox = PlDrawState_Get()->off_x;
    int _oy = PlDrawState_Get()->off_y;
    int _iy = y;
    x += _ox;
    y += _oy;
    log_info("[IMGDBG][win32-text] in=%d,%d off=%d,%d final=%d,%d", x-_ox, _iy, _ox, _oy, x, y);
    if (color != 0) color = rgb_to_hex(color);
    if (color == 0) color = COLOR_TEXT;
    int wlen = 0;
    wchar_t* wide = win32_widen(text, len, &wlen);
    if (!wide) { PROFILE_END_VAR(_pt, "Platform_DrawTextLine"); return false; }
    HFONT hfont = win32_sized_font(fontsize);
    HFONT old = hfont ? (HFONT)SelectObject(g_draw_hdc, hfont) : NULL;
    SetBkMode(g_draw_hdc, TRANSPARENT);
    SetTextColor(g_draw_hdc, color);
    TextOutW(g_draw_hdc, x, y, wide, wlen);
    if (old) SelectObject(g_draw_hdc, old);
    free(wide);
    { PROFILE_END_VAR(_pt, "Platform_DrawTextLine"); return true; }
}

void Platform_GetItemColors(int idx, int count, unsigned long *num_color, unsigned long *body_color, unsigned long *plain_color) {
        PROFILE_START(_pt);
(void)count;
    unsigned long col_text = COLOR_TEXT;
    unsigned long col_alt = COLOR_ALTERNATE;
    float t = (GradientItems > 1) ? (float)(idx % GradientItems) / (float)(GradientItems - 1) : 0;
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

static COLORREF win32_colorref(unsigned long hex) {
        PROFILE_START(_pt);
unsigned long r = hex & 0xFF;
    unsigned long g = (hex >> 8) & 0xFF;
    unsigned long b = (hex >> 16) & 0xFF;
    { PROFILE_END_VAR(_pt, "win32_colorref"); return (COLORREF)((b << 16) | (g << 8) | r); }
}

static COLORREF win32_resolve_color(unsigned long hex, bool opaque) {
        PROFILE_START(_pt);
COLORREF c = win32_colorref(hex);
    if (opaque) { PROFILE_END_VAR(_pt, "win32_resolve_color"); return c; }
    float t = WindowOpacity / 255.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    COLORREF bg = win32_colorref(COLOR_BG);
    int br = (bg >> 16) & 0xFF, bg_g = (bg >> 8) & 0xFF, bb = bg & 0xFF;
    int r = (int)((c & 0xFF) * t + br * (1.0f - t));
    int g = (int)(((c >> 8) & 0xFF) * t + bg_g * (1.0f - t));
    int b = (int)(((c >> 16) & 0xFF) * t + bb * (1.0f - t));
    { PROFILE_END_VAR(_pt, "win32_resolve_color"); return (COLORREF)((b << 16) | (g << 8) | r); }
}

static int win32_thickness_px(float thickness) {
        PROFILE_START(_pt);
if (thickness <= 0.0f) { PROFILE_END_VAR(_pt, "win32_thickness_px"); return 0; }
    int px = (int)(thickness + 0.5f);
    if (px < 1) px = 1;
    if (px > 255) px = 255;
    { PROFILE_END_VAR(_pt, "win32_thickness_px"); return px; }
}

bool Platform_DrawRectRaw(int x, int y, int w, int h, unsigned long color, int radius, bool rounded, bool opaque,
                             float thickness, bool filled) {
    PROFILE_START(_pt);

    if (!g_draw_hdc || w <= 0 || h <= 0) {
        PROFILE_END_VAR(_pt, "Platform_DrawRectRaw");
        return false;
    }

    x += PlDrawState_Get()->off_x;
    y += PlDrawState_Get()->off_y;

    int corner_w = 0;
    int corner_h = 0;
    if (rounded && radius > 0) {
        int r = radius;
        if (r * 2 > w) r = w / 2;
        if (r * 2 > h) r = h / 2;
        
        corner_w = r * 2;
        corner_h = r * 2;
    }

    if (filled || thickness <= 0.0f) {
        int fill_t = filled ? 0 : 1;
        if (filled) measure_track(x, y, w, h);
        else measure_track(x - fill_t, y - fill_t, w + 2 * fill_t, h + 2 * fill_t);
        HBRUSH brush = CreateSolidBrush(win32_resolve_color(color, opaque));
        HGDIOBJ old_brush = SelectObject(g_draw_hdc, brush);
        HGDIOBJ old_pen = SelectObject(g_draw_hdc, GetStockObject(NULL_PEN));

        if (corner_w > 0 && corner_h > 0) {
            RoundRect(g_draw_hdc, x, y, x + w, y + h, corner_w, corner_h);
        } else {
            /* RoundRect renders nothing when the rect is 1px thin, which
               silently killed the 1px prompt separator. */
            RECT rc;
            rc.left = x;
            rc.top = y;
            rc.right = x + w;
            rc.bottom = y + h;
            FillRect(g_draw_hdc, &rc, brush);
        }

        SelectObject(g_draw_hdc, old_pen);
        SelectObject(g_draw_hdc, old_brush);
        DeleteObject(brush);

        PROFILE_END_VAR(_pt, "Platform_DrawRectRaw");
        return true;
    }

    int t = win32_thickness_px(thickness);

    measure_track(x - t, y - t, w + 2 * t, h + 2 * t);

    HPEN pen = CreatePen(PS_SOLID, t, win32_resolve_color(color, opaque));
    HGDIOBJ old_pen = SelectObject(g_draw_hdc, pen);
    HGDIOBJ old_br = SelectObject(g_draw_hdc, GetStockObject(NULL_BRUSH));

    RoundRect(g_draw_hdc, x, y, x + w, y + h, corner_w, corner_h);

    SelectObject(g_draw_hdc, old_br);
    SelectObject(g_draw_hdc, old_pen);
    DeleteObject(pen);

    PROFILE_END_VAR(_pt, "Platform_DrawRectRaw");
    return true;
}

int Platform_GetGlyphInkHeight(const char *text, int fontsize) {
        PROFILE_START(_pt);
if (!text || !text[0] || fontsize <= 0) { PROFILE_END_VAR(_pt, "Platform_GetGlyphInkHeight"); return fontsize > 0 ? fontsize : 0; }
    if (!g_draw_hdc) { PROFILE_END_VAR(_pt, "Platform_GetGlyphInkHeight"); return fontsize; }
    int wlen = 0;
    wchar_t *wide = win32_widen(text, (int)strlen(text), &wlen);
    if (!wide || wlen <= 0) { PROFILE_END_VAR(_pt, "Platform_GetGlyphInkHeight"); return fontsize; }
    FontHandle font = Platform_CreateFont(fontsize, UsedFont);
    HFONT old = font.hfont ? (HFONT)SelectObject(g_draw_hdc, font.hfont) : NULL;
    GLYPHMETRICS gm;
    memset(&gm, 0, sizeof(gm));
    GetGlyphOutlineW(g_draw_hdc, (UINT)(wide[0] & 0xFFFF), GGO_METRICS, &gm, 0, NULL, NULL);
    if (old) SelectObject(g_draw_hdc, old);
    Platform_DestroyFont(font);
    free(wide);
    int ink = (int)(gm.gmBlackBoxY);
    if (ink <= 0) ink = fontsize;
    log_info("[IMGDBG][win32-glyph] fontsize=%d ink=%d return=%d", fontsize, ink, ink + PROMPT_GLYPH_INK_PAD);
    { PROFILE_END_VAR(_pt, "Platform_GetGlyphInkHeight"); return ink + PROMPT_GLYPH_INK_PAD; }
}

bool Platform_DrawLineRaw(int x1, int y1, int x2, int y2, unsigned long color, float thickness) {
        PROFILE_START(_pt);
int t = win32_thickness_px(thickness);
    if (t < 1) t = 1;
    x1 += PlDrawState_Get()->off_x; y1 += PlDrawState_Get()->off_y;
    x2 += PlDrawState_Get()->off_x; y2 += PlDrawState_Get()->off_y;
    int bx = x1 < x2 ? x1 : x2;
    int by = y1 < y2 ? y1 : y2;
    int bw = (x1 > x2 ? x1 - x2 : x2 - x1) + t;
    int bh = (y1 > y2 ? y1 - y2 : y2 - y1) + t;
    measure_track(bx - t / 2, by - t / 2, bw, bh);
    HPEN pen = CreatePen(PS_SOLID, t, win32_colorref(color));
    HGDIOBJ old = SelectObject(g_draw_hdc, pen);
    MoveToEx(g_draw_hdc, x1, y1, NULL);
    LineTo(g_draw_hdc, x2, y2);
    SelectObject(g_draw_hdc, old);
    DeleteObject(pen);
    { PROFILE_END_VAR(_pt, "Platform_DrawLineRaw"); return true; }
}

bool Platform_DrawCircleRaw(int cx, int cy, int radius, unsigned long color, float thickness) {
        PROFILE_START(_pt);
if (radius <= 0) { PROFILE_END_VAR(_pt, "Platform_DrawCircleRaw"); return false; }
    int t = win32_thickness_px(thickness);
    cx += PlDrawState_Get()->off_x;
    cy += PlDrawState_Get()->off_y;
    measure_track(cx - radius, cy - radius, radius * 2, radius * 2);
    int left = cx - radius, top = cy - radius;
    int right = cx + radius, bottom = cy + radius;
    if (t > 0) {
        HPEN pen = CreatePen(PS_SOLID, t, win32_colorref(color));
        HGDIOBJ old = SelectObject(g_draw_hdc, pen);
        HGDIOBJ old_br = SelectObject(g_draw_hdc, GetStockObject(NULL_BRUSH));
        Ellipse(g_draw_hdc, left, top, right, bottom);
        SelectObject(g_draw_hdc, old_br);
        SelectObject(g_draw_hdc, old);
        DeleteObject(pen);
    } else {
        HBRUSH br = CreateSolidBrush(win32_colorref(color));
        HGDIOBJ old = SelectObject(g_draw_hdc, br);
        HGDIOBJ old_pen = SelectObject(g_draw_hdc, GetStockObject(NULL_PEN));
        Ellipse(g_draw_hdc, left, top, right, bottom);
        SelectObject(g_draw_hdc, old_pen);
        SelectObject(g_draw_hdc, old);
        DeleteObject(br);
    }
    { PROFILE_END_VAR(_pt, "Platform_DrawCircleRaw"); return true; }
}

#define WIN32_IMG_CACHE_SLOTS 128
#define WIN32_IMG_REVALIDATE_MS 1000
#define WIN32_SRC_CACHE_SLOTS 32

typedef struct {
    char key[512];
    int w, h;
    FILETIME mtime;
    DWORD size;
    HDC screen;
    void* bmp;
    ULONGLONG stamp;
    DWORD checked_ms;
    int failed;
} Win32ImgSlot;

typedef struct {
    char key[512];
    FILETIME mtime;
    DWORD size;
    void* src;
    ULONGLONG stamp;
    DWORD checked_ms;
} Win32SrcSlot;

static Win32ImgSlot g_img_cache[WIN32_IMG_CACHE_SLOTS];
static Win32SrcSlot g_src_cache[WIN32_SRC_CACHE_SLOTS];
static ULONGLONG g_img_cache_tick = 0;
static CRITICAL_SECTION g_img_cache_cs;
static BOOL g_img_cache_cs_ready = FALSE;

#define WIN32_IMG_FAILED ((void*)1)

static ULONG_PTR g_gdip_token = 0;

static void win32_gdip_ensure(void) {
        PROFILE_START(_pt);
if (g_gdip_token != 0) { PROFILE_END_VAR(_pt, "win32_gdip_ensure"); return ; }
    GdiplusStartupInput gsi;
    gsi.GdiplusVersion = 1;
    gsi.DebugEventCallback = NULL;
    gsi.SuppressBackgroundThread = FALSE;
    gsi.SuppressExternalCodecs = FALSE;
    GdiplusStartup(&g_gdip_token, &gsi, NULL);
}

static void win32_gdip_dispose(void *img) {
        PROFILE_START(_pt);
if (img) GdipDisposeImage((GpImage *)img);
}

static HDC win32_screen_dc(void) {
        PROFILE_START(_pt);
static HDC dc = NULL;
    if (!dc) dc = GetDC(NULL);
    { PROFILE_END_VAR(_pt, "win32_screen_dc"); return dc; }
}

static void win32_img_cache_lock(void) {
        PROFILE_START(_pt);
if (!g_img_cache_cs_ready) {
        InitializeCriticalSection(&g_img_cache_cs);
        g_img_cache_cs_ready = TRUE;
    }
    EnterCriticalSection(&g_img_cache_cs);
}

static void win32_img_cache_unlock(void) {
        PROFILE_START(_pt);
if (g_img_cache_cs_ready) LeaveCriticalSection(&g_img_cache_cs);
}

static void win32_img_stat(const char *path, FILETIME *ft, DWORD *size) {
        PROFILE_START(_pt);
ft->dwLowDateTime = 0;
    ft->dwHighDateTime = 0;
    *size = 0;
    char native[MAX_PATH * 2];
    snprintf(native, sizeof(native), "%s", path);
    for (char *p = native; *p; p++) if (*p == '/') *p = '\\';
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(native, GetFileExInfoStandard, &fad)) { PROFILE_END_VAR(_pt, "win32_img_stat"); return ; }
    *ft = fad.ftLastWriteTime;
    *size = (fad.nFileSizeHigh << 16) | fad.nFileSizeLow;
}

static void* win32_img_cache_get(const char *path, HDC screen, int w, int h) {
        PROFILE_START(_pt);
    HBITMAP found = NULL;
    DWORD now = GetTickCount();
    win32_img_cache_lock();
    for (int i = 0; i < WIN32_IMG_CACHE_SLOTS; i++) {
        Win32ImgSlot *s = &g_img_cache[i];
        if (!s->bmp && !s->failed) continue;
        if (s->screen != screen || s->w != w || s->h != h) continue;
        if (strcmp(s->key, path) != 0) continue;
        if (now - s->checked_ms >= WIN32_IMG_REVALIDATE_MS) {
            FILETIME ft;
            DWORD size;
            win32_img_stat(path, &ft, &size);
            s->checked_ms = now;
            if (memcmp(&s->mtime, &ft, sizeof(ft)) != 0 || s->size != size) {
                win32_gdip_dispose(s->bmp);
                s->bmp = NULL;
                s->failed = 0;
                continue;
            }
        }
        s->stamp = ++g_img_cache_tick;
        found = s->bmp ? s->bmp : WIN32_IMG_FAILED;
        break;
    }
    win32_img_cache_unlock();
    { PROFILE_END_VAR(_pt, "win32_img_cache_get"); return found; }
}

static void win32_img_cache_put(const char *path, HDC screen, int w, int h, void* bmp) {
        PROFILE_START(_pt);
    FILETIME ft;
    DWORD size;
    win32_img_stat(path, &ft, &size);
    win32_img_cache_lock();
    int slot = -1;
    ULONGLONG oldest = ~(ULONGLONG)0;
    for (int i = 0; i < WIN32_IMG_CACHE_SLOTS; i++) {
        Win32ImgSlot *s = &g_img_cache[i];
        if (!s->bmp && !s->failed) { slot = i; break; }
        if (s->screen != screen || s->w != w || s->h != h || strcmp(s->key, path) != 0) continue;
        if (s->stamp < oldest) { oldest = s->stamp; slot = i; }
    }
    if (slot < 0) {
        for (int i = 0; i < WIN32_IMG_CACHE_SLOTS; i++) {
            Win32ImgSlot *s = &g_img_cache[i];
            if (s->screen != screen) continue;
            if (s->stamp < oldest) { oldest = s->stamp; slot = i; }
        }
    }
    if (slot < 0) slot = 0;
    Win32ImgSlot *s = &g_img_cache[slot];
    win32_gdip_dispose(s->bmp);
    snprintf(s->key, sizeof(s->key), "%s", path);
    s->w = w;
    s->h = h;
    s->mtime = ft;
    s->size = size;
    s->screen = screen;
    s->bmp = bmp;
    s->failed = (bmp == NULL) ? 1 : 0;
    s->checked_ms = GetTickCount();
    s->stamp = ++g_img_cache_tick;
    win32_img_cache_unlock();
}

static void* win32_src_cache_get(const char *path) {
    DWORD now = GetTickCount();
    void* found = NULL;
    win32_img_cache_lock();
    for (int i = 0; i < WIN32_SRC_CACHE_SLOTS; i++) {
        Win32SrcSlot *s = &g_src_cache[i];
        if (!s->src) continue;
        if (strcmp(s->key, path) != 0) continue;
        if (now - s->checked_ms >= WIN32_IMG_REVALIDATE_MS) {
            FILETIME ft;
            DWORD size;
            win32_img_stat(path, &ft, &size);
            s->checked_ms = now;
            if (memcmp(&s->mtime, &ft, sizeof(ft)) != 0 || s->size != size) {
                win32_gdip_dispose(s->src);
                s->src = NULL;
                continue;
            }
        }
        s->stamp = ++g_img_cache_tick;
        found = s->src;
        break;
    }
    win32_img_cache_unlock();
    return found;
}

static void win32_src_cache_put(const char *path, void* src) {
    if (!src) return;
    FILETIME ft;
    DWORD size;
    win32_img_stat(path, &ft, &size);
    win32_img_cache_lock();
    int slot = -1;
    ULONGLONG oldest = ~(ULONGLONG)0;
    for (int i = 0; i < WIN32_SRC_CACHE_SLOTS; i++) {
        Win32SrcSlot *s = &g_src_cache[i];
        if (!s->src) { slot = i; break; }
        if (s->stamp < oldest) { oldest = s->stamp; slot = i; }
    }
    if (slot < 0) slot = 0;
    Win32SrcSlot *s = &g_src_cache[slot];
    win32_gdip_dispose(s->src);
    snprintf(s->key, sizeof(s->key), "%s", path);
    s->mtime = ft;
    s->size = size;
    s->src = src;
    s->checked_ms = GetTickCount();
    s->stamp = ++g_img_cache_tick;
    win32_img_cache_unlock();
}

void win32_img_cache_free_all(void) {
        PROFILE_START(_pt);
win32_img_cache_lock();
    for (int i = 0; i < WIN32_IMG_CACHE_SLOTS; i++) {
        if (g_img_cache[i].bmp) { win32_gdip_dispose(g_img_cache[i].bmp); }
        memset(&g_img_cache[i], 0, sizeof(g_img_cache[i]));
    }
    for (int i = 0; i < WIN32_SRC_CACHE_SLOTS; i++) {
        if (g_src_cache[i].src) { win32_gdip_dispose(g_src_cache[i].src); }
        memset(&g_src_cache[i], 0, sizeof(g_src_cache[i]));
    }
    win32_img_cache_unlock();
}

static void win32_utf8_to_wide(const char *in, wchar_t *out, int out_max) {
        PROFILE_START(_pt);
if (!in || !out || out_max <= 0) { PROFILE_END_VAR(_pt, "win32_utf8_to_wide"); return ; }
    int n = MultiByteToWideChar(CP_UTF8, 0, in, -1, out, out_max);
    if (n <= 0) { out[0] = 0; return; }
}

static GpBitmap* win32_load_shell_icon_bitmap(const char *path) {
        PROFILE_START(_pt);
SHFILEINFOA sfi;
    ZeroMemory(&sfi, sizeof(sfi));
    if (!SHGetFileInfoA(path, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON)) { PROFILE_END_VAR(_pt, "win32_load_shell_icon_bitmap"); return NULL; }
    if (!sfi.hIcon) { PROFILE_END_VAR(_pt, "win32_load_shell_icon_bitmap"); return NULL; }
    GpBitmap* bmp = NULL;
    GpStatus st = GdipCreateBitmapFromHICON(sfi.hIcon, &bmp);
    DestroyIcon(sfi.hIcon);
    if (st != Ok || !bmp) { PROFILE_END_VAR(_pt, "win32_load_shell_icon_bitmap"); return NULL; }
    { PROFILE_END_VAR(_pt, "win32_load_shell_icon_bitmap"); return bmp; }
}

static void* win32_load_scaled_bitmap(const char *path, int w, int h) {
        PROFILE_START(_pt);
win32_gdip_ensure();
    if (g_gdip_token == 0) { log_error("img: GdiplusStartup failed"); return NULL; }
    if (w <= 0 || h <= 0) { PROFILE_END_VAR(_pt, "win32_load_scaled_bitmap"); return NULL; }

    GpBitmap *src = (GpBitmap *)win32_src_cache_get(path);
    if (!src) {
        char native[MAX_PATH * 2];
        snprintf(native, sizeof(native), "%s", path);
        for (char *p = native; *p; p++) if (*p == '/') *p = '\\';

        wchar_t wpath[MAX_PATH * 2];
        win32_utf8_to_wide(native, wpath, (int)(sizeof(wpath) / sizeof(wchar_t)));

        GpStatus _fs = GdipCreateBitmapFromFile(wpath, &src);
        if (_fs != Ok || !src) {
            if (src) GdipDisposeImage(src);
            src = win32_load_shell_icon_bitmap(native);
            if (!src) {
                log_error("img: cannot decode '%s' st=%d and no shell icon", path, (int)_fs);
                { PROFILE_END_VAR(_pt, "win32_load_scaled_bitmap"); return NULL; }
            }
        }
        win32_src_cache_put(path, src);
    }

    GpBitmap *out = NULL;
    if (GdipCreateBitmapFromScan0(w, h, w * 4, PixelFormat32bppPARGB, NULL, &out) != Ok || !out) {
        { PROFILE_END_VAR(_pt, "win32_load_scaled_bitmap"); return NULL; }
    }
    GpGraphics *g = NULL;
    GpStatus _gs = GdipGetImageGraphicsContext(out, &g);
    if (_gs == Ok && g) {
        GdipSetInterpolationMode(g, InterpolationModeHighQualityBicubic);
        GdipSetCompositingMode(g, CompositingModeSourceCopy);
        GdipSetSmoothingMode(g, SmoothingModeHighQuality);
        GdipDrawImageRectI(g, src, 0, 0, w, h);
        GdipDeleteGraphics(g);
    } else {
        log_error("img: ctx st=%d", (int)_gs);
    }
    { PROFILE_END_VAR(_pt, "win32_load_scaled_bitmap"); return out; }
}

static HDC s_cached_hdc = NULL;
static GpGraphics* s_cached_g = NULL;

void Platform_ReleaseDrawCache(void) {
    if (s_cached_g) {
        GdipDeleteGraphics(s_cached_g);
        s_cached_g = NULL;
    }
    s_cached_hdc = NULL;
}

bool Platform_DrawImageAt(const char *path, int x, int y, int w, int h) {
    PROFILE_START(_pt);
    PROFILE_START(timer);

    if (!path || !path[0]) { PROFILE_END_VAR(timer, "win32:Platform_DrawImageAt"); return false; }
    int _offx = PlDrawState_Get()->off_x;
    int _offy = PlDrawState_Get()->off_y;
    x += _offx;
    y += _offy;
    if (w <= 0) w = (IconSize > 0) ? IconSize : 16;
    if (h <= 0) h = (IconSize > 0) ? IconSize : 16;
    log_info("[IMGDBG][win32] path=%s in=%d,%d off=%d,%d final=%d,%d size=%dx%d IconSize=%d", path, x-_offx, y-_offy, _offx, _offy, x, y, w, h, IconSize);
    measure_track(x, y, w, h);

    if (!g_draw_hdc) { 
        log_error("img: no draw context (g_draw_hdc NULL)"); 
        PROFILE_END_VAR(timer, "win32:Platform_DrawImageAt"); 
        return false; 
    }

    HDC screen = win32_screen_dc();
    if (!screen) { 
        log_error("img: GetDC(NULL) failed"); 
        PROFILE_END_VAR(timer, "win32:Platform_DrawImageAt"); 
        return false; 
    }

    void* bmp = win32_img_cache_get(path, screen, w, h);
    if (bmp == WIN32_IMG_FAILED) {
        PROFILE_END_VAR(timer, "win32:Platform_DrawImageAt");
        { PROFILE_END_VAR(_pt, "Platform_DrawImageAt"); return false; }
    }
    if (!bmp) {
        bmp = win32_load_scaled_bitmap(path, w, h);
        win32_img_cache_put(path, screen, w, h, bmp);
    }
    if (!bmp) {
        PROFILE_END_VAR(timer, "win32:Platform_DrawImageAt");
        { PROFILE_END_VAR(_pt, "Platform_DrawImageAt"); return false; }
    }

    win32_gdip_ensure();
    if (g_gdip_token == 0) {
        log_error("img: Gdiplus unavailable");
        PROFILE_END_VAR(timer, "win32:Platform_DrawImageAt");
        { PROFILE_END_VAR(_pt, "Platform_DrawImageAt"); return false; }
    }

    if (s_cached_hdc != g_draw_hdc || !s_cached_g) {
        if (s_cached_g) {
            GdipDeleteGraphics(s_cached_g);
            s_cached_g = NULL;
        }
        s_cached_hdc = g_draw_hdc;
        if (GdipCreateFromHDC(g_draw_hdc, &s_cached_g) != Ok || !s_cached_g) {
            log_error("img: GdipCreateFromHDC failed for hdc=%p", (void *)g_draw_hdc);
            s_cached_hdc = NULL;
            s_cached_g = NULL;
            PROFILE_END_VAR(timer, "win32:Platform_DrawImageAt");
            { PROFILE_END_VAR(_pt, "Platform_DrawImageAt"); return false; }
        }
        GdipSetInterpolationMode(s_cached_g, InterpolationModeNearestNeighbor);
        GdipSetPixelOffsetMode(s_cached_g, PixelOffsetModeHalf);
        GdipSetCompositingQuality(s_cached_g, CompositingQualityHighSpeed);
        GdipSetSmoothingMode(s_cached_g, SmoothingModeHighSpeed);
    }

    GpStatus status = GdipDrawImageRectI(s_cached_g, (GpBitmap *)bmp, x, y, w, h);
    log_info("[IMGDBG][win32-draw] status=%d x=%d y=%d w=%d h=%d", (int)status, x, y, w, h);
    if (status != Ok) {
        log_error("img: GdipDrawImageRectI failed st=%d at %d,%d %dx%d", (int)status, x, y, w, h);
    }

    PROFILE_END_VAR(timer, "win32:Platform_DrawImageAt");
    { PROFILE_END_VAR(_pt, "Platform_DrawImageAt"); return (status == Ok); }
}

static bool win32_prompt_is_insert_mode(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "win32_prompt_is_insert_mode"); return g_current_mode == MODE_INSERT; }
}

static ColorRef win32_prompt_cursor_color(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "win32_prompt_cursor_color"); return g_current_mode == MODE_VISUAL ? COLOR_VISUAL_CURSOR : COLOR_CURSOR; }
}

void Platform_DrawListBackground(PromptState *state, HDC hdc, int y, int h, bool selected) {
    PROFILE_START(_pt);
    if (selected) {
        RECT rc = { 0, y, state->width, y + h };
        HBRUSH br = CreateSolidBrush(COLOR_SELECTED_TEXT);
        FillRect(hdc, &rc, br);
        DeleteObject(br);
    }
    PROFILE_END_VAR(_pt, "Platform_DrawListBackground");
}

void Platform_DrawPromptSeparator(int y, int width) {
        PROFILE_START(_pt);
    if (!EnableBorders || width <= 0) { PROFILE_END_VAR(_pt, "api:Platform_DrawPromptSeparator"); return; }
    Platform_DrawRectRaw(0, y, width, PROMPT_SEP_W,
                         rgb_to_hex(COLOR_BORDER), 0, false, true, 1, true);
    { PROFILE_END_VAR(_pt, "api:Platform_DrawPromptSeparator"); }
}

static void win32_prompt_editbox_spec(PromptState* state, FontHandle font, FlEditboxSpec* eb) {
    int text_len = (int)strlen(state->filter);
    int cursor_x = 0;
    int text_w = 0;
    int ascent = 0;
    int descent = 0;
    FontHandle mfont = font;
    HFONT told = NULL;
    if (!mfont.hfont) mfont = Platform_CreateFont(FontSize, UsedFont);
    if (mfont.hfont) told = (HFONT)SelectObject(g_draw_hdc, mfont.hfont);
    TEXTMETRIC tm;
    memset(&tm, 0, sizeof(tm));
    GetTextMetrics(g_draw_hdc, &tm);
    ascent = tm.tmAscent;
    descent = tm.tmDescent;
    SIZE szCur;
    GetTextExtentPoint32A(g_draw_hdc, state->filter, (int)state->cursor, &szCur);
    cursor_x = szCur.cx;
    if (text_len > 0) {
        SIZE szText;
        GetTextExtentPoint32A(g_draw_hdc, state->filter, text_len, &szText);
        text_w = szText.cx;
    }
    if (told) SelectObject(g_draw_hdc, told);
    int sel_start = 0, sel_end = 0;
    bool has_sel = false;
    Editbox_ResolveSelection(state, text_len, false, &sel_start, &sel_end, &has_sel);
    memset(eb, 0, sizeof(*eb));
    eb->text = state->filter;
    eb->text_len = text_len;
    eb->cursor = state->cursor;
    eb->sel_start = sel_start;
    eb->sel_end = sel_end;
    eb->has_selection = has_sel;
    eb->sel_mode = EDITBOX_SEL_INCLUSIVE_CARET;
    eb->insert_mode = win32_prompt_is_insert_mode();
    eb->x = 0;
    eb->y = PROMPT_EDIT_INSET;
    eb->w = state->width;
    eb->inset = PROMPT_EDIT_INSET;
    eb->visible_w = state->width - PROMPT_EDIT_SCROLL_PAD;
    eb->scroll = state->scroll_offset;
    eb->cursor_x = cursor_x;
    eb->text_w = text_w;
    eb->text_y = PROMPT_EDIT_INSET;
    eb->caret_y = Editbox_CaretY(PROMPT_EDIT_INSET, ascent, descent);
    eb->caret_h = PromptCaretHeight();
    eb->caret_w_normal = PromptNormalCaretWidth();
    eb->caret_w_insert = PROMPT_CARET_W_INSERT;
    eb->placeholder_dy = 2;
    eb->rainbow_limit_x = -1;
    eb->placeholder = state->placeholder;
    eb->text_color = rgb_to_hex(COLOR_TEXT);
    eb->font = font;
}

void Platform_DrawPromptEditText(PromptState* state) {
        PROFILE_START(_pt);
 PROFILE_START(timer);
    if (!state || !g_draw_hdc) { PROFILE_END_VAR(timer, "draw_win32:Platform_DrawPromptEditText"); return; }
    FontHandle font = Platform_CreateFont(FontSize, UsedFont);
    FlEditboxSpec eb;
    FlEditboxLayout lay;
    win32_prompt_editbox_spec(state, font, &eb);
    Editbox_Layout(&eb, &lay);
    state->scroll_offset = eb.scroll;
    Editbox_Draw(&eb, &lay, EDITBOX_PART_TINT | EDITBOX_PART_TEXT);
    Platform_DestroyFont(font);
    PROFILE_END_VAR(timer, "draw_win32:Platform_DrawPromptEditText");
}

void Platform_DrawPromptEditbox(PromptState* state) {
        PROFILE_START(_pt);
 PROFILE_START(timer);
    if (!state || !g_draw_hdc) { PROFILE_END_VAR(timer, "draw_win32:Platform_DrawPromptEditbox"); return; }
    FontHandle font = Platform_CreateFont(FontSize, UsedFont);
    FlEditboxSpec eb;
    FlEditboxLayout lay;
    win32_prompt_editbox_spec(state, font, &eb);
    Editbox_Layout(&eb, &lay);
    state->scroll_offset = eb.scroll;
    Editbox_Draw(&eb, &lay, EDITBOX_PART_ALL);
    Platform_DestroyFont(font);
    PROFILE_END_VAR(timer, "draw_win32:Platform_DrawPromptEditbox");
}

void Platform_DrawPromptEditCursor(PromptState* state) {
        PROFILE_START(_pt);
 PROFILE_START(timer);
    if (!state || !g_draw_hdc) { PROFILE_END_VAR(timer, "draw_win32:Platform_DrawPromptEditCursor"); return; }
    FontHandle font = Platform_CreateFont(FontSize, UsedFont);
    FlEditboxSpec eb;
    FlEditboxLayout lay;
    win32_prompt_editbox_spec(state, font, &eb);
    Editbox_Layout(&eb, &lay);
    Editbox_Draw(&eb, &lay, EDITBOX_PART_CARET);
    Platform_DestroyFont(font);
    PROFILE_END_VAR(timer, "draw_win32:Platform_DrawPromptEditCursor");
}
