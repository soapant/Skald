#include "helpers/win32/bg_image_win32.h"
#include "helpers/win32/raw_draw_win32.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_fs.h"
#include "core/types.h"
#include "config_values.h"
#include "platform/platform.h"
#include <windows.h>
#include <shellapi.h>
#include <gdiplus/gdiplus.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define BG_SRC_SLOTS 2
#define BG_TILE_SLOTS 4
#define BG_REVALIDATE_MS 1000
#define BG_RETRY_MS 2000
#define BG_CACHE_DIR "bg_cache"
#define BG_CACHE_MAGIC "FLCB"
#define BG_CACHE_VER 1

#pragma pack(push, 1)
typedef struct {
  char magic[4];
  uint32_t version;
  uint64_t src_mtime;
  uint32_t src_size;
} BgCacheHeader;
#pragma pack(pop)

typedef struct {
  char path[512];
  FILETIME mtime;
  DWORD size;
  int src_w;
  int src_h;
  void *src;
  ULONGLONG stamp;
  DWORD checked_ms;
  bool failed;
} BgSrcSlot;

typedef struct {
  HBITMAP bmp;
  HDC dc;
  int w;
  int h;
  char path[512];
  FILETIME mtime;
  DWORD size;
  ULONGLONG stamp;
  DWORD checked_ms;
} BgTileSlot;

static BgSrcSlot bg_srcs[BG_SRC_SLOTS];
static BgTileSlot g_bg_tiles[BG_TILE_SLOTS];
static ULONGLONG g_bg_tick = 0;
static CRITICAL_SECTION bg_cs;
static INIT_ONCE bg_cs_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK bg_cs_init(PINIT_ONCE once, PVOID param, PVOID *ctx) {
  InitializeCriticalSection(&bg_cs);
  return TRUE;
}
static ULONG_PTR bg_gdip_token = 0;

static void bg_lock(void) {

  InitOnceExecuteOnce(&bg_cs_once, bg_cs_init, NULL, NULL);
  EnterCriticalSection(&bg_cs);
}

static void bg_unlock(void) {

  InitOnceExecuteOnce(&bg_cs_once, bg_cs_init, NULL, NULL);
  LeaveCriticalSection(&bg_cs);
}

static void bg_gdip_ensure(void) {

  if (bg_gdip_token != 0) {
    return;
  }
  GdiplusStartupInput gsi;
  gsi.GdiplusVersion = 1;
  gsi.DebugEventCallback = NULL;
  gsi.SuppressBackgroundThread = FALSE;
  gsi.SuppressExternalCodecs = FALSE;
  GdiplusStartup(&bg_gdip_token, &gsi, NULL);
}

static void bg_native_path(char *out, size_t cap, const char *path) {

  if (!out || cap == 0) {
    return;
  }
  const char *src = path ? path : "";
  char expanded[512];
  if (src[0] == '~' && (src[1] == '\0' || src[1] == '/' || src[1] == '\\')) {
    const char *home = Platform_GetRealHome();
    if (home && home[0]) {
      snprintf(expanded, sizeof(expanded), "%s%s", home, src[1] ? src + 1 : "");
      src = expanded;
    }
  }
  snprintf(out, cap, "%s", src);
  for (char *q = out; *q; q++)
    if (*q == '/')
      *q = '\\';
}

static void bg_stat(const char *native, FILETIME *ft, DWORD *size) {

  ft->dwLowDateTime = 0;
  ft->dwHighDateTime = 0;
  *size = 0;
  WIN32_FILE_ATTRIBUTE_DATA fad;
  if (!GetFileAttributesExA(native, GetFileExInfoStandard, &fad)) {
    return;
  }
  *ft = fad.ftLastWriteTime;
  *size = fad.nFileSizeLow ^ (fad.nFileSizeHigh * 2654435761u);
}

static void bg_widen(const char *in, wchar_t *out, int out_max) {

  out[0] = 0;
  if (!in || !in[0] || out_max <= 0) {
    return;
  }
  if (MultiByteToWideChar(CP_UTF8, 0, in, -1, out, out_max) <= 0)
    out[0] = 0;
}

static unsigned int bg_hash_str(const char *s) {
  unsigned int h = 2166136261u;
  for (; s && *s; s++) {
    h ^= (unsigned char)*s;
    h *= 16777619u;
  }
  return h;
}

static bool bg_cache_file(char *out, size_t cap, const char *native, int w,
                          int h) {
  char dir[MAX_PATH];
  if (!feature_path_get("core", BG_CACHE_DIR, dir, sizeof(dir)))
    return false;
  CreateDirectoryA(dir, NULL);
  snprintf(out, cap, "%s\\%08x_%dx%d.bmp", dir, bg_hash_str(native), w, h);
  return true;
}

static uint64_t bg_mtime_key(const FILETIME *ft) {
  if (!ft)
    return 0;
  return ((uint64_t)ft->dwHighDateTime << 32) | (uint64_t)ft->dwLowDateTime;
}

static bool bg_cache_store(const char *file, int w, int h, const void *bits,
                           const FILETIME *ft, DWORD sz) {

  FILE *f = fopen(file, "wb");
  if (!f)
    return false;
  BgCacheHeader ch;
  memset(&ch, 0, sizeof(ch));
  memcpy(ch.magic, BG_CACHE_MAGIC, 4);
  ch.version = BG_CACHE_VER;
  ch.src_mtime = bg_mtime_key(ft);
  ch.src_size = sz;
  BITMAPFILEHEADER fh;
  memset(&fh, 0, sizeof(fh));
  fh.bfType = 0x4D42;
  fh.bfOffBits = (DWORD)(sizeof(BgCacheHeader) + sizeof(BITMAPFILEHEADER) +
                         sizeof(BITMAPINFOHEADER));
  fh.bfSize = fh.bfOffBits + (DWORD)((size_t)w * (size_t)h * 4);
  BITMAPINFOHEADER ih;
  memset(&ih, 0, sizeof(ih));
  ih.biSize = sizeof(BITMAPINFOHEADER);
  ih.biWidth = w;
  ih.biHeight = -h;
  ih.biPlanes = 1;
  ih.biBitCount = 32;
  ih.biCompression = BI_RGB;
  ih.biSizeImage = (DWORD)((size_t)w * (size_t)h * 4);
  bool ok = fwrite(&ch, sizeof(ch), 1, f) == 1 &&
            fwrite(&fh, sizeof(fh), 1, f) == 1 &&
            fwrite(&ih, sizeof(ih), 1, f) == 1 &&
            fwrite(bits, 4, (size_t)w * (size_t)h, f) == (size_t)w * (size_t)h;
  fclose(f);
  return ok;
}

static bool bg_cache_load(const char *file, int w, int h, void *bits,
                          const FILETIME *ft, DWORD sz) {

  FILE *f = fopen(file, "rb");
  if (!f)
    return false;
  BgCacheHeader ch;
  BITMAPFILEHEADER fh;
  BITMAPINFOHEADER ih;
  bool ok = false;
  if (fread(&ch, sizeof(ch), 1, f) == 1 &&
      memcmp(ch.magic, BG_CACHE_MAGIC, 4) == 0 && ch.version == BG_CACHE_VER &&
      ch.src_mtime == bg_mtime_key(ft) && ch.src_size == sz &&
      fread(&fh, sizeof(fh), 1, f) == 1 && fread(&ih, sizeof(ih), 1, f) == 1 &&
      fh.bfType == 0x4D42 && ih.biWidth == w && ih.biHeight == -h &&
      ih.biBitCount == 32 && ih.biCompression == BI_RGB) {
    ok = fread(bits, 4, (size_t)w * (size_t)h, f) == (size_t)w * (size_t)h;
  }
  fclose(f);
  return ok;
}

static void *bg_load_native(const char *native, int *out_w, int *out_h) {

  bg_gdip_ensure();
  if (bg_gdip_token == 0)
    return NULL;
  wchar_t wpath[1024];
  bg_widen(native, wpath, (int)(sizeof(wpath) / sizeof(wchar_t)));
  GpBitmap *bmp = NULL;
  GpStatus st =
      wpath[0] ? GdipCreateBitmapFromFile(wpath, &bmp) : InvalidParameter;
  if (st != Ok || !bmp) {
    if (bmp)
      GdipDisposeImage((GpImage *)bmp);
    bmp = (GpBitmap *)win32_load_shell_icon_ex(native, true);
    if (!bmp) {
      return NULL;
    }
  }
  UINT gw = 0, gh = 0;
  if (GdipGetImageWidth((GpImage *)bmp, &gw) != Ok ||
      GdipGetImageHeight((GpImage *)bmp, &gh) != Ok || gw == 0 || gh == 0) {
    GdipDisposeImage((GpImage *)bmp);
    return NULL;
  }
  if (out_w)
    *out_w = (int)gw;
  if (out_h)
    *out_h = (int)gh;
  return bmp;
}

static void bg_src_drop(BgSrcSlot *s) {

  if (!s) {
    return;
  }
  if (s->src)
    GdipDisposeImage((GpImage *)s->src);
  memset(s, 0, sizeof(*s));
}

static void bg_tile_drop(BgTileSlot *t) {

  if (!t) {
    return;
  }
  if (t->dc) {
    DeleteDC(t->dc);
    t->dc = NULL;
  }
  if (t->bmp) {
    DeleteObject(t->bmp);
    t->bmp = NULL;
  }
  memset(t, 0, sizeof(*t));
}

void win32_bg_image_purge(void) {
  bg_lock();
  for (int i = 0; i < BG_SRC_SLOTS; i++)
    bg_src_drop(&bg_srcs[i]);
  for (int i = 0; i < BG_TILE_SLOTS; i++)
    bg_tile_drop(&g_bg_tiles[i]);
  bg_unlock();
}

static BgSrcSlot *bg_src_get(const char *native) {

  DWORD now = GetTickCount();
  BgSrcSlot *slot = NULL;
  for (int i = 0; i < BG_SRC_SLOTS; i++) {
    BgSrcSlot *s = &bg_srcs[i];
    if (s->path[0] && strcmp(s->path, native) != 0)
      continue;
    if (strcmp(s->path, native) != 0) {
      slot = s;
      break;
    }
    FILETIME ft;
    DWORD sz = 0;
    bg_stat(native, &ft, &sz);
    if (memcmp(&s->mtime, &ft, sizeof(ft)) != 0 || s->size != sz) {
      bg_src_drop(s);
      slot = s;
      break;
    }
    if (s->src) {
      s->stamp = ++g_bg_tick;
      return s;
    }
    if (s->failed && now - s->checked_ms < BG_RETRY_MS)
      return NULL;
    slot = s;
    break;
  }
  if (!slot) {
    for (int i = 0; i < BG_SRC_SLOTS; i++) {
      if (bg_srcs[i].path[0] == 0) {
        slot = &bg_srcs[i];
        break;
      }
    }
  }
  if (!slot) {
    int oldest = 0;
    for (int i = 1; i < BG_SRC_SLOTS; i++)
      if (bg_srcs[i].stamp < bg_srcs[oldest].stamp)
        oldest = i;
    slot = &bg_srcs[oldest];
    bg_src_drop(slot);
  }
  bg_src_drop(slot);
  void *bmp = bg_load_native(native, &slot->src_w, &slot->src_h);
  snprintf(slot->path, sizeof(slot->path), "%s", native);
  slot->checked_ms = now;
  slot->stamp = ++g_bg_tick;
  if (!bmp) {
    slot->failed = true;
    return NULL;
  }
  FILETIME ft;
  DWORD sz = 0;
  bg_stat(native, &ft, &sz);
  slot->mtime = ft;
  slot->size = sz;
  slot->src = bmp;
  return slot;
}

static int bg_slot_empty(const void *s) {
  return ((const BgTileSlot *)s)->bmp == NULL;
}
static unsigned long bg_slot_stamp(const void *s) {
  return (unsigned long)((const BgTileSlot *)s)->stamp;
}

static BgTileSlot *bg_tile_compose(HDC refDC, const char *native, int w, int h,
                                   FILETIME ft, DWORD sz) {

  HDC dc = CreateCompatibleDC(NULL);
  if (!dc)
    return NULL;
  BITMAPINFO bmi;
  ZeroMemory(&bmi, sizeof(bmi));
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void *bits = NULL;
  HDC screenRef = GetDC(NULL);
  HBITMAP bmp = CreateDIBSection(screenRef ? screenRef : refDC, &bmi,
                                 DIB_RGB_COLORS, &bits, NULL, 0);
  if (screenRef)
    ReleaseDC(NULL, screenRef);
  if (!bmp || !bits) {
    DeleteDC(dc);
    if (bmp)
      DeleteObject(bmp);
    return NULL;
  }
  memset(bits, 0, (size_t)w * (size_t)h * 4);
  SelectObject(dc, bmp);

  char cacheFile[MAX_PATH];
  bool haveCache = bg_cache_file(cacheFile, sizeof(cacheFile), native, w, h);
  bool fromCache = haveCache && bg_cache_load(cacheFile, w, h, bits, &ft, sz);
  if (!fromCache) {
    BgSrcSlot *s = bg_src_get(native);
    if (!s || !s->src) {
      DeleteDC(dc);
      DeleteObject(bmp);
      return NULL;
    }
    int sw = s->src_w;
    int sh = s->src_h;
    int dw = 0, dh = 0;
    if (!cover_scale(sw, sh, w, h, &dw, &dh)) {
      DeleteDC(dc);
      DeleteObject(bmp);
      return NULL;
    }
    int ox = (w - dw) / 2;
    int oy = (h - dh) / 2;

    GpGraphics *g = NULL;
    GpStatus gst = GdipCreateFromHDC(dc, &g);
    if (gst == Ok && g) {
      GdipSetCompositingMode(g, CompositingModeSourceOver);
      GdipSetInterpolationMode(g, InterpolationModeHighQualityBicubic);
      GdipSetSmoothingMode(g, SmoothingModeHighQuality);
      GdipSetPixelOffsetMode(g, PixelOffsetModeHalf);
      GpStatus dst = GdipDrawImageRectI(g, (GpBitmap *)s->src, ox, oy, dw, dh);
      GdipDeleteGraphics(g);
      if (dst != Ok) {
        DeleteDC(dc);
        DeleteObject(bmp);
        return NULL;
      }
    }
    if (haveCache)
      bg_cache_store(cacheFile, w, h, bits, &ft, sz);
  }

  int slot_idx =
      bg_tile_lru_pick(g_bg_tiles, BG_TILE_SLOTS, sizeof(g_bg_tiles[0]),
                       bg_slot_empty, bg_slot_stamp);
  BgTileSlot *slot = &g_bg_tiles[slot_idx];
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
  return slot;
}

static BgTileSlot *bg_tile_get(HDC refDC, const char *path, int w, int h) {

  if (!path || !path[0] || w <= 0 || h <= 0)
    return NULL;
  char native[512];
  bg_native_path(native, sizeof(native), path);
  DWORD now = GetTickCount();

  BgTileSlot *stale = NULL;
  for (int i = 0; i < BG_TILE_SLOTS; i++) {
    BgTileSlot *t = &g_bg_tiles[i];
    if (!t->bmp || strcmp(t->path, native) != 0 || t->w != w || t->h != h)
      continue;
    if (now - t->checked_ms < BG_REVALIDATE_MS) {
      t->stamp = ++g_bg_tick;
      return t;
    }
    FILETIME ft;
    DWORD sz = 0;
    bg_stat(native, &ft, &sz);
    if (memcmp(&t->mtime, &ft, sizeof(ft)) != 0 || t->size != sz) {
      stale = t;
      continue;
    }
    t->checked_ms = now;
    t->stamp = ++g_bg_tick;
    return t;
  }

  FILETIME ft;
  DWORD sz = 0;
  bg_stat(native, &ft, &sz);
  BgTileSlot *made = bg_tile_compose(refDC, native, w, h, ft, sz);
  if (!made && stale) {
    return stale;
  }
  return made;
}

bool win32_bg_image_enabled(void) {
  const char *p = BackgroundImage;
  return p && p[0];
}

bool win32_draw_bg_image_slice(HDC hdc, const RECT *rc, int containerW,
                               int containerH, int originX, int originY) {

  if (!hdc || !rc)
    return false;
  if (!win32_bg_image_enabled())
    return false;
  int dw = rc->right - rc->left;
  int dh = rc->bottom - rc->top;
  if (dw <= 0 || dh <= 0)
    return false;
  if (containerW <= 0 || containerH <= 0)
    return false;

  BgTileSlot *t = NULL;
  bg_lock();
  t = bg_tile_get(hdc, BackgroundImage, containerW, containerH);
  if (!t || !t->bmp || !t->dc) {
    bg_unlock();
    return false;
  }

  int sx = rc->left + originX;
  int sy = rc->top + originY;
  int dx = rc->left;
  int dy = rc->top;
  int sw = dw;
  int sh = dh;
  if (sx < 0) {
    int adj = -sx;
    sx = 0;
    dx += adj;
    sw -= adj;
  }
  if (sy < 0) {
    int adj = -sy;
    sy = 0;
    dy += adj;
    sh -= adj;
  }
  if (sx + sw > containerW)
    sw = containerW - sx;
  if (sy + sh > containerH)
    sh = containerH - sy;
  bool ok = sw > 0 && sh > 0;
  if (ok)
    BitBlt(hdc, dx, dy, sw, sh, t->dc, sx, sy, SRCCOPY);
  bg_unlock();
  return ok;
}

int win32_bg_image_prewarm(int containerW, int containerH) {
  if (!win32_bg_image_enabled() || containerW <= 0 || containerH <= 0)
    return 0;
  HDC screen = GetDC(NULL);
  const BgTileSlot *t = NULL;
  bg_lock();
  if (screen)
    t = bg_tile_get(screen, BackgroundImage, containerW, containerH);
  bg_unlock();
  if (screen)
    ReleaseDC(NULL, screen);
  return t ? 1 : 0;
}
