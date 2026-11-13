#include "helpers/win32/raw_draw_win32.h"

__thread HFONT g_sizeFonts[WIN32_SIZE_FONT_SLOTS] = {NULL};

__thread int g_sizeFontSizes[WIN32_SIZE_FONT_SLOTS] = {0};

wchar_t *win32_widen(const char *in, int in_len, int *out_len) {

  if (out_len)
    *out_len = 0;
  if (!in || in_len <= 0)
    return NULL;
  int n = MultiByteToWideChar(CP_UTF8, 0, in, in_len, NULL, 0);
  if (n <= 0)
    return NULL;
  wchar_t *w = (wchar_t *)malloc((size_t)n * sizeof(wchar_t) + sizeof(wchar_t));
  if (!w)
    return NULL;
  if (MultiByteToWideChar(CP_UTF8, 0, in, in_len, w, n) <= 0) {
    free(w);
    return NULL;
  }
  w[n] = 0;
  if (out_len)
    *out_len = n;
  return w;
}

__thread HDC g_draw_hdc = NULL;

HFONT win32_sized_font(int size);

static void win32_img_stat(const char *path, FILETIME *ft, DWORD *size);

static void win32_path_to_native(char *dst, size_t cap, const char *src) {
  snprintf(dst, cap, "%s", src);
  for (char *p = dst; *p; p++)
    if (*p == '/')
      *p = '\\';
}

static bool win32_text_any(const wchar_t *wide, int len,
                           bool (*pred)(unsigned int)) {
  for (int i = 0; i < len; i++) {
    unsigned int c = (unsigned int)wide[i];
    if (c == 0xFE0F || c == 0xFE0E)
      continue;
    if (pred(c))
      return true;
  }
  return false;
}

static bool win32_pred_symbolic(unsigned int c) { return c > 0x7F; }

static bool win32_pred_boost(unsigned int c) {
  if (c >= 0x1F000)
    return true;
  if (c >= 0xD800 && c <= 0xDFFF)
    return true;
  if (c >= 0xE000 && c <= 0xF8FF)
    return true;
  return false;
}

static bool win32_entry_changed(DWORD now, DWORD *checked_ms, const char *path,
                                const FILETIME *slot_mtime, DWORD slot_size) {
  if (now - *checked_ms < WIN32_IMG_REVALIDATE_MS)
    return false;
  *checked_ms = now;
  FILETIME ft;
  DWORD size = 0;
  win32_img_stat(path, &ft, &size);
  if (memcmp(slot_mtime, &ft, sizeof(ft)) != 0)
    return true;
  return slot_size != size;
}

bool win32_text_is_symbolic(const wchar_t *wide, int len) {
  return win32_text_any(wide, len, win32_pred_symbolic);
}

bool win32_font_covers(HFONT f, const wchar_t *wide, int len) {

  if (!f || !wide || len <= 0)
    return false;
  HFONT old = (HFONT)SelectObject(g_draw_hdc, f);
  bool ok = true;
  for (int i = 0; i < len; i++) {
    if (wide[i] < 0x80)
      continue;
    if (wide[i] >= 0xD800 && wide[i] <= 0xDBFF) {
      if (i + 1 < len && wide[i + 1] >= 0xDC00 && wide[i + 1] <= 0xDFFF) {
        i++;
        continue;
      }
    }
    WORD gi = 0xFFFF;
    DWORD r = GetGlyphIndicesW(g_draw_hdc, wide + i, 1, &gi, 0);
    if (r == GDI_ERROR || gi == 0xFFFF || gi == 0) {
      ok = false;
      break;
    }
  }
  SelectObject(g_draw_hdc, old);
  return ok;
}

HFONT g_sym_fonts[WIN32_SYM_SLOTS] = {NULL};

int g_sym_sizes[WIN32_SYM_SLOTS] = {0};

wchar_t g_sym_cp[WIN32_SYM_SLOTS][8] = {{0}};

int g_sym_len[WIN32_SYM_SLOTS] = {0};

ULONGLONG g_sym_tick = 0;

ULONGLONG g_sym_ticks[WIN32_SYM_SLOTS] = {0};

HFONT win32_symbol_font(int size, const wchar_t *wide, int len) {

  if (len > 8)
    len = 8;
  for (int i = 0; i < WIN32_SYM_SLOTS; i++) {
    if (g_sym_fonts[i] && g_sym_sizes[i] == size && g_sym_len[i] == len &&
        memcmp(g_sym_cp[i], wide, (size_t)len * sizeof(wchar_t)) == 0) {
      g_sym_ticks[i] = ++g_sym_tick;
      return g_sym_fonts[i];
    }
  }

  static const char *faces[] = {"Segoe UI Symbol",  "Segoe UI Emoji",
                                "Malgun Gothic",    "Segoe UI",
                                "Arial Unicode MS", NULL};
  HFONT found = NULL;
  for (int i = 0; faces[i]; i++) {
    HFONT f = FH_TO_HFONT(Platform_CreateFont(size, faces[i]));
    if (!f)
      continue;
    if (win32_font_covers(f, wide, len)) {
      found = f;
      break;
    }
    DeleteObject(f);
  }
  if (!found)
    return NULL;

  int slot = -1;
  ULONGLONG oldest = ~(ULONGLONG)0;
  for (int i = 0; i < WIN32_SYM_SLOTS; i++) {
    if (!g_sym_fonts[i]) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    for (int i = 0; i < WIN32_SYM_SLOTS; i++) {
      if (g_sym_sizes[i] == 0) {
        slot = i;
        break;
      }
    }
  }
  if (slot < 0) {
    for (int i = 0; i < WIN32_SYM_SLOTS; i++) {
      ULONGLONG t = g_sym_ticks[i];
      if (t <= oldest) {
        oldest = t;
        slot = i;
      }
    }
  }
  if (g_sym_fonts[slot])
    DeleteObject(g_sym_fonts[slot]);
  g_sym_fonts[slot] = found;
  g_sym_sizes[slot] = size;
  g_sym_len[slot] = len;
  g_sym_ticks[slot] = ++g_sym_tick;
  for (int k = 0; k < len; k++)
    g_sym_cp[slot][k] = wide[k];
  return found;
}

bool win32_text_needs_boost(const wchar_t *wide, int len) {
  return win32_text_any(wide, len, win32_pred_boost);
}

HFONT win32_font_for_text(const wchar_t *wide, int len, int fontsize,
                          HFONT *out_prev, int *out_dy) {

  HFONT prev = NULL;
  if (out_dy)
    *out_dy = 0;
  if (win32_text_is_symbolic(wide, len)) {
    bool boost = win32_text_needs_boost(wide, len);
    int want = boost
                   ? (fontsize * WIN32_SYMBOL_SIZE_NUM / WIN32_SYMBOL_SIZE_DEN)
                   : fontsize;
    HFONT sf = win32_symbol_font(want, wide, len);
    if (sf) {
      HFONT nf = win32_sized_font(fontsize);
      if (!nf)
        nf = win32_sized_font(fontsize);
      HFONT tmp = nf ? (HFONT)SelectObject(g_draw_hdc, nf) : NULL;
      TEXTMETRICW ts;
      ts.tmAscent = 0;
      if (nf)
        GetTextMetricsW(g_draw_hdc, &ts);
      if (tmp)
        SelectObject(g_draw_hdc, tmp);
      prev = (HFONT)SelectObject(g_draw_hdc, sf);
      if (boost) {
        TEXTMETRICW tb;
        GetTextMetricsW(g_draw_hdc, &tb);
        if (out_dy)
          *out_dy = (ts.tmAscent - tb.tmAscent);
      }
      if (out_prev)
        *out_prev = prev;
      return sf;
    }
  }
  HFONT hfont = win32_sized_font(fontsize);
  if (!hfont)
    hfont = win32_sized_font(fontsize);
  if (hfont)
    prev = (HFONT)SelectObject(g_draw_hdc, hfont);
  if (out_prev)
    *out_prev = prev;
  return hfont;
}

HFONT win32_sized_font(int size) {

  if (size <= 0)
    size = FontSize;
  for (int i = 0; i < WIN32_SIZE_FONT_SLOTS; i++) {
    if (g_sizeFonts[i] && g_sizeFontSizes[i] == size)
      return g_sizeFonts[i];
  }
  int slot = -1;
  for (int i = 0; i < WIN32_SIZE_FONT_SLOTS; i++) {
    if (!g_sizeFonts[i]) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    slot = 0;
    for (int i = 1; i < WIN32_SIZE_FONT_SLOTS; i++) {
      if (g_sizeFontSizes[i] < g_sizeFontSizes[slot])
        slot = i;
    }
  }
  if (g_sizeFonts[slot])
    DeleteObject(g_sizeFonts[slot]);
  g_sizeFonts[slot] = FH_TO_HFONT(Platform_CreateFont(size, UsedFont));
  g_sizeFontSizes[slot] = size;
  return g_sizeFonts[slot];
}

void win32_set_draw_context(HDC hdc) { g_draw_hdc = hdc; }

void win32_get_draw_context(HDC *out) {
  if (out)
    *out = g_draw_hdc;
}

COLORREF win32_colorref(unsigned long hex) {

  unsigned long r = hex & 0xFF;
  unsigned long g = (hex >> 8) & 0xFF;
  unsigned long b = (hex >> 16) & 0xFF;
  return (COLORREF)((b << 16) | (g << 8) | r);
}

COLORREF win32_resolve_color(unsigned long hex, bool opaque) {

  COLORREF c = win32_colorref(hex);
  if (opaque)
    return c;
  float t = WindowOpacity / 255.0f;
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;
  COLORREF bg = win32_colorref(COLOR_BG);
  int br = (bg >> 16) & 0xFF, bg_g = (bg >> 8) & 0xFF, bb = bg & 0xFF;
  int r = (int)((c & 0xFF) * t + br * (1.0f - t));
  int g = (int)(((c >> 8) & 0xFF) * t + bg_g * (1.0f - t));
  int b = (int)(((c >> 16) & 0xFF) * t + bb * (1.0f - t));
  return (COLORREF)((b << 16) | (g << 8) | r);
}

Win32ImgSlot g_img_cache[WIN32_IMG_CACHE_SLOTS];

Win32SrcSlot g_src_cache[WIN32_SRC_CACHE_SLOTS];

ULONGLONG g_img_cache_tick = 0;

CRITICAL_SECTION g_img_cache_cs;

INIT_ONCE g_img_cache_cs_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK win32_img_cache_cs_init(PINIT_ONCE once, PVOID param,
                                             PVOID *ctx) {
  InitializeCriticalSection(&g_img_cache_cs);
  return TRUE;
}

ULONG_PTR g_gdip_token = 0;

void win32_gdip_ensure(void) {

  if (g_gdip_token != 0) {
    return;
  }
  GdiplusStartupInput gsi;
  gsi.GdiplusVersion = 1;
  gsi.DebugEventCallback = NULL;
  gsi.SuppressBackgroundThread = FALSE;
  gsi.SuppressExternalCodecs = FALSE;
  GdiplusStartup(&g_gdip_token, &gsi, NULL);
}

void win32_gdip_dispose(void *img) {

  if (img)
    GdipDisposeImage((GpImage *)img);
}

HDC win32_screen_dc(void) {

  static HDC dc = NULL;
  if (!dc)
    dc = GetDC(NULL);
  return dc;
}

void win32_img_cache_lock(void) {

  InitOnceExecuteOnce(&g_img_cache_cs_once, win32_img_cache_cs_init, NULL,
                      NULL);
  EnterCriticalSection(&g_img_cache_cs);
}

void win32_img_cache_unlock(void) {

  InitOnceExecuteOnce(&g_img_cache_cs_once, win32_img_cache_cs_init, NULL,
                      NULL);
  LeaveCriticalSection(&g_img_cache_cs);
}

static void win32_img_stat(const char *path, FILETIME *ft, DWORD *size) {

  ft->dwLowDateTime = 0;
  ft->dwHighDateTime = 0;
  *size = 0;
  char native[MAX_PATH * 2];
  win32_path_to_native(native, sizeof(native), path);
  WIN32_FILE_ATTRIBUTE_DATA fad;
  if (!GetFileAttributesExA(native, GetFileExInfoStandard, &fad)) {
    return;
  }
  *ft = fad.ftLastWriteTime;
  *size = fad.nFileSizeLow ^ (fad.nFileSizeHigh * 2654435761u);
}

void *win32_img_cache_get(const char *path, HDC screen, int w, int h) {

  HBITMAP found = NULL;
  DWORD now = GetTickCount();
  win32_img_cache_lock();
  for (int i = 0; i < WIN32_IMG_CACHE_SLOTS; i++) {
    Win32ImgSlot *s = &g_img_cache[i];
    if (!s->bmp && !s->failed)
      continue;
    if (s->screen != screen || s->w != w || s->h != h)
      continue;
    if (strcmp(s->key, path) != 0)
      continue;
    if (win32_entry_changed(now, &s->checked_ms, path, &s->mtime, s->size)) {
      win32_gdip_dispose(s->bmp);
      s->bmp = NULL;
      s->failed = 0;
      continue;
    }
    s->stamp = ++g_img_cache_tick;
    found = s->bmp ? s->bmp : WIN32_IMG_FAILED;
    break;
  }
  win32_img_cache_unlock();
  return found;
}

void win32_img_cache_put(const char *path, HDC screen, int w, int h,
                         void *bmp) {

  FILETIME ft;
  DWORD size;
  win32_img_stat(path, &ft, &size);
  win32_img_cache_lock();
  int slot = -1;
  ULONGLONG oldest = ~(ULONGLONG)0;
  for (int i = 0; i < WIN32_IMG_CACHE_SLOTS; i++) {
    Win32ImgSlot *s = &g_img_cache[i];
    if (!s->bmp && !s->failed) {
      slot = i;
      break;
    }
    if (s->screen != screen || s->w != w || s->h != h ||
        strcmp(s->key, path) != 0)
      continue;
    if (s->stamp < oldest) {
      oldest = s->stamp;
      slot = i;
    }
  }
  if (slot < 0) {
    for (int i = 0; i < WIN32_IMG_CACHE_SLOTS; i++) {
      Win32ImgSlot *s = &g_img_cache[i];
      if (s->screen != screen)
        continue;
      if (s->stamp < oldest) {
        oldest = s->stamp;
        slot = i;
      }
    }
  }
  if (slot < 0)
    slot = 0;
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

void *win32_src_cache_get(const char *path) {
  DWORD now = GetTickCount();
  void *found = NULL;
  win32_img_cache_lock();
  for (int i = 0; i < WIN32_SRC_CACHE_SLOTS; i++) {
    Win32SrcSlot *s = &g_src_cache[i];
    if (!s->src)
      continue;
    if (strcmp(s->key, path) != 0)
      continue;
    if (win32_entry_changed(now, &s->checked_ms, path, &s->mtime, s->size)) {
      win32_gdip_dispose(s->src);
      s->src = NULL;
      continue;
    }
    s->stamp = ++g_img_cache_tick;
    found = s->src;
    break;
  }
  win32_img_cache_unlock();
  return found;
}

void win32_src_cache_put(const char *path, void *src) {
  if (!src)
    return;
  FILETIME ft;
  DWORD size;
  win32_img_stat(path, &ft, &size);
  win32_img_cache_lock();
  int slot = -1;
  ULONGLONG oldest = ~(ULONGLONG)0;
  for (int i = 0; i < WIN32_SRC_CACHE_SLOTS; i++) {
    Win32SrcSlot *s = &g_src_cache[i];
    if (!s->src) {
      slot = i;
      break;
    }
    if (s->stamp < oldest) {
      oldest = s->stamp;
      slot = i;
    }
  }
  if (slot < 0)
    slot = 0;
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
  win32_img_cache_lock();
  for (int i = 0; i < WIN32_IMG_CACHE_SLOTS; i++) {
    if (g_img_cache[i].bmp) {
      win32_gdip_dispose(g_img_cache[i].bmp);
    }
    memset(&g_img_cache[i], 0, sizeof(g_img_cache[i]));
  }
  for (int i = 0; i < WIN32_SRC_CACHE_SLOTS; i++) {
    if (g_src_cache[i].src) {
      win32_gdip_dispose(g_src_cache[i].src);
    }
    memset(&g_src_cache[i], 0, sizeof(g_src_cache[i]));
  }
  win32_img_cache_unlock();
}

void win32_utf8_to_wide(const char *in, wchar_t *out, int out_max) {

  if (!in || !out || out_max <= 0) {
    return;
  }
  int n = MultiByteToWideChar(CP_UTF8, 0, in, -1, out, out_max);
  if (n <= 0) {
    out[0] = 0;
    return;
  }
}

GpBitmap *win32_load_shell_icon_ex(const char *path, bool use_file_attributes) {

  UINT flags = SHGFI_ICON | SHGFI_LARGEICON;
  if (use_file_attributes)
    flags |= SHGFI_USEFILEATTRIBUTES;
  SHFILEINFOA sfi;
  ZeroMemory(&sfi, sizeof(sfi));
  if (!SHGetFileInfoA(path, 0, &sfi, sizeof(sfi), flags))
    return NULL;
  if (!sfi.hIcon)
    return NULL;
  GpBitmap *bmp = NULL;
  GpStatus st = GdipCreateBitmapFromHICON(sfi.hIcon, &bmp);
  DestroyIcon(sfi.hIcon);
  if (st != Ok || !bmp)
    return NULL;
  return bmp;
}

GpBitmap *win32_load_shell_icon_bitmap(const char *path) {
  return win32_load_shell_icon_ex(path, false);
}

void *win32_load_scaled_bitmap(const char *path, int w, int h) {

  win32_gdip_ensure();
  if (g_gdip_token == 0) {
    log_error("img: GdiplusStartup failed");
    return NULL;
  }
  if (w <= 0 || h <= 0)
    return NULL;

  GpBitmap *src = (GpBitmap *)win32_src_cache_get(path);
  if (!src) {
    char native[MAX_PATH * 2];
    win32_path_to_native(native, sizeof(native), path);

    wchar_t wpath[MAX_PATH * 2];
    win32_utf8_to_wide(native, wpath, (int)(sizeof(wpath) / sizeof(wchar_t)));

    GpStatus _fs = GdipCreateBitmapFromFile(wpath, &src);
    if (_fs != Ok || !src) {
      if (src)
        GdipDisposeImage(src);
      src = win32_load_shell_icon_bitmap(native);
      if (!src) {
        log_error("img: cannot decode '%s' st=%d and no shell icon", path,
                  (int)_fs);
        return NULL;
      }
    }
    win32_src_cache_put(path, src);
  }

  GpBitmap *out = NULL;
  if (GdipCreateBitmapFromScan0(w, h, w * 4, PixelFormat32bppPARGB, NULL,
                                &out) != Ok ||
      !out) {
    return NULL;
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
  return out;
}

HDC s_cached_hdc = NULL;

GpGraphics *s_cached_g = NULL;

void win32_release_draw_cache(void) {
  if (s_cached_g) {
    GdipDeleteGraphics(s_cached_g);
    s_cached_g = NULL;
  }
  s_cached_hdc = NULL;
}

void win32_prompt_editbox_spec(PromptState *state, FontHandle font,
                               EditboxSpec *eb) {
  int text_len = (int)strlen(state->eb->filter);
  int cursor_x = 0;
  int text_w = 0;
  int ascent = 0;
  int descent = 0;
  FontHandle mfont = font;
  HFONT told = NULL;
  if (!mfont.hfont)
    mfont = Platform_CreateFont(FontSize, UsedFont);
  if (mfont.hfont)
    told = (HFONT)SelectObject(g_draw_hdc, mfont.hfont);
  TEXTMETRIC tm;
  memset(&tm, 0, sizeof(tm));
  GetTextMetrics(g_draw_hdc, &tm);
  ascent = tm.tmAscent;
  descent = tm.tmDescent;
  SIZE szCur;
  GetTextExtentPoint32A(g_draw_hdc, state->eb->filter, (int)state->eb->cursor,
                        &szCur);
  cursor_x = szCur.cx;
  if (text_len > 0) {
    SIZE szText;
    GetTextExtentPoint32A(g_draw_hdc, state->eb->filter, text_len, &szText);
    text_w = szText.cx;
  }
  if (told)
    SelectObject(g_draw_hdc, told);
  EditboxSpec_FillFromPrompt(eb, state, text_len, cursor_x, text_w, ascent,
                             descent, font);
}
