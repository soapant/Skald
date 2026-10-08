#include "core/context.h"
#include "deps/log.h"
#include "core/window_manager.h"
#include "core/modal.h"
#include "config.h"
#include <stdlib.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#include <X11/extensions/shape.h>
#include <unistd.h>
#include <pthread.h>
#include <png.h>
#include <stdint.h>
#include "config.h"
#include "core/prompt_geometry.h"
#include "platform/platform.h"
#include "platform/draw.h"
#include "helpers/helpers_ui.h"
#include "platform/x11/audio_x11.h"
#include "core/timer.h"
#include "wm/niri_wm.h"
#define NANOSVG_IMPLEMENTATION
#include "deps/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "deps/nanosvgrast.h"

#define X11_SHADER_SMOOTHNESS 5

#define BG_SLOT_COUNT 4

typedef struct {
    bool used;
    Pixmap pm;
    Display* dpy;
    int w, h;
    unsigned long base;
    int preset, shaders, radius;
    float intensity, brightness, smoothness;
    bool rounded;
} BgSlot;

static __thread BgSlot g_bg_slots[BG_SLOT_COUNT];
static __thread int g_bg_slot_next = 0;
static __thread Visual* g_dblVisual = NULL;
static __thread Colormap g_dblColormap = None;
static __thread int g_dblDepth = 0;
static __thread Display* g_dblDpy = NULL;

void x11_set_dbl_visual(Display* dpy, Visual* vis, Colormap cmap, int depth) {
    PROFILE_START(timer);
    g_dblVisual = vis;
    g_dblColormap = cmap;
    g_dblDepth = depth;
    g_dblDpy = dpy;
    PROFILE_END_VAR(timer, "helpers_x11:x11_set_dbl_visual");
}
void x11_clear_dbl_visual(void) {
    PROFILE_START(timer);
    g_dblVisual = NULL;
    g_dblColormap = None;
    g_dblDepth = 0;
    g_dblDpy = NULL;
    PROFILE_END_VAR(timer, "helpers_x11:x11_clear_dbl_visual");
}

bool x11_is_own_window_name(const char* name) {
    if (!name) return false;
    return strncmp(name, "flint-", 6) == 0;
}
bool x11_get_drawable_visual(Display* dpy, Drawable d, Visual** visOut, Colormap* cmapOut, int* depthOut) {
    PROFILE_START(timer);
    XWindowAttributes attrs;
    if (XGetWindowAttributes(dpy, d, &attrs)) {
        if (visOut) *visOut = attrs.visual;
        if (cmapOut) *cmapOut = attrs.colormap;
        if (depthOut) *depthOut = attrs.depth;
        PROFILE_END_VAR(timer, "helpers_x11:x11_get_drawable_visual");
        return true;
    }
    if (g_dblVisual && g_dblDpy == dpy) {
        if (visOut) *visOut = g_dblVisual;
        if (cmapOut) *cmapOut = g_dblColormap;
        if (depthOut) *depthOut = g_dblDepth;
        PROFILE_END_VAR(timer, "helpers_x11:x11_get_drawable_visual");
        return true;
    }
    PROFILE_END_VAR(timer, "helpers_x11:x11_get_drawable_visual");
    return false;
}

static int xft_width(Display* dpy, XftFont* xf, const char* text, int len) {
    PROFILE_START(timer);
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, xf, (FcChar8*)text, len, &ext);
    PROFILE_END_VAR(timer, "helpers_x11:xft_width");
    return ext.xOff;
}

#define X11_FB_CACHE 24
typedef struct { Display* dpy; FcChar32 cp; int px; XftFont* f; } X11FbSlot;
static X11FbSlot g_fb_cache[X11_FB_CACHE];
static int g_fb_next = 0;
static FcConfig* g_fb_config = NULL;
static pthread_mutex_t g_fb_mtx = PTHREAD_MUTEX_INITIALIZER;

static int x11_utf8_cp(const unsigned char* s, const unsigned char* end, int* clen) {
    unsigned char c = *s;
    int n;
    FcChar32 cp;
    if (c < 0x80) { n = 1; cp = c; }
    else if ((c & 0xE0) == 0xC0) { n = 2; cp = c & 0x1Fu; }
    else if ((c & 0xF0) == 0xE0) { n = 3; cp = c & 0x0Fu; }
    else if ((c & 0xF8) == 0xF0) { n = 4; cp = c & 0x07u; }
    else { *clen = 1; return 0xFFFD; }
    if (s + n > end) { *clen = 1; return 0xFFFD; }
    for (int i = 1; i < n; i++) {
        if ((s[i] & 0xC0) != 0x80) { *clen = 1; return 0xFFFD; }
        cp = (cp << 6) | (FcChar32)(s[i] & 0x3F);
    }
    *clen = n;
    return cp;
}

static bool x11_font_has(XftFont* f, FcChar32 cp) {
    if (!f || !f->pattern) return false;
    if (cp < 0x21 || cp == 0x7F) return true;
    FcCharSet* cs = NULL;
    if (FcPatternGetCharSet(f->pattern, FC_CHARSET, 0, &cs) != FcResultMatch || !cs) return true;
    if (!FcCharSetHasChar(cs, cp)) return false;
    FT_Face face = XftLockFace(f);
    if (face) {
        bool ft = FcFreeTypeCharIndex(face, cp) != 0;
        XftUnlockFace(f);
        return ft;
    }
    return true;
}

static int x11_font_px(XftFont* xf, int px) {
    if (px > 0) return px;
    double d = 0;
    if (xf && xf->pattern && FcPatternGetDouble(xf->pattern, FC_PIXEL_SIZE, 0, &d) == FcResultMatch && d > 0)
        return (int)(d + 0.5);
    return FontSize;
}

static XftFont* x11_open_fallback_at_locked(Display* dpy, FcChar32 cp, int px) {
    XftFont* fb = NULL;
    FcCharSet* cs = FcCharSetCreate();
    FcPattern* pat = FcPatternCreate();
    if (cs && pat) {
        FcCharSetAddChar(cs, cp);
        FcPatternAddCharSet(pat, FC_CHARSET, cs);
        FcPatternAddString(pat, FC_FAMILY, (FcChar8*)"sans-serif");
        FcPatternAddDouble(pat, FC_PIXEL_SIZE, (double)px);
        FcConfigSubstitute(g_fb_config, pat, FcMatchPattern);
        FcDefaultSubstitute(pat);
        FcResult res = FcResultNoMatch;
        FcPattern* match = FcFontMatch(g_fb_config, pat, &res);
        if (match) {
            XLockDisplay(dpy);
            fb = XftFontOpenPattern(dpy, match);
            XUnlockDisplay(dpy);
            if (!fb) FcPatternDestroy(match);
        }
    }
    if (pat) FcPatternDestroy(pat);
    if (cs) FcCharSetDestroy(cs);
    return fb;
}

static XftFont* x11_open_fallback_at_locked(Display* dpy, FcChar32 cp, int px);

static XftFont* x11_fallback_for_locked(Display* dpy, FcChar32 cp, int px) {
    if (!g_fb_config) g_fb_config = FcInitLoadConfigAndFonts();
    if (!g_fb_config) return NULL;
    for (int i = 0; i < X11_FB_CACHE; i++) {
        if (g_fb_cache[i].f && g_fb_cache[i].dpy == dpy && g_fb_cache[i].cp == cp && g_fb_cache[i].px == px)
            return g_fb_cache[i].f;
    }
    XftFont* fb = x11_open_fallback_at_locked(dpy, cp, px > 0 ? px : FontSize);
    if (!fb) return NULL;
    if (g_fb_cache[g_fb_next].f) {
        Display* odpy = g_fb_cache[g_fb_next].dpy;
        XLockDisplay(odpy);
        XftFontClose(odpy, g_fb_cache[g_fb_next].f);
        XUnlockDisplay(odpy);
    }
    g_fb_cache[g_fb_next].dpy = dpy;
    g_fb_cache[g_fb_next].cp = cp;
    g_fb_cache[g_fb_next].px = px;
    g_fb_cache[g_fb_next].f = fb;
    g_fb_next = (g_fb_next + 1) % X11_FB_CACHE;
    return fb;
}

static XftFont* x11_fallback_for(Display* dpy, FcChar32 cp, int px) {
    pthread_mutex_lock(&g_fb_mtx);
    XftFont* f = x11_fallback_for_locked(dpy, cp, px);
    pthread_mutex_unlock(&g_fb_mtx);
    return f;
}

int x11_measure_utf8_fallback(Display* dpy, XftFont* xf, const char* s, int n, int px) {
    if (!xf || !s || n <= 0) return 0;
    px = x11_font_px(xf, px);
    const unsigned char* b = (const unsigned char*)s;
    const unsigned char* end = b + n;
    bool all = true;
    const unsigned char* p = b;
    while (p < end) { int cl = 1; FcChar32 cp = x11_utf8_cp(p, end, &cl); if (!x11_font_has(xf, cp)) { all = false; break; } p += cl; }
    if (all) return xft_width(dpy, xf, s, n);
    int total = 0;
    p = b;
    while (p < end) {
        const unsigned char* runStart = p;
        XftFont* runFont = xf;
        while (p < end) {
            int cl = 1;
            FcChar32 cp = x11_utf8_cp(p, end, &cl);
            if (!x11_font_has(runFont, cp)) break;
            p += cl;
        }
        if (p > runStart) total += xft_width(dpy, runFont, (const char*)runStart, (int)(p - runStart));
        if (p >= end) break;
        int cl = 1;
        FcChar32 cp = x11_utf8_cp(p, end, &cl);
        XftFont* fb = x11_fallback_for(dpy, cp, px);
        if (fb) {
            const unsigned char* fs = p;
            int covered = 0;
            while (p < end) { int l2 = 1; FcChar32 c2 = x11_utf8_cp(p, end, &l2); if (!x11_font_has(fb, c2)) break; p += l2; covered += l2; }
            if (covered > 0) total += xft_width(dpy, fb, (const char*)fs, covered);
            else p += cl;
        } else {
            p += cl;
        }
    }
    return total;
}

static double x11_ink_center(Display* dpy, XftFont* f, const char* t, int n) {
    XGlyphInfo e;
    XftTextExtentsUtf8(dpy, f, (FcChar8*)t, n, &e);
    return (double)e.yOff + (double)e.height / 2.0;
}

int x11_draw_utf8_fallback(Display* dpy, XftDraw* xd, XftColor* color, int x, int y, XftFont* xf, const char* s, int n, int px) {
    if (!xf || !xd || !s || n <= 0) return 0;
    px = x11_font_px(xf, px);
    const unsigned char* b = (const unsigned char*)s;
    const unsigned char* end = b + n;
    const unsigned char* p = b;
    int cx = x;
    while (p < end) {
        const unsigned char* runStart = p;
        XftFont* runFont = xf;
        while (p < end) {
            int cl = 1;
            FcChar32 cp = x11_utf8_cp(p, end, &cl);
            if (!x11_font_has(runFont, cp)) break;
            p += cl;
        }
        if (p > runStart) {
            int rl = (int)(p - runStart);
            XftDrawStringUtf8(xd, color, runFont, cx, y, (FcChar8*)runStart, rl);
            cx += xft_width(dpy, runFont, (const char*)runStart, rl);
        }
        if (p >= end) break;
        int cl = 1;
        FcChar32 cp = x11_utf8_cp(p, end, &cl);
        XftFont* fb = x11_fallback_for(dpy, cp, px);
        if (fb) {
            const unsigned char* fs = p;
            int covered = 0;
            while (p < end) { int l2 = 1; FcChar32 c2 = x11_utf8_cp(p, end, &l2); if (!x11_font_has(fb, c2)) break; p += l2; covered += l2; }
            if (covered > 0) {
                XftDrawStringUtf8(xd, color, fb, cx, y, (FcChar8*)fs, covered);
                cx += xft_width(dpy, fb, (const char*)fs, covered);
            } else {
                p += cl;
            }
        } else {
            p += cl;
        }
    }
    return cx - x;
}

void draw_wrapped_text(Display* dpy, XftDraw* xd, XftColor* color, FontHandle font, const char* text, int startY, char* centered_to, int xOffset, int boxWidth) {
    PROFILE_START(timer);
    if (!text) { PROFILE_END_VAR(timer, "helpers_x11:draw_wrapped_text"); return; }
    XftFont* xf = FH_TO_XFT(font);
    if (!xf) { PROFILE_END_VAR(timer, "helpers_x11:draw_wrapped_text"); return; }
    int lineHeight = xf->ascent + xf->descent;
    int spaceWidth = xft_width(dpy, xf, " ", 1);
    int y = startY + xf->ascent;
    int text_len = strlen(text);
    int maxWidth = 0;   
    int alignment = 0;

    if (centered_to != NULL) {
        if (Platform_StringContainsCaseInsensitive(centered_to, "CENTER")) {
            alignment = 1;
            maxWidth = boxWidth > 0 ? boxWidth : PROMPT_WRAP_BOX_CENTER;
        } else if (Platform_StringContainsCaseInsensitive(centered_to, "RIGHT")) {
            alignment = 2;
            maxWidth = boxWidth > 0 ? boxWidth : PROMPT_WRAP_BOX_RIGHT;
        } else {
            alignment = 0;
            maxWidth = FontSize * text_len;
        }
    } else {
        alignment = 0;
        maxWidth = FontSize * text_len;
    }

    const char* p = text;
    const char* lineStart = text;
    int lineWidth = 0;

    while (*p) {
        const char* word_start = p;
        while (*p && *p != ' ') p++;
        int wordWidth = x11_measure_utf8_fallback(dpy, xf, word_start, (int)(p - word_start), 0);
        int add = wordWidth + (lineWidth > 0 ? spaceWidth : 0);
        
        if (lineWidth > 0 && lineWidth + add > maxWidth) {
            int lw = x11_measure_utf8_fallback(dpy, xf, lineStart, (int)(word_start - lineStart), 0);
            
            int drawX = 0;
            if (alignment == 1)      drawX = boxWidth > 0 ? (maxWidth - lw) / 2 : (maxWidth - lw) / 2 + xOffset;
            else if (alignment == 2) drawX = boxWidth > 0 ? maxWidth - lw : maxWidth - lw - xOffset;
            else                     drawX = xOffset;
            
            x11_draw_utf8_fallback(dpy, xd, color, drawX, y, xf, lineStart, (int)(word_start - lineStart), 0);
            y += lineHeight;
            lineStart = word_start;
            lineWidth = wordWidth;
        } else {
            lineWidth += add;
        }
        while (*p == ' ') p++;
    }
    
    if (p > lineStart) {
        int lw = x11_measure_utf8_fallback(dpy, xf, lineStart, (int)(p - lineStart), 0);

        int drawX = 0;
        if (alignment == 1)      drawX = boxWidth > 0 ? (maxWidth - lw) / 2 : xOffset + (maxWidth - lw) / 2;
        else if (alignment == 2) drawX = boxWidth > 0 ? maxWidth - lw : maxWidth - lw - xOffset;
        else                     drawX = xOffset;

        x11_draw_utf8_fallback(dpy, xd, color, drawX, y, xf, lineStart, (int)(p - lineStart), 0);
    }
    PROFILE_END_VAR(timer, "helpers_x11:draw_wrapped_text");
}

void x11_xft_alloc_color(Display* dpy, Visual* vis, Colormap cmap, unsigned long hex, unsigned short alpha, XftColor* out) {
    PROFILE_START(timer);
    XRenderColor rc = {
        ((hex >> 16) & 0xFF) * 0x101,
        ((hex >> 8) & 0xFF) * 0x101,
        (hex & 0xFF) * 0x101,
        alpha
    };
    XftColorAllocValue(dpy, vis, cmap, &rc, out);
    PROFILE_END_VAR(timer, "helpers_x11:x11_xft_alloc_color");
}

int x11_update_edit_scroll_offset(Display* dpy, XftFont* xf, const char* text, int text_len, int cursor, int width, int prev_scroll, int* out_cursor_x) {
    PROFILE_START(timer);
    int visible_w = width - PROMPT_EDIT_SCROLL_PAD;
    int cursor_x = 0;
    int text_w = 0;
    if (xf) {
        XGlyphInfo cext;
        XftTextExtentsUtf8(dpy, xf, (FcChar8*)text, cursor, &cext);
        cursor_x = cext.xOff;
        XGlyphInfo textExt;
        XftTextExtentsUtf8(dpy, xf, (FcChar8*)text, text_len, &textExt);
        text_w = textExt.xOff;
    }
    int scroll = prev_scroll;
    int margin = 0;
    if (cursor_x - scroll > visible_w - margin) scroll = cursor_x - visible_w + margin;
    if (cursor_x - scroll < margin) scroll = cursor_x - margin;
    if (scroll < 0) scroll = 0;
    if (text_w - scroll < visible_w && scroll > 0) {
        if (text_w < visible_w) scroll = 0;
        else scroll = text_w - visible_w;
    }
    if (cursor_x - scroll < 0) scroll = cursor_x;
    if (cursor_x - scroll > visible_w) scroll = cursor_x - visible_w;
    *out_cursor_x = cursor_x;
    PROFILE_END_VAR(timer, "helpers_x11:x11_update_edit_scroll_offset");
    return scroll;
}

int x11_font_row_height(int font_size) {
    return font_size + PROMPT_ROW_H_BONUS;
}

int x11_font_list_top(int ascent, int descent, int font_size) {
    return font_size + PROMPT_LIST_TOP_BONUS;
}

int x11_font_row_height_of(FontHandle font) {
    XftFont* xf = FH_TO_XFT(font);
    return x11_font_row_height(FontSize);
}

int x11_font_list_top_of(FontHandle font) {
    XftFont* xf = FH_TO_XFT(font);
    return x11_font_list_top(xf ? xf->ascent : 0, xf ? xf->descent : 0, FontSize);
}

int x11_font_cursor_y(int ascent, int descent, int font_size, int box_top) {
    int line_h = ascent + descent;
    if (line_h <= 0 || font_size <= 0) return box_top;
    return box_top + (line_h - font_size) / 2;
}

int x11_font_cursor_y_of(FontHandle font, int box_top) {
    XftFont* xf = FH_TO_XFT(font);
    return x11_font_cursor_y(xf ? xf->ascent : 0, xf ? xf->descent : 0, FontSize, box_top);
}

void x11_draw_edit_cursor(XftDraw* xd, XftColor* color, int cursor_x, int scroll, int width) {
    PROFILE_START(timer);
    int cursor_draw_x = PROMPT_EDIT_INSET + cursor_x - scroll;
    if (cursor_draw_x > width - PROMPT_X11_DEADCURSOR_CLAMP) cursor_draw_x = width - PROMPT_X11_DEADCURSOR_CLAMP;
    int cursor_y = PROMPT_X11_DEADCURSOR_Y;
    if (IsInsertMode()) {
        XftDrawRect(xd, color, cursor_draw_x, cursor_y, PROMPT_CARET_W_INSERT, FontSize);
    } else {
        XftDrawRect(xd, color, cursor_draw_x, cursor_y, PROMPT_EDITPOPUP_CARET_W_NORMAL, FontSize);
    }
    PROFILE_END_VAR(timer, "helpers_x11:x11_draw_edit_cursor");
}

const char* ListBoxItemDisplayText(ListBoxInfo* lb, int idx, char* trunc, size_t trunc_size) {
    PROFILE_START(timer);
    int raw = (idx >= 0 && lb && idx < lb->populated_count) ? lb->matching_indices[idx] : -1;
    const char* baseText = lb ? ListBoxInfo_Row(lb, idx) : NULL;
    if (lb && lb->alias_suffix_active && raw >= 0 && raw < lb->alias_suffix_count && lb->alias_suffix[raw]) {
        snprintf(trunc, trunc_size, "%s [%s]", baseText ? baseText : "", lb->alias_suffix[raw]);
    } else {
        snprintf(trunc, trunc_size, "%.80s", baseText ? baseText : "");
    }
    PROFILE_END_VAR(timer, "helpers_x11:ListBoxItemDisplayText");
    return trunc;
}

#define IMG_CACHE_SLOTS 16

static bool x11_blit_argb(Display* dpy, Drawable drw, Visual* vis, unsigned char* argb, int w, int h, int x, int y);
static unsigned char* x11_decode_image_argb(const char* path, int* out_w, int* out_h);

typedef struct {
    char key[512];
    int w, h;
    long long mtime;
    long long size;
    unsigned char* argb;
    unsigned long stamp;
} ImgCacheSlot;

static ImgCacheSlot g_img_cache[IMG_CACHE_SLOTS];
static unsigned long g_img_cache_tick = 0;
static pthread_mutex_t g_img_cache_mtx = PTHREAD_MUTEX_INITIALIZER;

void x11_img_cache_free_all(void) {
    pthread_mutex_lock(&g_img_cache_mtx);
    for (int i = 0; i < IMG_CACHE_SLOTS; i++) {
        if (g_img_cache[i].argb) free(g_img_cache[i].argb);
        memset(&g_img_cache[i], 0, sizeof(g_img_cache[i]));
    }
    pthread_mutex_unlock(&g_img_cache_mtx);
}

static unsigned char* x11_img_cache_get(const char* key, int w, int h, long long mtime, long long size) {
    for (int i = 0; i < IMG_CACHE_SLOTS; i++) {
        ImgCacheSlot* s = &g_img_cache[i];
        if (!s->argb) continue;
        if (s->w == w && s->h == h && s->mtime == mtime && s->size == size && strcmp(s->key, key) == 0) {
            s->stamp = ++g_img_cache_tick;
            return s->argb;
        }
    }
    return NULL;
}

static void x11_img_cache_put(const char* key, int w, int h, long long mtime, long long size, unsigned char* argb) {
    if (!argb) return;
    int slot = -1;
    unsigned long oldest = ~0UL;
    for (int i = 0; i < IMG_CACHE_SLOTS; i++) {
        ImgCacheSlot* s = &g_img_cache[i];
        if (!s->argb) { slot = i; break; }
        if (s->stamp < oldest) { oldest = s->stamp; slot = i; }
    }
    if (slot < 0) slot = 0;
    ImgCacheSlot* s = &g_img_cache[slot];
    if (s->argb) free(s->argb);
    snprintf(s->key, sizeof(s->key), "%s", key);
    s->w = w;
    s->h = h;
    s->mtime = mtime;
    s->size = size;
    s->argb = argb;
    s->stamp = ++g_img_cache_tick;
}

bool Platform_DrawImageScaled(Display* dpy, XftDraw* xd, Visual* vis, const char* path, int x, int y, int tw, int th) {
    PROFILE_START(timer);
    if (!dpy || !xd || !path || !path[0]) { PROFILE_END_VAR(timer, "helpers_x11:Platform_DrawImageScaled"); return false; }
    struct stat st;
    if (stat(path, &st) != 0) { PROFILE_END_VAR(timer, "helpers_x11:Platform_DrawImageScaled"); return false; }
    if (tw <= 0) tw = IconSize;
    if (th <= 0) th = IconSize;
    int cw = tw;
    int ch = th;
    unsigned char* argb = NULL;
    pthread_mutex_lock(&g_img_cache_mtx);
    argb = x11_img_cache_get(path, cw, ch, (long long)st.st_mtime, (long long)st.st_size);
    if (argb) {
        pthread_mutex_unlock(&g_img_cache_mtx);
        Drawable drw = XftDrawDrawable(xd);
        bool ok = x11_blit_argb(dpy, drw, vis, argb, cw, ch, x, y);
        PROFILE_END_VAR(timer, "helpers_x11:Platform_DrawImageScaled");
        return ok;
    }
    pthread_mutex_unlock(&g_img_cache_mtx);
    argb = x11_decode_image_argb(path, &cw, &ch);
    if (!argb) { PROFILE_END_VAR(timer, "helpers_x11:Platform_DrawImageScaled"); return false; }
    pthread_mutex_lock(&g_img_cache_mtx);
    x11_img_cache_put(path, cw, ch, (long long)st.st_mtime, (long long)st.st_size, argb);
    pthread_mutex_unlock(&g_img_cache_mtx);
    Drawable drw = XftDrawDrawable(xd);
    bool ok = x11_blit_argb(dpy, drw, vis, argb, cw, ch, x, y);
    PROFILE_END_VAR(timer, "helpers_x11:Platform_DrawImageScaled");
    return ok;
}

static bool x11_blit_argb(Display* dpy, Drawable drw, Visual* vis, unsigned char* argb, int w, int h, int x, int y) {
    if (!dpy || !argb || w <= 0 || h <= 0) return false;
    if (!vis) vis = DefaultVisualOfScreen(DefaultScreenOfDisplay(dpy));
    XImage img;
    memset(&img, 0, sizeof(img));
    img.width = w;
    img.height = h;
    img.xoffset = 0;
    img.format = ZPixmap;
    img.data = (char*)argb;
    img.byte_order = ImageByteOrder(dpy);
    img.bitmap_unit = BitmapUnit(dpy);
    img.bitmap_bit_order = BitmapBitOrder(dpy);
    img.bitmap_pad = 32;
    img.depth = 32;
    img.bytes_per_line = w * 4;
    img.bits_per_pixel = 32;
    img.red_mask = vis->red_mask;
    img.green_mask = vis->green_mask;
    img.blue_mask = vis->blue_mask;
    img.obdata = NULL;
    if (!XInitImage(&img)) return false;
    Pixmap pix = XCreatePixmap(dpy, drw, w, h, 32);
    if (!pix) return false;
    GC gc = XCreateGC(dpy, pix, 0, NULL);
    XPutImage(dpy, pix, gc, &img, 0, 0, 0, 0, w, h);
    XFreeGC(dpy, gc);
    XRenderPictFormat* src_fmt = XRenderFindStandardFormat(dpy, PictStandardARGB32);
    XRenderPictFormat* dst_fmt = XRenderFindVisualFormat(dpy, vis);
    bool ok = false;
    if (src_fmt && dst_fmt) {
        Picture src = XRenderCreatePicture(dpy, pix, src_fmt, 0, NULL);
        Picture dst = XRenderCreatePicture(dpy, drw, dst_fmt, 0, NULL);
        if (src && dst) {
            XRenderComposite(dpy, PictOpOver, src, None, dst, 0, 0, 0, 0, x, y, w, h);
            ok = true;
        }
        if (src) XRenderFreePicture(dpy, src);
        if (dst) XRenderFreePicture(dpy, dst);
    }
    XFreePixmap(dpy, pix);
    return ok;
}

static unsigned char* x11_decode_image_argb(const char* path, int* out_w, int* out_h) {
    if (!path || !path[0]) return NULL;
    int dst_w = *out_w;
    int dst_h = *out_h;
    if (dst_w <= 0) dst_w = 16;
    if (dst_h <= 0) dst_h = 16;
    size_t _plen = strlen(path);
    bool _is_svg = _plen > 4 && Platform_StringContainsCaseInsensitive(path + _plen - 4, ".svg");
    if (_is_svg) {
        NSVGimage* image = nsvgParseFromFile(path, "px", 96.0f);
        if (!image) return false;
        if (image->width <= 0 || image->height <= 0) { nsvgDelete(image); return false; }
        NSVGrasterizer* rast = nsvgCreateRasterizer();
        if (!rast) { nsvgDelete(image); return false; }
        unsigned char* svg_data = (unsigned char*)calloc((size_t)dst_w * dst_h * 4, 1);
        if (!svg_data) { nsvgDeleteRasterizer(rast); nsvgDelete(image); return false; }
        float scale = fminf((float)dst_w / image->width, (float)dst_h / image->height);
        float tx = ((float)dst_w - image->width * scale) * 0.5f;
        float ty = ((float)dst_h - image->height * scale) * 0.5f;
        nsvgRasterize(rast, image, tx, ty, scale, svg_data, dst_w, dst_h, dst_w * 4);
        unsigned char* argb = (unsigned char*)malloc((size_t)dst_w * dst_h * 4);
        if (!argb) { free(svg_data); nsvgDeleteRasterizer(rast); nsvgDelete(image); return false; }
        for (int i = 0; i < dst_w * dst_h; i++) {
            unsigned char r = svg_data[i*4+0];
            unsigned char g = svg_data[i*4+1];
            unsigned char b = svg_data[i*4+2];
            unsigned char a = svg_data[i*4+3];
            unsigned int pr = (unsigned int)r * a / 255;
            unsigned int pg = (unsigned int)g * a / 255;
            unsigned int pb = (unsigned int)b * a / 255;
            argb[i*4+0] = (unsigned char)pb;
            argb[i*4+1] = (unsigned char)pg;
            argb[i*4+2] = (unsigned char)pr;
            argb[i*4+3] = a;
        }
        free(svg_data);
        nsvgDeleteRasterizer(rast);
        nsvgDelete(image);
        *out_w = dst_w;
        *out_h = dst_h;
        return argb;
    }

    FILE* fp = fopen(path, "rb");
    if (!fp) return false;

    unsigned char sig[8];
    if (fread(sig, 1, 8, fp) != 8 || png_sig_cmp(sig, 0, 8) != 0) {
        fclose(fp);
        return false;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(fp); return false; }

    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, NULL, NULL); fclose(fp); return false; }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return false;
    }

    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    int w = png_get_image_width(png, info);
    int h = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth  = png_get_bit_depth(png, info);

    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    if (!(color_type == PNG_COLOR_TYPE_RGB_ALPHA || color_type == PNG_COLOR_TYPE_GRAY_ALPHA))
        png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);

    png_read_update_info(png, info);

    int row_bytes = png_get_rowbytes(png, info);
    unsigned char* data = (unsigned char*)malloc((size_t)row_bytes * h);
    if (!data) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return false;
    }

    png_bytep* rows = (png_bytep*)malloc(sizeof(png_bytep) * h);
    if (!rows) {
        free(data);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return false;
    }
    for (int i = 0; i < h; i++) rows[i] = data + (size_t)i * row_bytes;

    png_read_image(png, rows);
    png_read_end(png, NULL);
    fclose(fp);
    png_destroy_read_struct(&png, &info, NULL);
    free(rows);

    unsigned char* argb = (unsigned char*)malloc((size_t)dst_w * dst_h * 4);
    if (!argb) { free(data); return false; }

    for (int dy = 0; dy < dst_h; dy++) {
        float y0f = (float)dy       * h / dst_h;
        float y1f = (float)(dy + 1) * h / dst_h;

        int y0 = (int)y0f;
        int y1 = (int)(y1f + 0.5f);
        if (y1 > h) y1 = h;
        if (y1 <= y0) y1 = y0 + 1;
        if (y0 >= h) y0 = h - 1;

        unsigned char* dst_row = argb + (size_t)dy * dst_w * 4;

        for (int dx = 0; dx < dst_w; dx++) {
            float x0f = (float)dx       * w / dst_w;
            float x1f = (float)(dx + 1) * w / dst_w;

            int x0 = (int)x0f;
            int x1 = (int)(x1f + 0.5f);
            if (x1 > w) x1 = w;
            if (x1 <= x0) x1 = x0 + 1;
            if (x0 >= w) x0 = w - 1;

            unsigned int sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;
            unsigned int count = 0;

            for (int sy = y0; sy < y1; sy++) {
                const unsigned char* src_row = data + (size_t)sy * row_bytes;
                for (int sx = x0; sx < x1; sx++) {
                    const unsigned char* s = src_row + (size_t)sx * 4;
                    sum_r += s[0];
                    sum_g += s[1];
                    sum_b += s[2];
                    sum_a += s[3];
                    count++;
                }
            }

            unsigned int r = sum_r / count;
            unsigned int g = sum_g / count;
            unsigned int b = sum_b / count;
            unsigned int a = sum_a / count;

            unsigned int pr = r * a / 255;
            unsigned int pg = g * a / 255;
            unsigned int pb = b * a / 255;

            unsigned char* d = dst_row + (size_t)dx * 4;
            d[0] = (unsigned char)pb;
            d[1] = (unsigned char)pg;
            d[2] = (unsigned char)pr;
            d[3] = (unsigned char)a;
        }
    }
    free(data);

    *out_w = dst_w;
    *out_h = dst_h;
    return argb;
}

void DrawListBoxItemText(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, FontHandle font,
                         XftColor* num_color, XftColor* body_color, XftColor* plain_color,
                         ListBoxInfo* lb,char* icon_path,int idx, int y, int text_x) {
    PROFILE_START(timer);
    XftFont* xf = FH_TO_XFT(font);
    if (!xf) { PROFILE_END_VAR(timer, "helpers_x11:DrawListBoxItemText"); return; }
    int XOffset = PROMPT_ITEM_TEXT_X;
    int row_h = x11_font_row_height_of(font);
    int pad_top = (row_h - (xf->ascent + xf->descent)) / 2;
    if (pad_top < 0) pad_top = 0;
    int text_y = y + pad_top;

    if (icon_path) {
        int iconX = XOffset;
        int iconY = y + (x11_font_row_height_of(font) - IconSize)/2;
        XOffset += IconSize + XOffset;
        Platform_DrawImageScaled(dpy, xd, vis, icon_path, iconX, iconY, IconSize, IconSize);
    }

    char trunc[512];
    ListBoxItemDisplayText(lb, idx, trunc, sizeof(trunc));

    char* alias_text = strstr(trunc, " [");

    if (alias_text) {
        int XOffsetAlias = 0;
        eval_xoffset_for_alias(alias_text,strlen(trunc),&XOffsetAlias);
        Platform_SetDrawContext(dpy, xd, vis, cmap);
        Platform_DrawTextRaw(XOffsetAlias, text_y, alias_text, rgb_to_hex(COLOR_ALIAS_TEXT), font, "LEFT", XOffsetAlias, FontSize);
    }

    char* pos = strstr(trunc, " [");
    if (pos) {
      *pos = 0;
    }
    unsigned long pcol=0; Platform_GetItemColors(idx, lb->populated_count, NULL, NULL, &pcol);
    Platform_SetDrawContext(dpy, xd, vis, cmap);
    Platform_DrawTextRaw(XOffset, text_y, trunc, pcol, font, "LEFT", XOffset, FontSize);

    PROFILE_END_VAR(timer, "helpers_x11:DrawListBoxItemText");
}

bool CursorBack(size_t* cursor, size_t text_len) {
    PROFILE_START(timer);
    if (*cursor == 0) { PROFILE_END_VAR(timer, "helpers_x11:CursorBack"); return false;  }
    (*cursor)--;
    PROFILE_END_VAR(timer, "helpers_x11:CursorBack");
    return true;
}

bool CursorForward(size_t* cursor, size_t text_len) {
    PROFILE_START(timer);
    if (*cursor >= text_len) { PROFILE_END_VAR(timer, "helpers_x11:CursorForward"); return false; }
    (*cursor)++;
    PROFILE_END_VAR(timer, "helpers_x11:CursorForward");
    return true;
}

static int x11_error_occurred = 0;

static int dummy_x_error_handler(Display *dpy, XErrorEvent *error) {
    PROFILE_START(timer);
    if (error->error_code == BadWindow) {
        x11_error_occurred = 1;
        PROFILE_END_VAR(timer, "helpers_x11:dummy_x_error_handler");
        return 0;
    }
    PROFILE_END_VAR(timer, "helpers_x11:dummy_x_error_handler");
    return 0;
}

pid_t GetWindowPid(Display* dpy, Window w) {
    PROFILE_START(timer);
    if (w == 0) { PROFILE_END_VAR(timer, "helpers_x11:GetWindowPid"); return -1; }

    Atom prop = XInternAtom(dpy, "_NET_WM_PID", True);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop_data = NULL;

    x11_error_occurred = 0;
    int (*old_handler)(Display*, XErrorEvent*) = XSetErrorHandler(dummy_x_error_handler);

    pid_t pid = -1;
    if (prop != None) {
        int status = XGetWindowProperty(dpy, w, prop, 0, 1, False, XA_CARDINAL,
                                        &actual_type, &actual_format, &nitems,
                                        &bytes_after, &prop_data);
        if (status == Success && !x11_error_occurred && prop_data && nitems > 0) {
            pid = *((pid_t*)prop_data);
        }
    }

    if (prop_data) {
        XFree(prop_data);
    }

    XSetErrorHandler(old_handler);

    if (x11_error_occurred) {
        PROFILE_END_VAR(timer, "helpers_x11:GetWindowPid");
        return -1;
    }

    PROFILE_END_VAR(timer, "helpers_x11:GetWindowPid");
    return pid;
}

void GetExeNameFromPid(pid_t pid, char* outExe, size_t maxLen) {
    PROFILE_START(timer);
    if (pid <= 0) {
        strncpy(outExe, "unknown", maxLen);
        PROFILE_END_VAR(timer, "helpers_x11:GetExeNameFromPid");
        return;
    }

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/exe", pid);
    
    ssize_t len = readlink(path, outExe, maxLen - 1);
    if (len != -1) {
        outExe[len] = '\0';
        char* basename = strrchr(outExe, '/');
        if (basename) {
            memmove(outExe, basename + 1, strlen(basename));
        }
    } else {
        strncpy(outExe, "unknown", maxLen);
    }
    PROFILE_END_VAR(timer, "helpers_x11:GetExeNameFromPid");
}

Window x11_create_window_ex_or(Display* dpy, int x, int y, int w, int h, unsigned long bg_color, const char* res_name, const char* res_class, long event_mask, bool override_redirect);

Window x11_create_window_ex(Display* dpy, int x, int y, int w, int h, unsigned long bg_color, const char* res_name, const char* res_class, long event_mask) {
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "helpers_x11:x11_create_window_ex");
    return x11_create_window_ex_or(dpy, x, y, w, h, bg_color, res_name, res_class, event_mask, false);
}

static void x11_apply_shape_rounded(Display* dpy, Window win, int w, int h, int radius);

static bool x11_has_compositor(Display* dpy, int screen) {
    char sel[32];
    snprintf(sel, sizeof(sel), "_NET_WM_CM_S%d", screen);
    Atom a = XInternAtom(dpy, sel, False);
    return XGetSelectionOwner(dpy, a) != None;
}

Window x11_create_window_ex_or(Display* dpy, int x, int y, int w, int h, unsigned long bg_color, const char* res_name, const char* res_class, long event_mask, bool override_redirect) {
    PROFILE_START(timer);
    int screen = DefaultScreen(dpy);
    XVisualInfo vinfo;
    Visual* visual = NULL;
    int depth = 0;
    Colormap cmap = 0;
    bool use_argb = false;
    static __thread Colormap cached_cmap = None;
    static __thread Visual* cached_visual = NULL;
    static __thread int cached_screen = -1;
    if (XMatchVisualInfo(dpy, screen, 32, TrueColor, &vinfo)) {
        visual = vinfo.visual;
        depth = 32;
        if (cached_cmap == None || cached_screen != screen || cached_visual != visual) {
            if (cached_cmap != None) XFreeColormap(dpy, cached_cmap);
            cached_cmap = XCreateColormap(dpy, RootWindow(dpy, screen), visual, AllocNone);
            cached_visual = visual;
            cached_screen = screen;
        }
        cmap = cached_cmap;
        use_argb = true;
    }
    Window win;
    if (use_argb) {
        XSetWindowAttributes attrs = {0};
        unsigned int r = (bg_color >> 16) & 0xFF;
        unsigned int g = (bg_color >> 8) & 0xFF;
        unsigned int b = bg_color & 0xFF;
        unsigned int a = (RoundedCorners || Platform_GetTransparentWindow()) ? 0 : (unsigned int)WindowOpacity;
        attrs.background_pixel = (a << 24) | (r << 16) | (g << 8) | b;
        attrs.colormap = cmap;
        attrs.border_pixel = 0;
        if (override_redirect) attrs.override_redirect = True;
        win = XCreateWindow(dpy, RootWindow(dpy, screen), x, y, w, h, 0, depth, InputOutput, visual,
                            CWBackPixel|CWColormap|CWBorderPixel|(override_redirect ? CWOverrideRedirect : 0), &attrs);
    } else {
        XSetWindowAttributes attrs = {0};
        if (override_redirect) attrs.override_redirect = True;
        win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), x, y, w, h, 0, bg_color, bg_color);
        if (override_redirect) XChangeWindowAttributes(dpy, win, CWOverrideRedirect, &attrs);
    }
    if (!override_redirect) {
        Atom wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
        Atom wm_type_dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
        XChangeProperty(dpy, win, wm_type, XA_ATOM, 32, PropModeReplace, (unsigned char*)&wm_type_dialog, 1);
        Atom motif_atom = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
        if (motif_atom != None) {
            struct { unsigned long flags; unsigned long functions; unsigned long decorations; long input_mode; unsigned long status; } motif;
            motif.flags = 2;
            motif.functions = 0;
            motif.decorations = 0;
            motif.input_mode = 0;
            motif.status = 0;
            XChangeProperty(dpy, win, motif_atom, motif_atom, 32, PropModeReplace, (unsigned char*)&motif, 5);
        }
        Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
        Atom state_skip_taskbar = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
        Atom state_skip_pager = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
        Atom states[2] = {state_skip_taskbar, state_skip_pager};
        XChangeProperty(dpy, win, wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char*)states, 2);
    } else if (res_name && strcmp(res_name, "flashtext")==0) {
        Atom wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
        Atom wm_type_note = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
        XChangeProperty(dpy, win, wm_type, XA_ATOM, 32, PropModeReplace, (unsigned char*)&wm_type_note, 1);
        Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
        Atom state_skip_taskbar = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
        Atom state_skip_pager = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
        Atom states[2] = {state_skip_taskbar, state_skip_pager};
        XChangeProperty(dpy, win, wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char*)states, 2);
    }
    if (res_name || res_class) {
        XClassHint* ch = XAllocClassHint();
        ch->res_name = (char*)(res_name ? res_name : "colored");
        ch->res_class = (char*)(res_class ? res_class : "colored");
        XSetClassHint(dpy, win, ch);
        XFree(ch);
    }
    XWMHints* hints = XAllocWMHints();
    hints->flags = InputHint;
    hints->input = True;
    if (res_name && strcmp(res_name, "flashtext")==0) hints->input = False;
    XSetWMHints(dpy, win, hints);
    XFree(hints);
    if (override_redirect && niri_is_active()) {
        XSizeHints* sh = XAllocSizeHints();
        sh->flags = PMinSize | PMaxSize;
        sh->min_width = w;
        sh->min_height = h;
        sh->max_width = w;
        sh->max_height = h;
        XSetWMNormalHints(dpy, win, sh);
        XFree(sh);
    }
    XSelectInput(dpy, win, event_mask);
    if (RoundedCorners && !override_redirect && CornerRadius > 0) {
        x11_apply_shape_rounded(dpy, win, w, h, CornerRadius);
    }
    PROFILE_END_VAR(timer, "helpers_x11:x11_create_window_ex_or");
    return win;
}

static void x11_apply_shape_rounded(Display* dpy, Window win, int w, int h, int radius) {
    PROFILE_START(timer);
    if (radius <= 0 || w <= 0 || h <= 0) { PROFILE_END_VAR(timer, "helpers_x11:x11_apply_shape_rounded"); return; }
    Pixmap mask = XCreatePixmap(dpy, win, w, h, 1);
    GC gc = XCreateGC(dpy, mask, 0, NULL);
    XSetForeground(dpy, gc, 0);
    XFillRectangle(dpy, mask, gc, 0, 0, w, h);
    XSetForeground(dpy, gc, 1);

    XFillRectangle(dpy, mask, gc, radius, 0, w - 2*radius, h);
    XFillRectangle(dpy, mask, gc, 0, radius, w, h - 2*radius);

    XFillArc(dpy, mask, gc, 0, 0, radius*2, radius*2, 0, 360*64);
    XFillArc(dpy, mask, gc, w - radius*2 -1, 0, radius*2, radius*2, 0, 360*64);
    XFillArc(dpy, mask, gc, 0, h - radius*2 -1, radius*2, radius*2, 0, 360*64);
    XFillArc(dpy, mask, gc, w - radius*2 -1, h - radius*2 -1, radius*2, radius*2, 0, 360*64);
    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
    XFreePixmap(dpy, mask);
    XFreeGC(dpy, gc);
    PROFILE_END_VAR(timer, "helpers_x11:x11_apply_shape_rounded");
}

void x11_fill_rounded_rect(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, unsigned long hex, unsigned short alpha16, int x, int y, int w, int h, int radius) {
    PROFILE_START(timer);
    if (!RoundedCorners || radius <= 0 || w <= 0 || h <= 0) {
        XftColor c;
        x11_xft_alloc_color(dpy, vis, cmap, hex, alpha16, &c);
        XftDrawRect(xd, &c, x, y, w, h);
        XftColorFree(dpy, vis, cmap, &c);
        PROFILE_END_VAR(timer, "helpers_x11:x11_fill_rounded_rect");
        return;
    }
    if (radius > w/2) radius = w/2;
    if (radius > h/2) radius = h/2;
    XftColor solid;
    x11_xft_alloc_color(dpy, vis, cmap, hex, alpha16, &solid);

    XftDrawRect(xd, &solid, x + radius, y, w - 2*radius, h);
    XftDrawRect(xd, &solid, x, y + radius, radius, h - 2*radius);
    XftDrawRect(xd, &solid, x + w - radius, y + radius, radius, h - 2*radius);

    const float edge_lo = (float)radius - 0.5f;
    const float edge_hi = (float)radius + 0.5f;
    const float edge_sq_lo = edge_lo * edge_lo;
    const float edge_sq_hi = edge_hi * edge_hi;

    for (int corner = 0; corner < 4; corner++) {
        int cx = (corner % 2 == 0) ? x + radius : x + w - radius - 1;
        int cy = (corner < 2) ? y + radius : y + h - radius - 1;
        int ox0 = (corner % 2 == 0) ? x : x + w - radius;
        int oy0 = (corner < 2) ? y : y + h - radius;
        for (int dy = 0; dy < radius; dy++) {
            for (int dx = 0; dx < radius; dx++) {
                float px = ox0 + dx + 0.5f;
                float py = oy0 + dy + 0.5f;
                float ddx2 = px - (cx + 0.5f);
                float ddy2 = py - (cy + 0.5f);
                float d_sq = ddx2*ddx2 + ddy2*ddy2;
                if (d_sq >= edge_sq_hi) continue;
                unsigned short a;
                if (d_sq <= edge_sq_lo) {
                    a = alpha16;
                } else {
                    float d = sqrtf(d_sq);
                    float cov = (edge_hi - d);
                    if (cov < 0.0f) cov = 0.0f;
                    if (cov > 1.0f) cov = 1.0f;
                    a = (unsigned short)((float)alpha16 * cov + 0.5f);
                }
                if (a == 0) continue;
                XftColor px_c = solid;
                px_c.color.alpha = a;
                XftDrawRect(xd, &px_c, ox0 + dx, oy0 + dy, 1, 1);
            }
        }
    }
    XftColorFree(dpy, vis, cmap, &solid);
    PROFILE_END_VAR(timer, "helpers_x11:x11_fill_rounded_rect");
}
static unsigned long LerpHex(unsigned long a, unsigned long b, float t) {
    return Color_Lerp(a, b, t);
}
static unsigned long DeriveLighter(unsigned long base, float intensity) {
    unsigned char r, g, b;
    Color_DeriveLighter((unsigned int)base, intensity, &r, &g, &b);
    return ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)b;
}
static unsigned int HashNoise(int x, int y) {
    return Color_HashNoise(x, y);
}
static unsigned int CornerAlpha(int x, int y, int w, int h, int R, unsigned int baseAlpha8) {
    float cx, cy;
    if (x < R && y < R) { cx = (float)R; cy = (float)R; }
    else if (x >= w - R && y < R) { cx = (float)(w - R - 1); cy = (float)R; }
    else if (x < R && y >= h - R) { cx = (float)R; cy = (float)(h - R - 1); }
    else if (x >= w - R && y >= h - R) { cx = (float)(w - R - 1); cy = (float)(h - R - 1); }
    else return baseAlpha8;

    const int SS = 4;
    int inside = 0;
    for (int sy = 0; sy < SS; sy++) {
        for (int sx = 0; sx < SS; sx++) {
            float px = (float)x + ((float)sx + 0.5f) / (float)SS;
            float py = (float)y + ((float)sy + 0.5f) / (float)SS;
            float dx = px - (cx + 0.5f);
            float dy = py - (cy + 0.5f);
            if (dx*dx + dy*dy <= (float)R * (float)R) inside++;
        }
    }
    const int total = SS * SS;
    if (inside <= 0) return 0;
    if (inside >= total) return baseAlpha8;
    return (unsigned int)((float)baseAlpha8 * ((float)inside / (float)total) + 0.5f);
}

void x11_free_cached_background(void) {
    PROFILE_START(timer);
    for (int i = 0; i < BG_SLOT_COUNT; i++) {
        if (g_bg_slots[i].used && g_bg_slots[i].pm != None && g_bg_slots[i].dpy) {
            XFreePixmap(g_bg_slots[i].dpy, g_bg_slots[i].pm);
        }
        g_bg_slots[i].used = false;
        g_bg_slots[i].pm = None;
        g_bg_slots[i].dpy = NULL;
    }
    g_bg_slot_next = 0;
    PROFILE_END_VAR(timer, "helpers_x11:x11_free_cached_background");
}

static void x11_generate_background_pixels(uint32_t* img_data, int bytes_per_line, int w, int h, int R, unsigned int baseAlpha8, unsigned long base, unsigned long fill) {
    uint32_t *row;
    unsigned int r = (base >> 16) & 0xFF, g = (base >> 8) & 0xFF, b = base & 0xFF;
    float _gamma = Color_ShaderGamma(X11_SHADER_SMOOTHNESS);
    #define _DITHER(x,y) (Color_BayerDither((x),(y)) * 2.5f)
    #define _APPLY_DITHER(cr,cg,cb,xx,yy) do { float _d = _DITHER(xx,yy); int _nr = (int)(cr + _d); int _ng = (int)(cg + _d); int _nb = (int)(cb + _d); if (_nr < 0) _nr = 0; if (_nr > 255) _nr = 255; if (_ng < 0) _ng = 0; if (_ng > 255) _ng = 255; if (_nb < 0) _nb = 0; if (_nb > 255) _nb = 255; cr = (unsigned int)_nr; cg = (unsigned int)_ng; cb = (unsigned int)_nb; } while(0)

    if (!EnableShaders || ShaderPreset == 0) {
        if (R <= 0) {
            row = (uint32_t*)img_data;
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) row[x] = fill;
                row = (uint32_t*)((char*)row + bytes_per_line);
            }
        } else {
            for (int y = 0; y < h; y++) {
                row = (uint32_t*)((char*)img_data + y * bytes_per_line);
                for (int x = 0; x < w; x++) {
                    unsigned int a = CornerAlpha(x, y, w, h, R, baseAlpha8);
                    row[x] = a ? (a << 24) | (r << 16) | (g << 8) | b : 0;
                }
            }
        }
    } else if (ShaderPreset == 1) {
        unsigned long light = DeriveLighter(base, 1.0f);
        float _bright = ShaderBrightness;
        for (int y = 0; y < h; y++) {
            float t = h > 1 ? (float)y / (float)(h - 1) : 0;
            t = powf(t, _gamma);
            unsigned long col = LerpHex(base, light, t * 0.9f * _bright);
            unsigned int cr0 = (col >> 16) & 0xFF, cg0 = (col >> 8) & 0xFF, cb0 = col & 0xFF;
            row = (uint32_t*)((char*)img_data + y * bytes_per_line);
            for (int x = 0; x < w; x++) {
                unsigned int cr = cr0, cg = cg0, cb = cb0;
                _APPLY_DITHER(cr,cg,cb,x,y);
                unsigned int a = CornerAlpha(x, y, w, h, R, baseAlpha8);
                row[x] = a ? (a << 24) | (cr << 16) | (cg << 8) | cb : 0;
            }
        }
    } else if (ShaderPreset == 2) {
        unsigned long light = DeriveLighter(base, 1.0f);
        float denom = (float)(w + h); if (denom < 1) denom = 1;
        for (int y = 0; y < h; y++) {
            row = (uint32_t*)((char*)img_data + y * bytes_per_line);
            for (int x = 0; x < w; x++) {
                float t = (float)(x + y) / denom;
                t = powf(t, _gamma);
                unsigned long col = LerpHex(base, light, t * 0.9f);
                unsigned int cr = (col >> 16) & 0xFF, cg = (col >> 8) & 0xFF, cb = col & 0xFF;
                _APPLY_DITHER(cr,cg,cb,x,y);
                unsigned int a = CornerAlpha(x, y, w, h, R, baseAlpha8);
                row[x] = a ? (a << 24) | (cr << 16) | (cg << 8) | cb : 0;
            }
        }
    } else if (ShaderPreset == 3) {
        unsigned long light = DeriveLighter(base, 1.0f);
        float cx = w * 0.5f, cy = h * 0.35f;
        float maxDist = sqrtf(cx*cx + cy*cy); if (maxDist < 1) maxDist = 1;
        for (int y = 0; y < h; y++) {
            row = (uint32_t*)((char*)img_data + y * bytes_per_line);
            for (int x = 0; x < w; x++) {
                float dx = x - cx, dy = y - cy;
                float t = sqrtf(dx*dx + dy*dy) / maxDist; if (t > 1) t = 1;
                t = powf(t, _gamma);
                unsigned long col = LerpHex(light, base, t * 0.85f);
                unsigned int cr = (col >> 16) & 0xFF, cg = (col >> 8) & 0xFF, cb = col & 0xFF;
                _APPLY_DITHER(cr,cg,cb,x,y);
                unsigned int a = CornerAlpha(x, y, w, h, R, baseAlpha8);
                row[x] = a ? (a << 24) | (cr << 16) | (cg << 8) | cb : 0;
            }
        }
    } else if (ShaderPreset == 4) {
        unsigned long light = DeriveLighter(base, 1.0f);
        for (int y = 0; y < h; y++) {
            float tBase = h > 1 ? (float)y / (float)(h - 1) : 0;
            tBase = powf(tBase, _gamma);
            row = (uint32_t*)((char*)img_data + y * bytes_per_line);
            for (int x = 0; x < w; x++) {
                float t = tBase * 0.9f;
                unsigned long col = LerpHex(base, light, t);
                unsigned int n = HashNoise(x, y);
                int delta = (int)(n & 0x1F) - 16;
                delta = (int)(delta * ShaderIntensity * 0.5f);
                int nr = (int)((col >> 16) & 0xFF) + delta;
                int ng = (int)((col >> 8) & 0xFF) + delta;
                int nb = (int)(col & 0xFF) + delta;
                float d = _DITHER(x,y);
                nr = (int)(nr + d); ng = (int)(ng + d); nb = (int)(nb + d);
                if (nr < 0) nr = 0; if (nr > 255) nr = 255;
                if (ng < 0) ng = 0; if (ng > 255) ng = 255;
                if (nb < 0) nb = 0; if (nb > 255) nb = 255;
                unsigned int a = CornerAlpha(x, y, w, h, R, baseAlpha8);
                row[x] = a ? ((a << 24) | ((unsigned int)nr << 16) | ((unsigned int)ng << 8) | (unsigned int)nb) : 0;
            }
        }
    } else {
        for (int y = 0; y < h; y++) {
            row = (uint32_t*)((char*)img_data + y * bytes_per_line);
            for (int x = 0; x < w; x++) {
                unsigned int a = CornerAlpha(x, y, w, h, R, baseAlpha8);
                row[x] = a ? (a << 24) | (r << 16) | (g << 8) | b : 0;
            }
        }
    }

    #undef _DITHER
    #undef _APPLY_DITHER
}

typedef struct {
    unsigned long base;
    int preset, shaders, radius;
    float intensity, brightness, smoothness;
    bool rounded;
    int w, h;
} BgKey;

static bool bg_slot_matches(const BgSlot* slot, const BgKey* k) {
    return slot->used && slot->dpy != NULL && slot->base == k->base
        && slot->preset == k->preset && slot->shaders == k->shaders && slot->radius == k->radius
        && slot->rounded == k->rounded && slot->intensity == k->intensity
        && slot->brightness == k->brightness && slot->smoothness == k->smoothness
        && slot->w == k->w && slot->h == k->h;
}

static Pixmap bg_cache_find(Display* dpy, const BgKey* k) {
    for (int i = 0; i < BG_SLOT_COUNT; i++) {
        if (g_bg_slots[i].dpy == dpy && bg_slot_matches(&g_bg_slots[i], k)) return g_bg_slots[i].pm;
    }
    return None;
}

static void bg_cache_store(Display* dpy, Pixmap pm, const BgKey* k) {
    BgSlot* slot = &g_bg_slots[g_bg_slot_next];
    g_bg_slot_next = (g_bg_slot_next + 1) % BG_SLOT_COUNT;
    if (slot->used && slot->pm != None) XFreePixmap(slot->dpy, slot->pm);
    slot->used = true;
    slot->pm = pm;
    slot->dpy = dpy;
    slot->w = k->w; slot->h = k->h; slot->base = k->base; slot->preset = k->preset;
    slot->shaders = k->shaders; slot->radius = k->radius; slot->rounded = k->rounded;
    slot->intensity = k->intensity; slot->brightness = k->brightness; slot->smoothness = k->smoothness;
}

static Pixmap x11_create_background_pixmap(Display* dpy, Window win, int w, int h, XWindowAttributes* attrs, unsigned long base, unsigned int baseAlpha8) {
    Visual* vis = attrs->visual;
    int depth = attrs->depth;
    if (depth != 32) return None;
    bool hasAlpha = true;
    bool rounded = RoundedCorners && CornerRadius > 0 && hasAlpha;
    int R = rounded ? CornerRadius : 0;
    if (R > w/2) R = w/2; if (R > h/2) R = h/2;
    Pixmap pm = XCreatePixmap(dpy, win, w, h, depth);
    if (!pm) return None;
    size_t bpl = (((size_t)w * (size_t)depth + 7u) / 8u + 3u) & ~(size_t)3u;
    char* pixbuf = (char*)calloc(1, bpl * (size_t)h);
    if (!pixbuf) { XFreePixmap(dpy, pm); return None; }
    XImage* img = XCreateImage(dpy, vis, depth, ZPixmap, 0, pixbuf, w, h, 32, (int)bpl);
    if (!img) { free(pixbuf); XFreePixmap(dpy, pm); return None; }

    unsigned int r = (base >> 16) & 0xFF, g = (base >> 8) & 0xFF, b = base & 0xFF;
    unsigned long fill = (baseAlpha8 << 24) | (r << 16) | (g << 8) | b;
    x11_generate_background_pixels((uint32_t*)img->data, img->bytes_per_line, w, h, R, baseAlpha8, base, fill);

    GC gc = XCreateGC(dpy, pm, 0, NULL);
    XPutImage(dpy, pm, gc, img, 0, 0, 0, 0, w, h);
    XFreeGC(dpy, gc);
    XDestroyImage(img);
    return pm;
}

Pixmap x11_get_cached_background(Display* dpy, Window win, int w, int h) {
    PROFILE_START(timer);
    if (!dpy || w <= 0 || h <= 0) { PROFILE_END_VAR(timer, "helpers_x11:x11_get_cached_background"); return None; }
    unsigned long base = Platform_GetDrawBgColor();
    if (!base) base = rgb_to_hex(COLOR_BG);
    int preset = ShaderPreset;
    float intensity = ShaderIntensity;
    float brightness = ShaderBrightness;
    int shaders = EnableShaders ? 1 : 0;
    float smoothness = 1;
    int radius = CornerRadius;
    bool rounded = RoundedCorners;
    if ((!EnableShaders || ShaderPreset == 0) && !(rounded && radius > 0)) {
        PROFILE_END_VAR(timer, "helpers_x11:x11_get_cached_background");
        return None;
    }
    BgKey k;
    k.base = base; k.preset = preset; k.shaders = shaders; k.radius = radius;
    k.rounded = rounded; k.intensity = intensity; k.brightness = brightness;
    k.smoothness = smoothness; k.w = w; k.h = h;
    Pixmap hit = bg_cache_find(dpy, &k);
    if (hit) {
        PROFILE_END_VAR(timer, "helpers_x11:x11_get_cached_background");
        return hit;
    }
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, win, &attrs)) { PROFILE_END_VAR(timer, "helpers_x11:x11_get_cached_background"); return None; }
    unsigned int baseAlpha8 = (unsigned int)WindowOpacity;
    if (baseAlpha8 > 255) baseAlpha8 = 255;
    Pixmap pm = x11_create_background_pixmap(dpy, win, w, h, &attrs, base, baseAlpha8);
    if (!pm) { PROFILE_END_VAR(timer, "helpers_x11:x11_get_cached_background"); return None; }
    bg_cache_store(dpy, pm, &k);
    PROFILE_END_VAR(timer, "helpers_x11:x11_get_cached_background");
    return pm;
}

void x11_draw_background_to_drawable(Display* dpy, Drawable drw, int w, int h) {
    if (!dpy || w <= 0 || h <= 0) return;
    unsigned long base = Platform_GetDrawBgColor();
    if (!base) base = rgb_to_hex(COLOR_BG);
    int preset = ShaderPreset;
    float intensity = ShaderIntensity;
    float brightness = ShaderBrightness;
    int shaders = EnableShaders ? 1 : 0;
    float smoothness = 1;
    int radius = CornerRadius;
    bool rounded = RoundedCorners;
    BgKey k;
    k.base = base; k.preset = preset; k.shaders = shaders; k.radius = radius;
    k.rounded = rounded; k.intensity = intensity; k.brightness = brightness;
    k.smoothness = smoothness; k.w = w; k.h = h;
    Pixmap cached = bg_cache_find(dpy, &k);
    if (cached != None) {
        GC gc = XCreateGC(dpy, drw, 0, NULL);
        XCopyArea(dpy, cached, drw, gc, 0, 0, w, h, 0, 0);
        XFreeGC(dpy, gc);
        return;
    }
    int screen = DefaultScreen(dpy);
    Visual* vis = DefaultVisual(dpy, screen);
    int depth = DefaultDepth(dpy, screen);
    Colormap cmap = DefaultColormap(dpy, screen);
    if (!x11_get_drawable_visual(dpy, drw, &vis, &cmap, &depth)) {
        vis = DefaultVisual(dpy, screen);
        depth = DefaultDepth(dpy, screen);
    }
bool hasAlpha = (depth == 32);
    bool rded = rounded && radius > 0 && hasAlpha;
    int R = rded ? radius : 0;
    if (R > w/2) R = w/2; if (R > h/2) R = h/2;
    unsigned int baseAlpha8 = (unsigned int)WindowOpacity;
    if (baseAlpha8 > 255) baseAlpha8 = 255;
    unsigned int cr0 = (base >> 16) & 0xFF, cg0 = (base >> 8) & 0xFF, cb0 = base & 0xFF;
    if (!hasAlpha) {
        unsigned long solid = (cr0 << 16) | (cg0 << 8) | cb0;
        GC sgc = XCreateGC(dpy, drw, 0, NULL);
        XSetForeground(dpy, sgc, solid);
        XFillRectangle(dpy, drw, sgc, 0, 0, w, h);
        XFreeGC(dpy, sgc);
        return;
    }
    size_t bpl = (((size_t)w * (size_t)depth + 7u) / 8u + 3u) & ~(size_t)3u;
    char* pixbuf = (char*)calloc(1, bpl * (size_t)h);
    if (!pixbuf) return;
    XImage* img = XCreateImage(dpy, vis, depth, ZPixmap, 0, pixbuf, w, h, 32, (int)bpl);
    if (!img) { free(pixbuf); return; }
    unsigned int r = cr0, g = cg0, b = cb0;
    unsigned long fill = (baseAlpha8 << 24) | (r << 16) | (g << 8) | b;
    GC gc = XCreateGC(dpy, drw, 0, NULL);
    x11_generate_background_pixels((uint32_t*)img->data, img->bytes_per_line, w, h, R, baseAlpha8, base, fill);
    XPutImage(dpy, drw, gc, img, 0, 0, 0, 0, w, h);
    XFreeGC(dpy, gc);
    XDestroyImage(img);
}

void x11_draw_background(Display* dpy, Window win, int w, int h) {
    PROFILE_START(timer);
    const char* presetName = "solid";
    if (!EnableShaders || ShaderPreset == 0) presetName = "solid";
    else if (ShaderPreset == 1) presetName = "vertical";
    else if (ShaderPreset == 2) presetName = "radial";
    else if (ShaderPreset == 3) presetName = "diagonal";
    else if (ShaderPreset == 4) presetName = "noise";
    else presetName = "solid-fallback";
    Pixmap bg = x11_get_cached_background(dpy, win, w, h);
    if (bg) {
        GC gc = XCreateGC(dpy, win, 0, NULL);
        XCopyArea(dpy, bg, win, gc, 0, 0, w, h, 0, 0);
        XFreeGC(dpy, gc);
    } else {
        x11_draw_background_to_drawable(dpy, win, w, h);
    }
    (void)presetName;
    PROFILE_END_VAR(timer, "helpers_x11:x11_draw_background");
}

void x11_draw_selection_background(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, int x, int y, int w, int h, unsigned long baseHex) {
    if (w <= 0 || h <= 0) return;
    XftColor c; x11_xft_alloc_color(dpy, vis, cmap, baseHex, OpacityAlpha16(), &c);
    XftDrawRect(xd, &c, x, y, w, h);
    XftColorFree(dpy, vis, cmap, &c);
}

Window x11_create_prompt_window(Display* dpy, int x, int y, int w, int h) {
    PROFILE_START(timer);
    unsigned long bg = rgb_to_hex(COLOR_BG);
    PROFILE_END_VAR(timer, "helpers_x11:x11_create_prompt_window");
    if (niri_is_active()) {
        log_error("niri override_redirect prompt w=%d h=%d", w, h);
        return x11_create_window_ex_or(dpy, x, y, w, h, bg, "colored", "colored", KeyPressMask | ExposureMask | FocusChangeMask, true);
    }
    Window win = x11_create_window_ex(dpy, x, y, w, h, bg, "colored", "colored", KeyPressMask | ExposureMask | FocusChangeMask);
    Window focus_win = None;
    int revert_to = 0;
    XGetInputFocus(dpy, &focus_win, &revert_to);
    if (focus_win != None && focus_win != PointerRoot && focus_win != win) XSetTransientForHint(dpy, win, focus_win);
    char unique_title[64];
    snprintf(unique_title, sizeof(unique_title), "flint-%lu-%d", (unsigned long)win, (int)getpid());
    XStoreName(dpy, win, unique_title);
    XSizeHints* sh = XAllocSizeHints();
    sh->flags = PMinSize | PMaxSize;
    sh->min_width = w;
    sh->min_height = h;
    sh->max_width = w;
    sh->max_height = h;
    XSetWMNormalHints(dpy, win, sh);
    XFree(sh);
    return win;
}
Window x11_create_edit_popup_window(Display* dpy, int x, int y, int w, int h) {
    PROFILE_START(timer);
    unsigned long bg = rgb_to_hex(COLOR_BG);
    PROFILE_END_VAR(timer, "helpers_x11:x11_create_edit_popup_window");
    if (niri_is_active()) {
        log_error("niri override_redirect edit w=%d h=%d", w, h);
        return x11_create_window_ex_or(dpy, x, y, w, h, bg, "colored", "colored", KeyPressMask | ExposureMask | FocusChangeMask, true);
    }
    Window win = x11_create_window_ex(dpy, x, y, w, h, bg, "colored", "colored", KeyPressMask | ExposureMask | FocusChangeMask);
    Window focus_win = None;
    int revert_to = 0;
    XGetInputFocus(dpy, &focus_win, &revert_to);
    if (focus_win != None && focus_win != PointerRoot && focus_win != win) XSetTransientForHint(dpy, win, focus_win);
    char unique_title[64];
    snprintf(unique_title, sizeof(unique_title), "flint-%lu-%d", (unsigned long)win, (int)getpid());
    XStoreName(dpy, win, unique_title);
    XSizeHints* sh = XAllocSizeHints();
    sh->flags = PMinSize | PMaxSize;
    sh->min_width = w;
    sh->min_height = h;
    sh->max_width = w;
    sh->max_height = h;
    XSetWMNormalHints(dpy, win, sh);
    XFree(sh);
    return win;
}
void x11_apply_click_through(Display* dpy, Window win) {
    PROFILE_START(timer);
    if (!dpy || win == None) { PROFILE_END_VAR(timer, "helpers_x11:x11_apply_click_through"); return; }
    XShapeCombineRectangles(dpy, win, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted);
    PROFILE_END_VAR(timer, "helpers_x11:x11_apply_click_through");
}

Window x11_create_display_message_window(Display* dpy, int x, int y, int w, int h, bool clickable) {
    PROFILE_START(timer);
    unsigned long bg = 0;
    if (!Platform_GetTransparentWindow()) {
        bg = Platform_GetDrawBgColor();
        if (!bg) bg = rgb_to_hex(COLOR_BG);
    }
    Platform_SetDrawContext(dpy, NULL, NULL, 0);
    long mask = ExposureMask | (clickable ? ButtonPressMask : 0L);
    Window win = x11_create_window_ex_or(dpy, x, y, w, h, bg, "flashtext", "flashtext", mask, true);
    if (win != None && !clickable) x11_apply_click_through(dpy, win);
    PROFILE_END_VAR(timer, "helpers_x11:x11_create_display_message_window");
    Platform_SetDrawContext(NULL,NULL,NULL,0);
    return win;
}

XftDraw* x11_create_xft_draw(Display* dpy, Window win) {
    PROFILE_START(timer);
    Visual* vis = NULL; Colormap cmap = 0; int depth = 0;
    if (x11_get_drawable_visual(dpy, win, &vis, &cmap, &depth)) {
        PROFILE_END_VAR(timer, "helpers_x11:x11_create_xft_draw");
        return XftDrawCreate(dpy, win, vis, cmap);
    }
    int screen = DefaultScreen(dpy);
    PROFILE_END_VAR(timer, "helpers_x11:x11_create_xft_draw");
    return XftDrawCreate(dpy, win, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));
}

gchar* FindPlayerBusName(GDBusConnection *conn) {
    GVariant *reply = g_dbus_connection_call_sync(conn,
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "ListNames", NULL, G_VARIANT_TYPE("(as)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
    
    gchar* player_name = NULL;
    if (reply) {
        GVariantIter *iter;
        const gchar *name;
        g_variant_get(reply, "(as)", &iter);
        while (g_variant_iter_next(iter, "&s", &name)) {
            if (g_str_has_prefix(name, "org.mpris.MediaPlayer2.")) {
                player_name = g_strdup(name);
                break;
            }
        }
        g_variant_iter_free(iter);
        g_variant_unref(reply);
    }
    return player_name;
}

void UpdateTextCaret(void) {
    PROFILE_START(timer);
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (top && top->vtable && top->vtable->on_render) top->vtable->on_render(top);
    PROFILE_END_VAR(timer, "helpers_x11:UpdateTextCaret");
}
