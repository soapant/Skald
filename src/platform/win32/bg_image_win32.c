#include "platform/win32/bg_image_win32.h"
#include "config.h"
#include "deps/log.h"
#include "core/timer.h"
#include <windows.h>
#include <shellapi.h>
#include <gdiplus/gdiplus.h>
#include <stdio.h>
#include <string.h>

#define BG_SRC_SLOTS      2
#define BG_TILE_SLOTS     4
#define BG_REVALIDATE_MS  1000
#define BG_RETRY_MS       2000

typedef struct {
    char      path[512];
    FILETIME  mtime;
    DWORD     size;
    int       src_w;
    int       src_h;
    void*     src;
    ULONGLONG stamp;
    DWORD     checked_ms;
    bool      failed;
} BgSrcSlot;

typedef struct {
    HBITMAP   bmp;
    HDC       dc;
    int       w;
    int       h;
    char      path[512];
    FILETIME  mtime;
    DWORD     size;
    ULONGLONG stamp;
    DWORD     checked_ms;
} BgTileSlot;

static BgSrcSlot  g_bg_srcs[BG_SRC_SLOTS];
static BgTileSlot g_bg_tiles[BG_TILE_SLOTS];
static ULONGLONG g_bg_tick = 0;
static CRITICAL_SECTION g_bg_cs;
static BOOL g_bg_cs_ready = FALSE;
static ULONG_PTR g_bg_gdip_token = 0;

static void bg_lock(void) {
    PROFILE_START(_pt);
if (!g_bg_cs_ready) {
        InitializeCriticalSection(&g_bg_cs);
        g_bg_cs_ready = TRUE;
    }
    EnterCriticalSection(&g_bg_cs);
    { PROFILE_END_VAR(_pt, "bg_lock"); }
}

static void bg_unlock(void) {
    PROFILE_START(_pt);
if (g_bg_cs_ready) LeaveCriticalSection(&g_bg_cs);
    { PROFILE_END_VAR(_pt, "bg_unlock"); }
}

static void bg_gdip_ensure(void) {
    PROFILE_START(_pt);
if (g_bg_gdip_token != 0) { PROFILE_END_VAR(_pt, "bg_gdip_ensure"); return; }
    GdiplusStartupInput gsi;
    gsi.GdiplusVersion = 1;
    gsi.DebugEventCallback = NULL;
    gsi.SuppressBackgroundThread = FALSE;
    gsi.SuppressExternalCodecs = FALSE;
    GdiplusStartup(&g_bg_gdip_token, &gsi, NULL);
    { PROFILE_END_VAR(_pt, "bg_gdip_ensure"); }
}

static void bg_native_path(char* out, size_t cap, const char* path) {
    PROFILE_START(_pt);
if (!out || cap == 0) { PROFILE_END_VAR(_pt, "bg_native_path"); return; }
    snprintf(out, cap, "%s", path ? path : "");
    for (char* q = out; *q; q++) if (*q == '/') *q = '\\';
    { PROFILE_END_VAR(_pt, "bg_native_path"); }
}

static void bg_stat(const char* native, FILETIME* ft, DWORD* size) {
    PROFILE_START(_pt);
ft->dwLowDateTime = 0;
    ft->dwHighDateTime = 0;
    *size = 0;
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(native, GetFileExInfoStandard, &fad)) { PROFILE_END_VAR(_pt, "bg_stat"); return; }
    *ft = fad.ftLastWriteTime;
    *size = (fad.nFileSizeHigh << 16) | (fad.nFileSizeLow);
    { PROFILE_END_VAR(_pt, "bg_stat"); }
}

static void bg_widen(const char* in, wchar_t* out, int out_max) {
    PROFILE_START(_pt);
out[0] = 0;
    if (!in || !in[0] || out_max <= 0) { PROFILE_END_VAR(_pt, "bg_widen"); return; }
    if (MultiByteToWideChar(CP_UTF8, 0, in, -1, out, out_max) <= 0) out[0] = 0;
    { PROFILE_END_VAR(_pt, "bg_widen"); }
}

static void* bg_shell_icon_bitmap(const char* native) {
    PROFILE_START(_pt);
SHFILEINFOA sfi;
    ZeroMemory(&sfi, sizeof(sfi));
    if (!SHGetFileInfoA(native, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES)) { PROFILE_END_VAR(_pt, "bg_shell_icon_bitmap"); return NULL; }
    if (!sfi.hIcon) { PROFILE_END_VAR(_pt, "bg_shell_icon_bitmap"); return NULL; }
    GpBitmap* bmp = NULL;
    GpStatus st = GdipCreateBitmapFromHICON(sfi.hIcon, &bmp);
    DestroyIcon(sfi.hIcon);
    if (st != Ok || !bmp) { PROFILE_END_VAR(_pt, "bg_shell_icon_bitmap"); return NULL; }
    { PROFILE_END_VAR(_pt, "bg_shell_icon_bitmap"); return bmp; }
}

static void* bg_load_native(const char* native, int* out_w, int* out_h) {
    PROFILE_START(_pt);
bg_gdip_ensure();
    if (g_bg_gdip_token == 0) { PROFILE_END_VAR(_pt, "bg_load_native"); return NULL; }
    wchar_t wpath[1024];
    bg_widen(native, wpath, (int)(sizeof(wpath) / sizeof(wchar_t)));
    GpBitmap* bmp = NULL;
    GpStatus st = wpath[0] ? GdipCreateBitmapFromFile(wpath, &bmp) : InvalidParameter;
    if (st != Ok || !bmp) {
        if (bmp) GdipDisposeImage((GpImage*)bmp);
        bmp = (GpBitmap*)bg_shell_icon_bitmap(native);
        if (!bmp) {
            log_error("bg_image: cannot decode '%s' st=%d", native, (int)st);
            fprintf(stderr, "[bg_image] DECODE FAILED path='%s' status=%d\n", native, (int)st);
            PROFILE_END_VAR(_pt, "bg_load_native");
            return NULL;
        }
    }
    UINT gw = 0, gh = 0;
    if (GdipGetImageWidth((GpImage*)bmp, &gw) != Ok || GdipGetImageHeight((GpImage*)bmp, &gh) != Ok || gw == 0 || gh == 0) {
        fprintf(stderr, "[bg_image] bad image dimensions in '%s'\n", native);
        GdipDisposeImage((GpImage*)bmp);
        PROFILE_END_VAR(_pt, "bg_load_native");
        return NULL;
    }
    if (out_w) *out_w = (int)gw;
    if (out_h) *out_h = (int)gh;
    { PROFILE_END_VAR(_pt, "bg_load_native"); return bmp; }
}

static void bg_src_drop(BgSrcSlot* s) {
    PROFILE_START(_pt);
if (!s) { PROFILE_END_VAR(_pt, "bg_src_drop"); return; }
    if (s->src) GdipDisposeImage((GpImage*)s->src);
    memset(s, 0, sizeof(*s));
    { PROFILE_END_VAR(_pt, "bg_src_drop"); }
}

static void bg_tile_drop(BgTileSlot* t) {
    PROFILE_START(_pt);
if (!t) { PROFILE_END_VAR(_pt, "bg_tile_drop"); return; }
    if (t->dc) { DeleteDC(t->dc); t->dc = NULL; }
    if (t->bmp) { DeleteObject(t->bmp); t->bmp = NULL; }
    memset(t, 0, sizeof(*t));
    { PROFILE_END_VAR(_pt, "bg_tile_drop"); }
}

void win32_bg_image_purge(void) {
    PROFILE_START(_pt);
bg_lock();
    for (int i = 0; i < BG_SRC_SLOTS; i++) bg_src_drop(&g_bg_srcs[i]);
    for (int i = 0; i < BG_TILE_SLOTS; i++) bg_tile_drop(&g_bg_tiles[i]);
    bg_unlock();
    { PROFILE_END_VAR(_pt, "win32_bg_image_purge"); }
}

static BgSrcSlot* bg_src_get(const char* native) {
    PROFILE_START(_pt);
DWORD now = GetTickCount();
    BgSrcSlot* slot = NULL;
    for (int i = 0; i < BG_SRC_SLOTS; i++) {
        BgSrcSlot* s = &g_bg_srcs[i];
        if (s->path[0] && strcmp(s->path, native) != 0) continue;
        if (strcmp(s->path, native) != 0) { slot = s; break; }
        FILETIME ft;
        DWORD sz = 0;
        bg_stat(native, &ft, &sz);
        if (memcmp(&s->mtime, &ft, sizeof(ft)) != 0 || s->size != sz) {
            bg_src_drop(s);
            slot = s;
            break;
        }
        if (s->src) { s->stamp = ++g_bg_tick; PROFILE_END_VAR(_pt, "bg_src_get"); return s; }
        if (s->failed && now - s->checked_ms < BG_RETRY_MS) { PROFILE_END_VAR(_pt, "bg_src_get"); return NULL; }
        slot = s;
        break;
    }
    if (!slot) {
        for (int i = 0; i < BG_SRC_SLOTS; i++) {
            if (g_bg_srcs[i].path[0] == 0) { slot = &g_bg_srcs[i]; break; }
        }
    }
    if (!slot) {
        int oldest = 0;
        for (int i = 1; i < BG_SRC_SLOTS; i++) if (g_bg_srcs[i].stamp < g_bg_srcs[oldest].stamp) oldest = i;
        slot = &g_bg_srcs[oldest];
        bg_src_drop(slot);
    }
    bg_src_drop(slot);
    void* bmp = bg_load_native(native, &slot->src_w, &slot->src_h);
    snprintf(slot->path, sizeof(slot->path), "%s", native);
    slot->checked_ms = now;
    slot->stamp = ++g_bg_tick;
    if (!bmp) {
        slot->failed = true;
        PROFILE_END_VAR(_pt, "bg_src_get");
        return NULL;
    }
    FILETIME ft;
    DWORD sz = 0;
    bg_stat(native, &ft, &sz);
    slot->mtime = ft;
    slot->size = sz;
    slot->src = bmp;
    { PROFILE_END_VAR(_pt, "bg_src_get"); return slot; }
}

static BgTileSlot* bg_tile_compose(HDC refDC, const char* native, int w, int h, FILETIME ft, DWORD sz) {
    PROFILE_START(_pt);
PROFILE_START(timer);
    BgSrcSlot* s = bg_src_get(native);
    if (!s || !s->src) { PROFILE_END_VAR(timer, "bg_tile_compose"); return NULL; }
    int sw = s->src_w;
    int sh = s->src_h;
    double scale = (double)w / (double)sw;
    double scale_y = (double)h / (double)sh;
    if (scale_y > scale) scale = scale_y;
    if (scale <= 0.0) { PROFILE_END_VAR(timer, "bg_tile_compose"); return NULL; }
    int dw = (int)((double)sw * scale + 0.5);
    int dh = (int)((double)sh * scale + 0.5);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    int ox = (w - dw) / 2;
    int oy = (h - dh) / 2;

    HDC dc = CreateCompatibleDC(NULL);
    if (!dc) { PROFILE_END_VAR(timer, "bg_tile_compose"); return NULL; }
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = NULL;
    HDC screenRef = GetDC(NULL);
    HBITMAP bmp = CreateDIBSection(screenRef ? screenRef : refDC, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (screenRef) ReleaseDC(NULL, screenRef);
    if (!bmp || !bits) {
        if (dc) DeleteDC(dc);
        if (bmp) DeleteObject(bmp);
        PROFILE_END_VAR(timer, "bg_tile_compose");
        return NULL;
    }
    memset(bits, 0, (size_t)w * (size_t)h * 4);
    SelectObject(dc, bmp);
    GpGraphics* g = NULL;
    GpStatus gst = GdipCreateFromHDC(dc, &g);
    if (gst == Ok && g) {
        GdipSetCompositingMode(g, CompositingModeSourceOver);
        GdipSetInterpolationMode(g, InterpolationModeHighQualityBicubic);
        GdipSetSmoothingMode(g, SmoothingModeHighQuality);
        GdipSetPixelOffsetMode(g, PixelOffsetModeHalf);
        GpStatus dst = GdipDrawImageRectI(g, (GpBitmap*)s->src, ox, oy, dw, dh);
        if (dst != Ok) {
            log_error("bg_image: draw st=%d at %d,%d %dx%d", (int)dst, ox, oy, dw, dh);
            fprintf(stderr, "[bg_image] DRAW FAILED st=%d dest=%d,%d %dx%d\n", (int)dst, ox, oy, dw, dh);
        }
        GdipDeleteGraphics(g);
    } else {
        fprintf(stderr, "[bg_image] GdipCreateFromHDC failed st=%d (tile would be black)\n", (int)gst);
    }

    BgTileSlot* slot = NULL;
    for (int i = 0; i < BG_TILE_SLOTS; i++) {
        if (g_bg_tiles[i].bmp == NULL) { slot = &g_bg_tiles[i]; break; }
    }
    if (!slot) {
        int oldest = 0;
        for (int i = 1; i < BG_TILE_SLOTS; i++) if (g_bg_tiles[i].stamp < g_bg_tiles[oldest].stamp) oldest = i;
        slot = &g_bg_tiles[oldest];
    }
    bg_tile_drop(slot);
    slot->bmp = bmp;
    slot->dc = dc;
    slot->w = w;
    slot->h = h;
    slot->mtime = ft;
    slot->size = sz;
    slot->stamp = ++g_bg_tick;
    slot->checked_ms = GetTickCount();
    snprintf(slot->path, sizeof(slot->path), "%s", native);
    log_info("bg_image: tile %dx%d from %dx%d -> %dx%d at %d,%d", w, h, sw, sh, dw, dh, ox, oy);
    fprintf(stderr, "[bg_image] tile OK container=%dx%d src=%dx%d scaled=%dx%d off=%d,%d\n", w, h, sw, sh, dw, dh, ox, oy);
    PROFILE_END_VAR(timer, "bg_tile_compose");
    { PROFILE_END_VAR(_pt, "bg_tile_compose"); return slot; }
}

static BgTileSlot* bg_tile_get(HDC refDC, const char* path, int w, int h) {
    PROFILE_START(_pt);
PROFILE_START(timer);
    if (!path || !path[0] || w <= 0 || h <= 0) { PROFILE_END_VAR(timer, "bg_tile_get"); return NULL; }
    char native[512];
    bg_native_path(native, sizeof(native), path);
    DWORD now = GetTickCount();

    BgTileSlot* stale = NULL;
    for (int i = 0; i < BG_TILE_SLOTS; i++) {
        BgTileSlot* t = &g_bg_tiles[i];
        if (!t->bmp || strcmp(t->path, native) != 0 || t->w != w || t->h != h) continue;
        if (now - t->checked_ms < BG_REVALIDATE_MS) {
            t->stamp = ++g_bg_tick;
            PROFILE_END_VAR(timer, "bg_tile_get");
            return t;
        }
        FILETIME ft;
        DWORD sz = 0;
        bg_stat(native, &ft, &sz);
        if (memcmp(&t->mtime, &ft, sizeof(ft)) != 0 || t->size != sz) { stale = t; continue; }
        t->checked_ms = now;
        t->stamp = ++g_bg_tick;
        PROFILE_END_VAR(timer, "bg_tile_get");
        return t;
    }

    FILETIME ft;
    DWORD sz = 0;
    bg_stat(native, &ft, &sz);
    BgTileSlot* made = bg_tile_compose(refDC, native, w, h, ft, sz);
    if (!made && stale) {
        PROFILE_END_VAR(timer, "bg_tile_get");
        return stale;
    }
    PROFILE_END_VAR(timer, "bg_tile_get");
    { PROFILE_END_VAR(_pt, "bg_tile_get"); return made; }
}

bool win32_bg_image_enabled(void) {
    PROFILE_START(_pt);
const char* p = BackgroundImage;
    { PROFILE_END_VAR(_pt, "win32_bg_image_enabled"); return p && p[0]; }
}

bool win32_draw_bg_image_slice(HDC hdc, RECT* rc, int containerW, int containerH, int originX, int originY) {
    PROFILE_START(_pt);
PROFILE_START(timer);
    if (!hdc || !rc) { PROFILE_END_VAR(timer, "bg_slice"); return false; }
    if (!win32_bg_image_enabled()) { PROFILE_END_VAR(timer, "bg_slice"); return false; }
    int dw = rc->right - rc->left;
    int dh = rc->bottom - rc->top;
    if (dw <= 0 || dh <= 0) { PROFILE_END_VAR(timer, "bg_slice"); return false; }
    if (containerW <= 0 || containerH <= 0) { PROFILE_END_VAR(timer, "bg_slice"); return false; }

    BgTileSlot* t = NULL;
    bg_lock();
    t = bg_tile_get(hdc, BackgroundImage, containerW, containerH);
    if (!t || !t->bmp || !t->dc) {
        bg_unlock();
        PROFILE_END_VAR(timer, "bg_slice");
        return false;
    }

    int sx = rc->left + originX;
    int sy = rc->top + originY;
    int dx = rc->left;
    int dy = rc->top;
    int sw = dw;
    int sh = dh;
    if (sx < 0) { int adj = -sx; sx = 0; dx += adj; sw -= adj; }
    if (sy < 0) { int adj = -sy; sy = 0; dy += adj; sh -= adj; }
    if (sx + sw > containerW) sw = containerW - sx;
    if (sy + sh > containerH) sh = containerH - sy;
    bool ok = sw > 0 && sh > 0;
    if (ok) BitBlt(hdc, dx, dy, sw, sh, t->dc, sx, sy, SRCCOPY);
    bg_unlock();
    PROFILE_END_VAR(timer, "bg_slice");
    { PROFILE_END_VAR(_pt, "win32_draw_bg_image_slice"); return ok; }
}

int win32_bg_image_prewarm(int containerW, int containerH) {
    PROFILE_START(_pt);
PROFILE_START(timer);
    if (!win32_bg_image_enabled() || containerW <= 0 || containerH <= 0) { PROFILE_END_VAR(timer, "bg_prewarm"); return 0; }
    HDC screen = GetDC(NULL);
    BgTileSlot* t = NULL;
    bg_lock();
    if (screen) t = bg_tile_get(screen, BackgroundImage, containerW, containerH);
    bg_unlock();
    if (screen) ReleaseDC(NULL, screen);
    PROFILE_END_VAR(timer, "bg_prewarm");
    { PROFILE_END_VAR(_pt, "win32_bg_image_prewarm"); return t ? 1 : 0; }
}