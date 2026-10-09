#include "platform/draw.h"
#include "core/prompt_geometry.h"
#include "core/modal.h"
#include "platform/platform.h"
#include "platform/win32/win32_state.h"
#include "config.h"
#include "platform/ui.h"
#include "core/context.h"
#include "helpers/helpers_ui.h"
#include "core/timer.h"
#include "deps/log.h"
#include <stdio.h>
#include <string.h>
#include <objidl.h>
#include <gdiplus/gdiplus.h>
#include <shlwapi.h>

#define WIN32_SIZE_FONT_SLOTS 8
static __thread HFONT g_sizeFonts[WIN32_SIZE_FONT_SLOTS] = {NULL};
static __thread int g_sizeFontSizes[WIN32_SIZE_FONT_SLOTS] = {0};

static wchar_t* win32_widen(const char* in, int in_len, int* out_len) {
    if (out_len) *out_len = 0;
    if (!in || in_len <= 0) return NULL;
    int n = MultiByteToWideChar(CP_UTF8, 0, in, in_len, NULL, 0);
    if (n <= 0) return NULL;
    wchar_t* w = (wchar_t*)malloc((size_t)n * sizeof(wchar_t) + sizeof(wchar_t));
    if (!w) return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, in, in_len, w, n) <= 0) { free(w); return NULL; }
    w[n] = 0;
    if (out_len) *out_len = n;
    return w;
}

static __thread HDC g_draw_hdc = NULL;

static HFONT win32_sized_font(int size);

static bool win32_text_is_symbolic(const wchar_t* wide, int len) {
    for (int i = 0; i < len; i++) {
        unsigned int c = (unsigned int)wide[i];
        if (c == 0xFE0F || c == 0xFE0E) continue;
        if (c > 0x7F) return true;
    }
    return false;
}

static bool win32_font_covers(HFONT f, const wchar_t* wide, int len) {
    if (!f || !wide || len <= 0) return false;
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
    return ok;
}

#define WIN32_SYM_SLOTS 4
static HFONT g_sym_fonts[WIN32_SYM_SLOTS] = {NULL};
static int g_sym_sizes[WIN32_SYM_SLOTS] = {0};
static wchar_t g_sym_cp[WIN32_SYM_SLOTS][8] = {{0}};
static int g_sym_len[WIN32_SYM_SLOTS] = {0};
static ULONGLONG g_sym_tick = 0;

static HFONT win32_symbol_font(int size, const wchar_t* wide, int len) {
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
    if (!found) return NULL;

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
    return found;
}

#define WIN32_SYMBOL_SIZE_NUM 5
#define WIN32_SYMBOL_SIZE_DEN 4

static bool win32_text_needs_boost(const wchar_t* wide, int len) {
    for (int i = 0; i < len; i++) {
        unsigned int c = (unsigned int)wide[i];
        if (c == 0xFE0F || c == 0xFE0E) continue;
        if (c >= 0x1F000) return true;
        if (c >= 0xD800 && c <= 0xDFFF) return true;
        if (c >= 0xE000 && c <= 0xF8FF) return true;
    }
    return false;
}

static HFONT win32_font_for_text(const wchar_t* wide, int len, int fontsize, HFONT* out_prev, int* out_dy) {
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
    return hfont;
}

static HFONT win32_sized_font(int size) {
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
  return g_sizeFonts[slot];
}

static __thread const char *g_prerender_text = NULL;
static __thread void *g_prerender_rect = NULL;
void Platform_SetPreRenderStateWin32(const char *text, void *rect) {
  g_prerender_text = text; g_prerender_rect = rect; 
}

bool Platform_PreRender(void) {
    if (!g_prerender_text || !g_prerender_rect) return false;
    return true;
}

__attribute__((weak)) bool fl_prerender(void) {
    return Platform_PreRender();
}

static __thread unsigned long g_draw_bg_override = 0;
static __thread bool g_transparent_window = false;

void Platform_SetTransparentWindow(bool on) {
    g_transparent_window = on;
}
bool Platform_GetTransparentWindow(void) {
    return g_transparent_window;
}

void Platform_SetDrawBgColor(unsigned long hex) {
    g_draw_bg_override = hex;
}
unsigned long Platform_GetDrawBgColor(void) {
    return g_draw_bg_override;
}

static __thread bool g_measuring = false;
static __thread bool g_meas_any = false;
static __thread int g_meas_origin_x = 0, g_meas_origin_y = 0;
static __thread int g_meas_x0 = 0, g_meas_y0 = 0, g_meas_x1 = 0, g_meas_y1 = 0;

static void measure_reset(void) {
    g_measuring = false;
    g_meas_any = false;
    g_meas_x0 = 0; g_meas_y0 = 0; g_meas_x1 = 0; g_meas_y1 = 0;
}
void Platform_BeginMeasure(void) {
    g_measuring = true;
    g_meas_x0 = 1 << 30; g_meas_y0 = 1 << 30;
    g_meas_x1 = -(1 << 30); g_meas_y1 = -(1 << 30);
    g_meas_any = false;
}
void Platform_EndMeasure(int *out_w, int *out_h) {
    int rw = 0, rh = 0;
    if (g_meas_any) {
        rw = (g_meas_x1 > g_meas_x0) ? (g_meas_x1 - g_meas_x0) : 0;
        rh = (g_meas_y1 > g_meas_y0) ? (g_meas_y1 - g_meas_y0) : 0;
    }
    if (out_w) *out_w = rw;
    if (out_h) *out_h = rh;
    g_meas_origin_x = g_meas_x0;
    g_meas_origin_y = g_meas_y0;
    measure_reset();
}
void Platform_GetLastMeasureOrigin(int *out_x, int *out_y) {
    if (out_x) *out_x = g_meas_origin_x;
    if (out_y) *out_y = g_meas_origin_y;
}
static __thread int g_draw_off_x = 0, g_draw_off_y = 0;
void Platform_SetDrawOffset(int dx, int dy) {
    g_draw_off_x = dx;
    g_draw_off_y = dy;
}
static void measure_track(int x, int y, int w, int h) {
    if (!g_measuring) return;
    if (x < g_meas_x0) g_meas_x0 = x;
    if (y < g_meas_y0) g_meas_y0 = y;
    if (x + w > g_meas_x1) g_meas_x1 = x + w;
    if (y + h > g_meas_y1) g_meas_y1 = y + h;
    g_meas_any = true;
}
void Platform_SetDrawContextWin32(HDC hdc) {
  g_draw_hdc = hdc; 
}
void Platform_GetDrawContextWin32(HDC *out) {
  if (out) *out = g_draw_hdc; 
}
bool Platform_DrawTextRaw(int x, int y, const char *text, unsigned long color, FontHandle font, const char *align, int pad,int fontsize) {
    if (!text || !g_draw_hdc) return false;
    x += g_draw_off_x;
    y += g_draw_off_y;
    if (color != 0) color = rgb_to_hex(color);
    if (color == 0) color = COLOR_TEXT;
    const char *a = align ? align : "CENTER";
    HFONT old = NULL;
    int tlen = (int)strlen(text);
    int wlen = 0;
    wchar_t* wide = win32_widen(text, tlen, &wlen);
    if (!wide) return false;
    int dy = 0;
    HFONT hfont = font.hfont ? font.hfont : win32_font_for_text(wide, wlen, fontsize, &old, &dy);
    if (!hfont) hfont = win32_sized_font(fontsize);
    if (!hfont) hfont = g_win32_hMainFont;
    if (!old && hfont) old = (HFONT)SelectObject(g_draw_hdc, hfont);
    int y_draw = y + dy;
    SetBkMode(g_draw_hdc, TRANSPARENT);
    SetTextColor(g_draw_hdc, color);
    bool centered = Platform_StringContainsCaseInsensitive(a,"CENTER");
    bool right = !centered && Platform_StringContainsCaseInsensitive(a,"RIGHT");
    SIZE msz;
    msz.cx = 0;
    msz.cy = 0;
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
    if (g_measuring) {
        int bx = x;
        if (centered) bx = x - msz.cx / 2;
        else if (right) bx = x - msz.cx;
        measure_track(bx, y, msz.cx, msz.cy);
    }
    if (!centered && !right) {
        int x0 = pad ? pad : x;
        if (g_measuring) measure_track(x0, y, msz.cx, msz.cy);
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
    return true;
}

int Platform_MeasureTextWidth(const char* text, int len, int fontsize) {
    if (!text || len <= 0) return 0;
    if (!g_draw_hdc) return len * (fontsize > 0 ? fontsize : 16) / 2;
    int wlen = 0;
    wchar_t* wide = win32_widen(text, len, &wlen);
    if (!wide) return 0;
    HFONT old = NULL;
    HFONT hfont = win32_font_for_text(wide, wlen, fontsize, &old, NULL);
    if (!hfont) hfont = win32_sized_font(fontsize);
    SIZE sz;
    sz.cx = 0;
    GetTextExtentPoint32W(g_draw_hdc, wide, wlen, &sz);
    if (old) SelectObject(g_draw_hdc, old);
    free(wide);
    return sz.cx;
}

bool Platform_DrawTextLine(int x, int y, const char* text, int len, unsigned long color, int fontsize) {
    if (!text || len <= 0 || !g_draw_hdc) return false;
    if (color != 0) color = rgb_to_hex(color);
    if (color == 0) color = COLOR_TEXT;
    int wlen = 0;
    wchar_t* wide = win32_widen(text, len, &wlen);
    if (!wide) return false;
    HFONT hfont = win32_sized_font(fontsize);
    HFONT old = hfont ? (HFONT)SelectObject(g_draw_hdc, hfont) : NULL;
    SetBkMode(g_draw_hdc, TRANSPARENT);
    SetTextColor(g_draw_hdc, color);
    TextOutW(g_draw_hdc, x, y, wide, wlen);
    if (old) SelectObject(g_draw_hdc, old);
    free(wide);
    return true;
}

void Platform_GetItemColors(int idx, int count, unsigned long *num_color, unsigned long *body_color, unsigned long *plain_color) {
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

bool Platform_DrawRectRaw(int x, int y, int w, int h, unsigned long color) {
    measure_track(x, y, w, h);
RECT rect;
  rect.left = x;
  rect.top = y;
  rect.right = x + w;
  rect.bottom = y + h;

  HBRUSH hBrush = CreateSolidBrush(color);

    FillRect(g_draw_hdc, &rect, hBrush);

    DeleteObject(hBrush);

    return true;
}

#define WIN32_IMG_CACHE_SLOTS 16

typedef struct {
    char key[512];
    int w, h;
    FILETIME mtime;
    DWORD size;
    HDC screen;
    void* bmp;
    ULONGLONG stamp;
} Win32ImgSlot;

static Win32ImgSlot g_img_cache[WIN32_IMG_CACHE_SLOTS];
static ULONGLONG g_img_cache_tick = 0;
static CRITICAL_SECTION g_img_cache_cs;
static BOOL g_img_cache_cs_ready = FALSE;

static ULONG_PTR g_gdip_token = 0;

static void win32_gdip_ensure(void) {
    if (g_gdip_token != 0) return;
    GdiplusStartupInput gsi;
    gsi.GdiplusVersion = 1;
    gsi.DebugEventCallback = NULL;
    gsi.SuppressBackgroundThread = FALSE;
    gsi.SuppressExternalCodecs = FALSE;
    GdiplusStartup(&g_gdip_token, &gsi, NULL);
}

static void win32_gdip_dispose(void *img) {
    if (img) GdipDisposeImage((GpImage *)img);
}

static HDC win32_screen_dc(void) {
    static HDC dc = NULL;
    if (!dc) dc = GetDC(NULL);
    return dc;
}

static void win32_img_cache_lock(void) {
    if (!g_img_cache_cs_ready) {
        InitializeCriticalSection(&g_img_cache_cs);
        g_img_cache_cs_ready = TRUE;
    }
    EnterCriticalSection(&g_img_cache_cs);
}

static void win32_img_cache_unlock(void) {
    if (g_img_cache_cs_ready) LeaveCriticalSection(&g_img_cache_cs);
}

static void win32_img_stat(const char *path, FILETIME *ft, DWORD *size) {
    ft->dwLowDateTime = 0;
    ft->dwHighDateTime = 0;
    *size = 0;
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad)) return;
    *ft = fad.ftLastWriteTime;
    *size = (fad.nFileSizeHigh << 16) | fad.nFileSizeLow;
}

static void* win32_img_cache_get(const char *path, HDC screen, int w, int h) {
    FILETIME ft;
    DWORD size;
    win32_img_stat(path, &ft, &size);
    HBITMAP found = NULL;
    win32_img_cache_lock();
    for (int i = 0; i < WIN32_IMG_CACHE_SLOTS; i++) {
        Win32ImgSlot *s = &g_img_cache[i];
        if (!s->bmp) continue;
        if (s->screen != screen || s->w != w || s->h != h) continue;
        if (memcmp(&s->mtime, &ft, sizeof(ft)) != 0 || s->size != size) continue;
        if (strcmp(s->key, path) != 0) continue;
        s->stamp = ++g_img_cache_tick;
        found = s->bmp;
        break;
    }
    win32_img_cache_unlock();
    return found;
}

static void win32_img_cache_put(const char *path, HDC screen, int w, int h, void* bmp) {
    if (!bmp) return;
    FILETIME ft;
    DWORD size;
    win32_img_stat(path, &ft, &size);
    win32_img_cache_lock();
    int slot = -1;
    ULONGLONG oldest = ~(ULONGLONG)0;
    for (int i = 0; i < WIN32_IMG_CACHE_SLOTS; i++) {
        Win32ImgSlot *s = &g_img_cache[i];
        if (!s->bmp) { slot = i; break; }
        if (s->screen != screen) continue;
        if (s->stamp < oldest) { oldest = s->stamp; slot = i; }
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
    s->stamp = ++g_img_cache_tick;
    win32_img_cache_unlock();
}

void win32_img_cache_free_all(void) {
    win32_img_cache_lock();
    for (int i = 0; i < WIN32_IMG_CACHE_SLOTS; i++) {
        if (g_img_cache[i].bmp) { win32_gdip_dispose(g_img_cache[i].bmp); }
        memset(&g_img_cache[i], 0, sizeof(g_img_cache[i]));
    }
    win32_img_cache_unlock();
}

static void win32_utf8_to_wide(const char *in, wchar_t *out, int out_max) {
    if (!in || !out || out_max <= 0) return;
    int n = MultiByteToWideChar(CP_UTF8, 0, in, -1, out, out_max);
    if (n <= 0) { out[0] = 0; return; }
}

static void* win32_load_scaled_bitmap(const char *path, int w, int h) {
    win32_gdip_ensure();
    if (g_gdip_token == 0) { log_error("img: GdiplusStartup failed"); return NULL; }

    wchar_t wpath[MAX_PATH * 2];
    win32_utf8_to_wide(path, wpath, (int)(sizeof(wpath) / sizeof(wchar_t)));

    GpBitmap *out = NULL;
    GpBitmap *src = NULL;
    GpStatus _fs = GdipCreateBitmapFromFile(wpath, &src);
    if (_fs != Ok || !src) {
        log_error("img: FromFile st=%d path=%s", (int)_fs, path);
        if (src) GdipDisposeImage(src);
        return NULL;
    }
    HDC sc = win32_screen_dc();
    void *raw = NULL;
    HDC mdc = sc ? CreateCompatibleDC(sc) : NULL;
    if (mdc) {
        BITMAPINFOHEADER bi;
        memset(&bi, 0, sizeof(bi));
        bi.biSize = sizeof(bi);
        bi.biWidth = w;
        bi.biHeight = -h;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;
        HBITMAP d = CreateDIBSection(mdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &raw, NULL, 0);
        GdipCreateBitmapFromHBITMAP(d, NULL, &out);
        if (d) DeleteObject(d);
        DeleteDC(mdc);
    }
    if (!out) {
        GdipDisposeImage(src);
        return NULL;
    }
    GpGraphics *g = NULL;
    GpStatus _gs = GdipGetImageGraphicsContext(out, &g);
    if (_gs == Ok && g) {
        GdipSetInterpolationMode(g, InterpolationModeHighQualityBicubic);
        GdipSetCompositingMode(g, CompositingModeSourceOver);
        GdipSetSmoothingMode(g, SmoothingModeHighQuality);
        GdipDrawImageRectI(g, src, 0, 0, w, h);
        GdipDeleteGraphics(g);
    } else {
        log_error("img: ctx st=%d", (int)_gs);
    }
    GdipDisposeImage(src);
    return out;
}

bool Platform_DrawImageAt(const char *path, int x, int y, int w, int h) {
    PROFILE_START(timer);
    if (!path || !path[0]) { PROFILE_END_VAR(timer, "win32:Platform_DrawImageAt"); return false; }
    if (w <= 0) w = IconSize;
    if (h <= 0) h = IconSize;
    if (w <= 0) w = 16;
    if (h <= 0) h = 16;
    measure_track(x, y, w, h);
    if (!g_draw_hdc) { log_error("img: no draw context (g_draw_hdc NULL)"); PROFILE_END_VAR(timer, "win32:Platform_DrawImageAt"); return false; }

    HDC screen = win32_screen_dc();
    if (!screen) { log_error("img: GetDC(NULL) failed"); PROFILE_END_VAR(timer, "win32:Platform_DrawImageAt"); return false; }

    void* bmp = win32_img_cache_get(path, screen, w, h);
    if (!bmp) {
        bmp = win32_load_scaled_bitmap(path, w, h);
        if (bmp) win32_img_cache_put(path, screen, w, h, bmp);
    }
    bool ok = false;
    if (bmp) {
        win32_gdip_ensure();
        if (g_gdip_token == 0) {
            log_error("img: Gdiplus unavailable");
        } else {
            GpGraphics *g = NULL;
            GpStatus _h2 = GdipCreateFromHDC(g_draw_hdc, &g);
            if (_h2 != Ok || !g) {
                log_error("img: GdipCreateFromHDC st=%d hdc=%p", (int)_h2, (void *)g_draw_hdc);
            } else {
                GpStatus _d2 = GdipDrawImageRectI(g, (GpBitmap *)bmp, x, y, w, h);
                if (_d2 != Ok) log_error("img: GdipDrawImageRectI st=%d at %d,%d %dx%d", (int)_d2, x, y, w, h);
                GdipDeleteGraphics(g);
                ok = true;
            }
        }
    }
    PROFILE_END_VAR(timer, "win32:Platform_DrawImageAt");
    return ok;
}

static int win32_update_edit_scroll_offset(HDC hdc, const char* text, int text_len, int cursor, int width, int prev_scroll, int* out_cursor_x) {
    int visible_w = width - PROMPT_EDIT_SCROLL_PAD;
    int cursor_x = 0;
    int text_w = 0;
    if (text) {
        if (cursor > 0) cursor_x = Platform_MeasureTextWidth(text, cursor, FontSize);
        if (text_len > 0) text_w = Platform_MeasureTextWidth(text, text_len, FontSize);
    }
    (void)hdc;
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
    if (out_cursor_x) *out_cursor_x = cursor_x;
    return scroll;
}

void Platform_DrawPromptEditText(PromptState* state) {
    PROFILE_START(timer);
    if (!state || !g_draw_hdc) { PROFILE_END_VAR(timer, "draw_win32:Platform_DrawPromptEditText"); return; }
    int cursor_x = 0;
    int text_len = (int)strlen(state->filter);
    int scroll = win32_update_edit_scroll_offset(g_draw_hdc, state->filter, text_len, (int)state->cursor, state->width, state->scroll_offset, &cursor_x);
    state->scroll_offset = scroll;
    if (text_len > 0) {
        FontHandle font = Platform_CreateFont(FontSize, UsedFont);
        Platform_DrawTextRaw(PROMPT_EDIT_INSET - scroll, PROMPT_EDIT_INSET, state->filter, rgb_to_hex(COLOR_TEXT), font, "", 0, 0);
        Platform_DestroyFont(font);
    }
    PROFILE_END_VAR(timer, "draw_win32:Platform_DrawPromptEditText");
}

void Platform_DrawPromptEditCursor(PromptState* state) {
    PROFILE_START(timer);
    if (!state || !g_draw_hdc) { PROFILE_END_VAR(timer, "draw_win32:Platform_DrawPromptEditCursor"); return; }
    int cursor_x = 0;
    win32_update_edit_scroll_offset(g_draw_hdc, state->filter, (int)strlen(state->filter), (int)state->cursor, state->width, state->scroll_offset, &cursor_x);
    int cursor_draw_x = PROMPT_EDIT_INSET + cursor_x - state->scroll_offset;
    if (cursor_draw_x < PROMPT_CARET_LO_CLAMP) cursor_draw_x = PROMPT_CARET_LO_CLAMP;
    if (cursor_draw_x > state->width - PROMPT_CARET_HI_CLAMP) cursor_draw_x = state->width - PROMPT_CARET_HI_CLAMP;
    FontHandle font = Platform_CreateFont(FontSize, UsedFont);
    HFONT oldFont = NULL;
    TEXTMETRIC tm;
    memset(&tm, 0, sizeof(tm));
    if (font.hfont) oldFont = (HFONT)SelectObject(g_draw_hdc, font.hfont);
    GetTextMetrics(g_draw_hdc, &tm);
    int line_h = tm.tmAscent + tm.tmDescent;
    int cursor_y = (line_h <= 0 || FontSize <= 0) ? PROMPT_EDIT_INSET : PROMPT_EDIT_INSET + (line_h - FontSize) / 2;
    if (oldFont) SelectObject(g_draw_hdc, oldFont);
    Platform_DestroyFont(font);
    if (IsInsertMode()) {
        Platform_DrawRectRaw(cursor_draw_x, cursor_y, PROMPT_CARET_W_INSERT, FontSize, Modal_CursorColor());
    } else {
        Platform_DrawRectRaw(cursor_draw_x, cursor_y, PromptNormalCaretWidth(), FontSize, Modal_CursorColor());
    }
    PROFILE_END_VAR(timer, "draw_win32:Platform_DrawPromptEditCursor");
}
