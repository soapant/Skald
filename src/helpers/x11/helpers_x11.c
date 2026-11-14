#include "helpers/helpers_text.h"
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#include <X11/extensions/shape.h>
#include <unistd.h>
#include <sys/wait.h>
#include "helpers/x11/env_helpers_x11.h"
#include "helpers/x11/helpers_x11.h"
#include "helpers/x11/window_factory_x11.h"
#include "core/window_manager.h"
#include "helpers/x11/bg_image_x11.h"
#include "core/prompt_geometry.h"
#include "core/listbox_hover.h"
#include "platform/platform.h"
#include "platform/draw.h"
#include "helpers/x11/drawing_x11.h"
#include "helpers/helpers_ui.h"
#include "wm/niri_wm.h"
#include "config_values.h"
#include "core/types.h"
#include "core/types/window.h"
#include "freetype/freetype.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wsign-compare"
#define NANOSVG_IMPLEMENTATION
#include "deps/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "deps/nanosvgrast.h"
#define STB_IMAGE_IMPLEMENTATION
#include "deps/stb_image.h"
#pragma GCC diagnostic pop

#define BG_SLOT_COUNT 4

typedef struct {
  bool used;
  Pixmap pm;
  Display *dpy;
  int w, h;
  unsigned long base;
  int radius;
  bool rounded;
} BgSlot;

static __thread BgSlot bg_slots[BG_SLOT_COUNT];
static __thread int bg_slot_next = 0;
static __thread Visual *dblVisual = NULL;
static __thread Colormap dblColormap = None;
static __thread int dblDepth = 0;
static __thread Display *dblDpy = NULL;

void x11_set_dbl_visual(Display *dpy, Visual *vis, Colormap cmap, int depth) {
  dblVisual = vis;
  dblColormap = cmap;
  dblDepth = depth;
  dblDpy = dpy;
}
void x11_clear_dbl_visual(void) {
  dblVisual = NULL;
  dblColormap = None;
  dblDepth = 0;
  dblDpy = NULL;
}

bool x11_get_drawable_visual(Display *dpy, Drawable d, Visual **visOut,
                             Colormap *cmapOut, int *depthOut) {
  XWindowAttributes attrs;
  if (XGetWindowAttributes(dpy, d, &attrs)) {
    if (visOut)
      *visOut = attrs.visual;
    if (cmapOut)
      *cmapOut = attrs.colormap;
    if (depthOut)
      *depthOut = attrs.depth;
    return true;
  }
  if (dblVisual && dblDpy == dpy) {
    if (visOut)
      *visOut = dblVisual;
    if (cmapOut)
      *cmapOut = dblColormap;
    if (depthOut)
      *depthOut = dblDepth;
    return true;
  }
  return false;
}

static int xft_width(Display *dpy, XftFont *xf, const char *text, int len) {
  XGlyphInfo ext;
  XftTextExtentsUtf8(dpy, xf, (FcChar8 *)text, len, &ext);
  return ext.xOff;
}

#define X11_FB_CACHE 24
typedef struct {
  Display *dpy;
  FcChar32 cp;
  int px;
  XftFont *f;
} X11FbSlot;
static X11FbSlot fb_cache[X11_FB_CACHE];
static int fb_next = 0;
static FcConfig *fb_config = NULL;
static pthread_mutex_t fb_mtx = PTHREAD_MUTEX_INITIALIZER;

static int x11_utf8_cp(const unsigned char *s, const unsigned char *end,
                       int *clen) {

  unsigned char c = *s;
  int n;
  FcChar32 cp;
  if (c < 0x80) {
    n = 1;
    cp = c;
  } else if ((c & 0xE0) == 0xC0) {
    n = 2;
    cp = c & 0x1Fu;
  } else if ((c & 0xF0) == 0xE0) {
    n = 3;
    cp = c & 0x0Fu;
  } else if ((c & 0xF8) == 0xF0) {
    n = 4;
    cp = c & 0x07u;
  } else {
    *clen = 1;
    return 0xFFFD;
  }
  if (s + n > end) {
    *clen = 1;
    return 0xFFFD;
  }
  for (int i = 1; i < n; i++) {
    if ((s[i] & 0xC0) != 0x80) {
      *clen = 1;
      return 0xFFFD;
    }
    cp = (cp << 6) | (FcChar32)(s[i] & 0x3F);
  }
  *clen = n;
  return cp;
}

static bool x11_font_has(XftFont *f, FcChar32 cp) {
  if (!f || !f->pattern)
    return false;
  if (cp < 0x21 || cp == 0x7F)
    return true;
  FcCharSet *cs = NULL;
  if (FcPatternGetCharSet(f->pattern, FC_CHARSET, 0, &cs) != FcResultMatch ||
      !cs)
    return true;
  if (!FcCharSetHasChar(cs, cp))
    return false;
  FT_Face face = XftLockFace(f);
  if (face) {
    bool ft = FcFreeTypeCharIndex(face, cp) != 0;
    XftUnlockFace(f);
    return ft;
  }
  return true;
}

static int x11_font_px(XftFont *xf, int px) {
  if (px > 0)
    return px;
  double d = 0;
  if (xf && xf->pattern &&
      FcPatternGetDouble(xf->pattern, FC_PIXEL_SIZE, 0, &d) == FcResultMatch &&
      d > 0)
    return (int)(d + 0.5);
  return FontSize;
}

static XftFont *x11_open_fallback_at_locked(Display *dpy, FcChar32 cp, int px) {
  XftFont *fb = NULL;
  FcCharSet *cs = FcCharSetCreate();
  FcPattern *pat = FcPatternCreate();
  if (cs && pat) {
    FcCharSetAddChar(cs, cp);
    FcPatternAddCharSet(pat, FC_CHARSET, cs);
    FcPatternAddString(pat, FC_FAMILY, (FcChar8 *)"sans-serif");
    FcPatternAddDouble(pat, FC_PIXEL_SIZE, (double)px);
    FcConfigSubstitute(fb_config, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);
    FcResult res = FcResultNoMatch;
    FcPattern *match = FcFontMatch(fb_config, pat, &res);
    if (match) {
      XLockDisplay(dpy);
      fb = XftFontOpenPattern(dpy, match);
      XUnlockDisplay(dpy);
      if (!fb)
        FcPatternDestroy(match);
    }
  }
  if (pat)
    FcPatternDestroy(pat);
  if (cs)
    FcCharSetDestroy(cs);
  return fb;
}

static XftFont *x11_fallback_for_locked(Display *dpy, FcChar32 cp, int px) {
  if (!fb_config)
    fb_config = FcInitLoadConfigAndFonts();
  if (!fb_config)
    return NULL;
  for (int i = 0; i < X11_FB_CACHE; i++) {
    if (fb_cache[i].f && fb_cache[i].dpy == dpy && fb_cache[i].cp == cp &&
        fb_cache[i].px == px)
      return fb_cache[i].f;
  }
  XftFont *fb = x11_open_fallback_at_locked(dpy, cp, px > 0 ? px : FontSize);
  if (!fb)
    return NULL;
  if (fb_cache[fb_next].f) {
    Display *odpy = fb_cache[fb_next].dpy;
    XLockDisplay(odpy);
    XftFontClose(odpy, fb_cache[fb_next].f);
    XUnlockDisplay(odpy);
  }
  fb_cache[fb_next].dpy = dpy;
  fb_cache[fb_next].cp = cp;
  fb_cache[fb_next].px = px;
  fb_cache[fb_next].f = fb;
  fb_next = (fb_next + 1) % X11_FB_CACHE;
  return fb;
}

static XftFont *x11_fallback_for(Display *dpy, FcChar32 cp, int px) {
  pthread_mutex_lock(&fb_mtx);
  XftFont *f = x11_fallback_for_locked(dpy, cp, px);
  pthread_mutex_unlock(&fb_mtx);
  return f;
}

static int x11_utf8_fallback_runs(Display *dpy, XftFont *xf,
                                  const unsigned char *b, int n, int px,
                                  int (*run_cb)(Display *dpy, XftFont *font,
                                                const unsigned char *run,
                                                int len, void *ud),
                                  void *ud) {
  if (!xf || !b || n <= 0)
    return 0;
  int total = 0;
  const unsigned char *end = b + n;
  const unsigned char *p = b;
  while (p < end) {
    const unsigned char *runStart = p;
    XftFont *runFont = xf;
    while (p < end) {
      int cl = 1;
      FcChar32 cp = x11_utf8_cp(p, end, &cl);
      if (!x11_font_has(runFont, cp))
        break;
      p += cl;
    }
    if (p > runStart)
      total += run_cb(dpy, runFont, runStart, (int)(p - runStart), ud);
    if (p >= end)
      break;
    int cl = 1;
    FcChar32 cp = x11_utf8_cp(p, end, &cl);
    XftFont *fb = x11_fallback_for(dpy, cp, px);
    if (fb) {
      const unsigned char *fs = p;
      int covered = 0;
      while (p < end) {
        int l2 = 1;
        FcChar32 c2 = x11_utf8_cp(p, end, &l2);
        if (!x11_font_has(fb, c2))
          break;
        p += l2;
        covered += l2;
      }
      if (covered > 0)
        total += run_cb(dpy, fb, fs, covered, ud);
      else
        p += cl;
    } else {
      p += cl;
    }
  }
  return total;
}

static int x11_measure_run(Display *dpy, XftFont *font,
                           const unsigned char *run, int len, void *ud) {
  (void)ud;
  return xft_width(dpy, font, (const char *)run, len);
}

typedef struct {
  XftDraw *xd;
  XftColor *color;
  int y;
  int cx;
} X11DrawRunCtx;

static int x11_draw_run(Display *dpy, XftFont *font, const unsigned char *run,
                        int len, void *ud) {
  X11DrawRunCtx *ctx = (X11DrawRunCtx *)ud;
  XftDrawStringUtf8(ctx->xd, ctx->color, font, ctx->cx, ctx->y, (FcChar8 *)run,
                    len);
  int w = xft_width(dpy, font, (const char *)run, len);
  ctx->cx += w;
  return w;
}

int x11_measure_utf8_fallback(Display *dpy, XftFont *xf, const char *s, int n,
                              int px) {
  if (!xf || !s || n <= 0)
    return 0;
  px = x11_font_px(xf, px);
  const unsigned char *b = (const unsigned char *)s;
  const unsigned char *end = b + n;
  bool all = true;
  const unsigned char *p = b;
  while (p < end) {
    int cl = 1;
    FcChar32 cp = x11_utf8_cp(p, end, &cl);
    if (!x11_font_has(xf, cp)) {
      all = false;
      break;
    }
    p += cl;
  }
  if (all)
    return xft_width(dpy, xf, s, n);
  return x11_utf8_fallback_runs(dpy, xf, b, n, px, x11_measure_run, NULL);
}

int x11_draw_utf8_fallback(Display *dpy, XftDraw *xd, XftColor *color, int x,
                           int y, XftFont *xf, const char *s, int n, int px) {
  if (!xf || !xd || !s || n <= 0)
    return 0;
  px = x11_font_px(xf, px);
  X11DrawRunCtx ctx = {xd, color, y, x};
  x11_utf8_fallback_runs(dpy, xf, (const unsigned char *)s, n, px, x11_draw_run,
                         &ctx);
  return ctx.cx - x;
}

static void x11_wrap_flush_line(Display *dpy, XftDraw *xd, XftColor *color,
                                XftFont *xf, const char *lineStart,
                                int line_len, int alignment, int boxWidth,
                                int maxWidth, int xOffset, int y) {
  int lw = x11_measure_utf8_fallback(dpy, xf, lineStart, line_len, 0);
  int drawX = 0;
  if (alignment == 1)
    drawX = boxWidth > 0 ? (maxWidth - lw) / 2 : xOffset + (maxWidth - lw) / 2;
  else if (alignment == 2)
    drawX = boxWidth > 0 ? maxWidth - lw : maxWidth - lw - xOffset;
  else
    drawX = xOffset;
  x11_draw_utf8_fallback(dpy, xd, color, drawX, y, xf, lineStart, line_len, 0);
}

void x11_draw_wrapped_text(Display *dpy, XftDraw *xd, XftColor *color,
                           FontHandle font, const char *text, int startY,
                           char *centered_to, int xOffset, int boxWidth) {
  if (!text) {
    return;
  }
  XftFont *xf = FH_TO_XFT(font);
  if (!xf) {
    return;
  }
  int lineHeight = xf->ascent + xf->descent;
  int spaceWidth = xft_width(dpy, xf, " ", 1);
  int y = startY + xf->ascent;
  int text_len = strlen(text);
  int maxWidth = 0;
  int alignment = 0;

  if (centered_to != NULL) {
    if (String_ContainsCI(centered_to, "CENTER")) {
      alignment = 1;
      maxWidth = boxWidth > 0 ? boxWidth : PROMPT_WRAP_BOX_CENTER;
    } else if (String_ContainsCI(centered_to, "RIGHT")) {
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

  const char *p = text;
  const char *lineStart = text;
  int lineWidth = 0;

  while (*p) {
    const char *word_start = p;
    while (*p && *p != ' ')
      p++;
    int wordWidth = x11_measure_utf8_fallback(dpy, xf, word_start,
                                              (int)(p - word_start), 0);
    int add = wordWidth + (lineWidth > 0 ? spaceWidth : 0);

    if (lineWidth > 0 && lineWidth + add > maxWidth) {
      x11_wrap_flush_line(dpy, xd, color, xf, lineStart,
                          (int)(word_start - lineStart), alignment, boxWidth,
                          maxWidth, xOffset, y);
      y += lineHeight;
      lineStart = word_start;
      lineWidth = wordWidth;
    } else {
      lineWidth += add;
    }
    while (*p == ' ')
      p++;
  }

  if (p > lineStart) {
    x11_wrap_flush_line(dpy, xd, color, xf, lineStart, (int)(p - lineStart),
                        alignment, boxWidth, maxWidth, xOffset, y);
  }
}

void x11_init_argb_image(Display *dpy, XImage *img, unsigned char *data, int w,
                         int h, unsigned long red_mask,
                         unsigned long green_mask, unsigned long blue_mask) {
  memset(img, 0, sizeof(*img));
  img->width = w;
  img->height = h;
  img->xoffset = 0;
  img->format = ZPixmap;
  img->data = (char *)data;
  img->byte_order = ImageByteOrder(dpy);
  img->bitmap_unit = BitmapUnit(dpy);
  img->bitmap_bit_order = BitmapBitOrder(dpy);
  img->bitmap_pad = 32;
  img->depth = 32;
  img->bytes_per_line = w * 4;
  img->bits_per_pixel = 32;
  img->red_mask = red_mask;
  img->green_mask = green_mask;
  img->blue_mask = blue_mask;
  img->obdata = NULL;
}

void x11_xft_alloc_color(Display *dpy, Visual *vis, Colormap cmap,
                         unsigned long hex, unsigned short alpha,
                         XftColor *out) {
  XRenderColor rc = {((hex >> 16) & 0xFF) * 0x101, ((hex >> 8) & 0xFF) * 0x101,
                     (hex & 0xFF) * 0x101, alpha};
  XftColorAllocValue(dpy, vis, cmap, &rc, out);
}

const char *x11_listbox_item_display_text(ListBoxInfo *lb, int idx, char *trunc,
                                          size_t trunc_size) {
  const char *baseText = lb ? ListBoxInfo_Row(lb, idx) : NULL;
  snprintf(trunc, trunc_size, "%.80s", baseText ? baseText : "");
  return trunc;
}

#define IMG_CACHE_SLOTS 256
#define X11_IMG_REVALIDATE_MS 1000

static bool x11_blit_argb(Display *dpy, Drawable drw, Visual *vis,
                          unsigned char *argb, int w, int h, int x, int y);

typedef struct {
  char key[512];
  int w, h;
  long long mtime;
  long long size;
  unsigned char *argb;
  unsigned long stamp;
  long long checked_ms;
} ImgCacheSlot;

static long long x11_now_ms(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    return 0;
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static ImgCacheSlot g_img_cache[IMG_CACHE_SLOTS];
static unsigned long g_img_cache_tick = 0;
static pthread_mutex_t img_cache_mtx = PTHREAD_MUTEX_INITIALIZER;

void x11_img_cache_free_all(void) {
  pthread_mutex_lock(&img_cache_mtx);
  for (int i = 0; i < IMG_CACHE_SLOTS; i++) {
    if (g_img_cache[i].argb)
      free(g_img_cache[i].argb);
    memset(&g_img_cache[i], 0, sizeof(g_img_cache[i]));
  }
  pthread_mutex_unlock(&img_cache_mtx);
}

static unsigned char *x11_img_cache_get(const char *key, int w, int h) {
  long long now = x11_now_ms();
  for (int i = 0; i < IMG_CACHE_SLOTS; i++) {
    ImgCacheSlot *s = &g_img_cache[i];
    if (!s->argb)
      continue;
    if (s->w != w || s->h != h)
      continue;
    if (strcmp(s->key, key) != 0)
      continue;
    if (now - s->checked_ms >= X11_IMG_REVALIDATE_MS) {
      struct stat st;
      s->checked_ms = now;
      if (stat(key, &st) != 0 || (long long)st.st_mtime != s->mtime ||
          (long long)st.st_size != s->size) {
        free(s->argb);
        s->argb = NULL;
        continue;
      }
    }
    s->stamp = ++g_img_cache_tick;
    return s->argb;
  }
  return NULL;
}

static void x11_img_cache_put(const char *key, int w, int h, long long mtime,
                              long long size, unsigned char *argb) {
  if (!argb)
    return;
  int slot = -1;
  unsigned long oldest = ~0UL;
  for (int i = 0; i < IMG_CACHE_SLOTS; i++) {
    const ImgCacheSlot *s = &g_img_cache[i];
    if (!s->argb) {
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
  ImgCacheSlot *s = &g_img_cache[slot];
  if (s->argb)
    free(s->argb);
  snprintf(s->key, sizeof(s->key), "%s", key);
  s->w = w;
  s->h = h;
  s->mtime = mtime;
  s->size = size;
  s->argb = argb;
  s->checked_ms = x11_now_ms();
  s->stamp = ++g_img_cache_tick;
}

bool x11_draw_image_scaled(Display *dpy, XftDraw *xd, Visual *vis,
                           const char *path, int x, int y, int tw, int th) {
  if (!dpy || !xd || !path || !path[0])
    return false;
  if (tw <= 0)
    tw = IconSize;
  if (th <= 0)
    th = IconSize;
  int cw = tw;
  int ch = th;
  unsigned char *argb = NULL;
  pthread_mutex_lock(&img_cache_mtx);
  argb = x11_img_cache_get(path, cw, ch);
  if (argb) {
    pthread_mutex_unlock(&img_cache_mtx);
    Drawable drw = XftDrawDrawable(xd);
    return x11_blit_argb(dpy, drw, vis, argb, cw, ch, x, y);
  }
  pthread_mutex_unlock(&img_cache_mtx);
  struct stat st;
  if (stat(path, &st) != 0)
    return false;
  argb = x11_decode_image_argb(path, &cw, &ch);
  if (!argb)
    return false;
  pthread_mutex_lock(&img_cache_mtx);
  x11_img_cache_put(path, cw, ch, (long long)st.st_mtime, (long long)st.st_size,
                    argb);
  pthread_mutex_unlock(&img_cache_mtx);
  Drawable drw = XftDrawDrawable(xd);
  return x11_blit_argb(dpy, drw, vis, argb, cw, ch, x, y);
}

static bool x11_blit_argb(Display *dpy, Drawable drw, Visual *vis,
                          unsigned char *argb, int w, int h, int x, int y) {
  if (!dpy || !argb || w <= 0 || h <= 0)
    return false;
  int first_opaque_row = -1;
  for (int ry = 0; ry < h && first_opaque_row < 0; ry++) {
    for (int rx = 0; rx < w; rx++) {
      if (argb[(ry * w + rx) * 4 + 3] > 0) {
        first_opaque_row = ry;
        break;
      }
    }
  }
  if (!vis)
    vis = DefaultVisualOfScreen(DefaultScreenOfDisplay(dpy));
  XImage img;
  x11_init_argb_image(dpy, &img, argb, w, h, vis->red_mask, vis->green_mask,
                      vis->blue_mask);
  if (!XInitImage(&img))
    return false;
  Pixmap pix = XCreatePixmap(dpy, drw, w, h, 32);
  if (!pix)
    return false;
  GC gc = XCreateGC(dpy, pix, 0, NULL);
  XPutImage(dpy, pix, gc, &img, 0, 0, 0, 0, w, h);
  XFreeGC(dpy, gc);
  XRenderPictFormat *src_fmt =
      XRenderFindStandardFormat(dpy, PictStandardARGB32);
  XRenderPictFormat *dst_fmt = XRenderFindVisualFormat(dpy, vis);
  bool ok = false;
  if (src_fmt && dst_fmt) {
    Picture src = XRenderCreatePicture(dpy, pix, src_fmt, 0, NULL);
    Picture dst = XRenderCreatePicture(dpy, drw, dst_fmt, 0, NULL);
    if (src && dst) {
      XRenderComposite(dpy, PictOpOver, src, None, dst, 0, 0, 0, 0, x, y, w, h);
      ok = true;
    }
    if (src)
      XRenderFreePicture(dpy, src);
    if (dst)
      XRenderFreePicture(dpy, dst);
  }
  XFreePixmap(dpy, pix);
  return ok;
}

static unsigned char *xpm_load(const char *path, int *out_w, int *out_h) {
  FILE *f = fopen(path, "r");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  if (sz <= 0) {
    fclose(f);
    return NULL;
  }
  fseek(f, 0, SEEK_SET);
  char *buf = (char *)malloc((size_t)sz + 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  size_t rd = fread(buf, 1, (size_t)sz, f);
  buf[rd] = '\0';
  const char *buf_end = buf + rd;
  fclose(f);

  int w = 0, h = 0, ncolors = 0, cpp = 1;
  char *p = buf;
  while (*p && *p != '"')
    p++;
  if (*p == '"') {
    p++;
    if (sscanf(p, "%d %d %d %d", &w, &h, &ncolors, &cpp) < 3) {
      free(buf);
      return NULL;
    }
    if (cpp < 1)
      cpp = 1;
    if (cpp > 15) {
      free(buf);
      return NULL;
    }
    if (ncolors < 1) {
      free(buf);
      return NULL;
    }
  }
  if (w <= 0 || h <= 0 || w > 8192 || h > 8192 || ncolors > 65536) {
    free(buf);
    return NULL;
  }

  char (*keys)[16] = (char (*)[16])calloc((size_t)ncolors, 16);
  unsigned char *colors = (unsigned char *)calloc((size_t)ncolors * 4, 1);
  if (!keys || !colors) {
    free(buf);
    free(keys);
    free(colors);
    return NULL;
  }

  char *q = strchr(p, '"');
  if (q)
    p = q + 1;

  int ci = 0;
  while (ci < ncolors) {
    while (*p && *p != '"')
      p++;
    if (!*p)
      break;
    p++;
    if (buf_end - p < (ptrdiff_t)cpp)
      break;
    char keybuf[16] = {0};
    strncpy(keybuf, p, (size_t)cpp);
    keybuf[cpp] = '\0';
    char *cp = p;
    cp += cpp;
    while (*cp == ' ' || *cp == '\t')
      cp++;
    if (*cp == 'c') {
      cp++;
      while (*cp == ' ' || *cp == '\t')
        cp++;
    }
    char colorname[64] = {0};
    int ci2 = 0;
    while (cp[ci2] && cp[ci2] != '"' && cp[ci2] != ' ' && cp[ci2] != '\t' &&
           ci2 < 63) {
      colorname[ci2] = cp[ci2];
      ci2++;
    }
    colorname[ci2] = '\0';
    unsigned char r = 0, g = 0, b = 0, a = 255;
    if (strcasecmp(colorname, "None") == 0 || colorname[0] == '\0') {
      a = 0;
    } else if (colorname[0] == '#') {
      unsigned int val = 0;
      if (strlen(colorname) == 7) {
        sscanf(colorname + 1, "%06x", &val);
        r = (val >> 16) & 0xff;
        g = (val >> 8) & 0xff;
        b = val & 0xff;
      } else if (strlen(colorname) == 4) {
        sscanf(colorname + 1, "%03x", &val);
        r = ((val >> 8) & 0xf) * 17;
        g = ((val >> 4) & 0xf) * 17;
        b = (val & 0xf) * 17;
      }
    }
    SafeCopy(keys[ci], 16, keybuf);
    colors[ci * 4 + 0] = b;
    colors[ci * 4 + 1] = g;
    colors[ci * 4 + 2] = r;
    colors[ci * 4 + 3] = a;
    char *endq = strchr(p, '"');
    if (endq)
      p = endq + 1;
    ci++;
  }

  unsigned char *out = (unsigned char *)calloc((size_t)w * h * 4, 1);
  if (!out) {
    free(buf);
    free(keys);
    free(colors);
    return NULL;
  }
  int row = 0;
  while (row < h && *p) {
    while (*p && *p != '"')
      p++;
    if (!*p)
      break;
    p++;
    const char *row_end = strchr(p, '"');
    if (!row_end || (size_t)(row_end - p) < (size_t)w * (size_t)cpp)
      break;
    for (int x = 0; x < w; x++) {
      char keybuf[16] = {0};
      strncpy(keybuf, p + x * cpp, (size_t)cpp);
      keybuf[cpp] = '\0';
      int idx = -1;
      for (int c = 0; c < ncolors; c++) {
        if (strcmp(keys[c], keybuf) == 0) {
          idx = c;
          break;
        }
      }
      if (idx >= 0) {
        out[(row * w + x) * 4 + 0] = colors[idx * 4 + 0];
        out[(row * w + x) * 4 + 1] = colors[idx * 4 + 1];
        out[(row * w + x) * 4 + 2] = colors[idx * 4 + 2];
        out[(row * w + x) * 4 + 3] = colors[idx * 4 + 3];
      }
    }
    p = (char *)row_end + 1;
    row++;
  }
  free(buf);
  free(keys);
  free(colors);
  *out_w = w;
  *out_h = h;
  return out;
}

typedef struct {
  char path[512];
  unsigned char *argb;
  int w;
  int h;
  int dst_w;
  int dst_h;
  struct stat st;
  bool has_stat;
} DecodedCacheSlot;

static DecodedCacheSlot decoded_cache[32];
static int decoded_count = 0;

unsigned char *x11_decode_image_argb(const char *path, int *out_w, int *out_h) {
  if (!path || !path[0])
    return NULL;
  int dst_w = *out_w;
  int dst_h = *out_h;
  if (dst_w <= 0)
    dst_w = 16;
  if (dst_h <= 0)
    dst_h = 16;

  struct stat st;
  memset(&st, 0, sizeof(st));
  bool has_stat = (stat(path, &st) == 0);

  for (int i = 0; i < decoded_count; i++) {
    DecodedCacheSlot *slot = &decoded_cache[i];
    if (slot->dst_w == dst_w && slot->dst_h == dst_h &&
        strcmp(slot->path, path) == 0) {
      bool fresh =
          !has_stat || (slot->has_stat && slot->st.st_mtime == st.st_mtime &&
                        slot->st.st_size == st.st_size);
      if (!fresh)
        continue;
      size_t bytes = (size_t)dst_w * dst_h * 4;
      unsigned char *copy = (unsigned char *)malloc(bytes);
      if (!copy)
        return NULL;
      memcpy(copy, slot->argb, bytes);
      *out_w = dst_w;
      *out_h = dst_h;
      return copy;
    }
  }

  size_t _plen = strlen(path);
  bool _is_svg = _plen > 4 && String_ContainsCI(path + _plen - 4, ".svg");

  unsigned char *argb = NULL;
  if (_is_svg) {
    NSVGimage *image = nsvgParseFromFile(path, "px", 96.0f);
    if (!image)
      return NULL;
    if (image->width <= 0 || image->height <= 0) {
      nsvgDelete(image);
      return NULL;
    }
    NSVGrasterizer *rast = nsvgCreateRasterizer();
    if (!rast) {
      nsvgDelete(image);
      return NULL;
    }
    argb = (unsigned char *)calloc((size_t)dst_w * dst_h * 4, 1);
    if (!argb) {
      nsvgDeleteRasterizer(rast);
      nsvgDelete(image);
      return NULL;
    }
    nsvgRasterize(rast, image, 0, 0, (float)dst_w / image->width, argb, dst_w,
                  dst_h, dst_w * 4);
    for (int i = 0; i < dst_w * dst_h; i++) {
      unsigned char r = argb[i * 4 + 0];
      unsigned char g = argb[i * 4 + 1];
      unsigned char b = argb[i * 4 + 2];
      unsigned char a = argb[i * 4 + 3];
      unsigned int pr = (unsigned int)r * a / 255;
      unsigned int pg = (unsigned int)g * a / 255;
      unsigned int pb = (unsigned int)b * a / 255;
      argb[i * 4 + 0] = (unsigned char)pb;
      argb[i * 4 + 1] = (unsigned char)pg;
      argb[i * 4 + 2] = (unsigned char)pr;
      argb[i * 4 + 3] = a;
    }
    nsvgDeleteRasterizer(rast);
    nsvgDelete(image);
  } else {
    bool _is_xpm = _plen > 4 && String_ContainsCI(path + _plen - 4, ".xpm");
    int w = 0, h = 0;
    int comp = 0;
    unsigned char *src = NULL;
    if (_is_xpm) {
      src = xpm_load(path, &w, &h);
    } else {
      src = stbi_load(path, &w, &h, &comp, 4);
      if (!src) {
        log_error("img: cannot decode '%s': %s", path, stbi_failure_reason());
        return NULL;
      }
    }
    if (!src)
      return NULL;

    for (int i = 0; i < w * h; i++) {
      unsigned char r = src[i * 4 + 0];
      unsigned char g = src[i * 4 + 1];
      unsigned char b = src[i * 4 + 2];
      unsigned char a = src[i * 4 + 3];
      src[i * 4 + 0] = (unsigned char)((unsigned int)b * a / 255);
      src[i * 4 + 1] = (unsigned char)((unsigned int)g * a / 255);
      src[i * 4 + 2] = (unsigned char)((unsigned int)r * a / 255);
      src[i * 4 + 3] = a;
    }

    argb = (unsigned char *)malloc((size_t)dst_w * dst_h * 4);
    if (!argb) {
      free(src);
      return NULL;
    }

    for (int dy = 0; dy < dst_h; dy++) {
      int y0 = (int)((long long)dy * h / dst_h);
      int y1 = (int)((long long)(dy + 1) * h / dst_h);
      if (y1 <= y0)
        y1 = y0 + 1;
      if (y1 > h)
        y1 = h;
      unsigned char *dst_row = argb + (size_t)dy * dst_w * 4;
      for (int dx = 0; dx < dst_w; dx++) {
        int x0 = (int)((long long)dx * w / dst_w);
        int x1 = (int)((long long)(dx + 1) * w / dst_w);
        if (x1 <= x0)
          x1 = x0 + 1;
        if (x1 > w)
          x1 = w;
        long long sr = 0, sg = 0, sb = 0, sa = 0, n = 0;
        for (int sy = y0; sy < y1; sy++) {
          const unsigned char *src_row = src + (size_t)sy * w * 4;
          for (int sx = x0; sx < x1; sx++) {
            const unsigned char *s = src_row + (size_t)sx * 4;
            sb += s[0];
            sg += s[1];
            sr += s[2];
            sa += s[3];
            n++;
          }
        }
        unsigned char *d = dst_row + (size_t)dx * 4;
        d[0] = (unsigned char)(sb / n);
        d[1] = (unsigned char)(sg / n);
        d[2] = (unsigned char)(sr / n);
        d[3] = (unsigned char)(sa / n);
      }
    }
    free(src);
  }

  int idx = decoded_count;
  if (idx >= 32) {
    free(decoded_cache[0].argb);
    for (int i = 0; i < 31; i++)
      decoded_cache[i] = decoded_cache[i + 1];
    idx = 31;
  } else {
    decoded_count++;
  }
  snprintf(decoded_cache[idx].path, sizeof(decoded_cache[idx].path), "%s",
           path);
  decoded_cache[idx].argb = argb;
  decoded_cache[idx].w = dst_w;
  decoded_cache[idx].h = dst_h;
  decoded_cache[idx].dst_w = dst_w;
  decoded_cache[idx].dst_h = dst_h;
  decoded_cache[idx].st = st;
  decoded_cache[idx].has_stat = has_stat;

  unsigned char *copy = (unsigned char *)malloc((size_t)dst_w * dst_h * 4);
  if (!copy)
    return NULL;
  memcpy(copy, argb, (size_t)dst_w * dst_h * 4);
  *out_w = dst_w;
  *out_h = dst_h;
  return copy;
}

void x11_draw_listbox_item_text(Display *dpy, XftDraw *xd, Visual *vis,
                                Colormap cmap, FontHandle font,
                                XftColor *plain_color, ListBoxInfo *lb,
                                const char *icon_path, int idx, int y) {
  const XftFont *xf = FH_TO_XFT(font);
  if (!xf)
    return;
  int XOffset = PROMPT_ITEM_TEXT_X;
  int row_h = PromptRowHeight();
  int pad_top = (row_h - (xf->ascent + xf->descent)) / 2;
  if (pad_top < 0)
    pad_top = 0;
  int text_y = y + pad_top;
  if (icon_path) {
    int iconX = XOffset;
    int iconY = y + (PromptRowHeight() - IconSize) / 2;
    XOffset += IconSize + XOffset;
    x11_draw_image_scaled(dpy, xd, vis, icon_path, iconX, iconY, IconSize,
                          IconSize);
  }
  char trunc[512];
  x11_listbox_item_display_text(lb, idx, trunc, sizeof(trunc));

  const char *draw_text = trunc;
  int draw_x = XOffset;
  int h_scroll = (lb && idx == lb->selected_index) ? lb->text_hscroll : 0;
  if (h_scroll > 0)
    draw_x -= h_scroll;

  unsigned long pcol = plain_color ? plain_color->pixel : 0;
  if (!pcol)
    pcol = RgbToHex(COLOR_TEXT);
  x11_set_draw_context(dpy, xd, vis, cmap);
  Platform_DrawTextRaw((PlDrawTextRawSpec){.x = draw_x,
                                           .y = text_y,
                                           .text = draw_text,
                                           .color = pcol,
                                           .font = font,
                                           .align = "LEFT",
                                           .pad = XOffset,
                                           .fontsize = FontSize});
}

static Window x11_create_window_ex_or(Display *dpy, int x, int y, int w, int h,
                                      unsigned long bg_color,
                                      const char *res_name,
                                      const char *res_class, long event_mask,
                                      bool override_redirect);

static void x11_set_net_wm_state_skip(Display *dpy, Window win) {
  Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
  Atom state_skip_taskbar =
      XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
  Atom state_skip_pager = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
  Atom states[2] = {state_skip_taskbar, state_skip_pager};
  XChangeProperty(dpy, win, wm_state, XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)states, 2);
}

static void x11_set_fixed_size_hints(Display *dpy, Window win, int w, int h) {
  XSizeHints *sh = XAllocSizeHints();
  sh->flags = PMinSize | PMaxSize;
  sh->min_width = w;
  sh->min_height = h;
  sh->max_width = w;
  sh->max_height = h;
  XSetWMNormalHints(dpy, win, sh);
  XFree(sh);
}

Window x11_create_window_ex(Display *dpy, int x, int y, int w, int h,
                            unsigned long bg_color, const char *res_name,
                            const char *res_class, long event_mask) {
  return x11_create_window_ex_or(dpy, x, y, w, h, bg_color, res_name, res_class,
                                 event_mask, false);
}

static void x11_apply_shape_rounded(Display *dpy, Window win, int w, int h,
                                    int radius);
static unsigned int x11_corner_alpha(int x, int y, int w, int h, int R,
                                     unsigned int baseAlpha8);

static Window x11_create_window_ex_or(Display *dpy, int x, int y, int w, int h,
                                      unsigned long bg_color,
                                      const char *res_name,
                                      const char *res_class, long event_mask,
                                      bool override_redirect) {
  int screen = DefaultScreen(dpy);
  XVisualInfo vinfo;
  Visual *visual = NULL;
  int depth = 0;
  Colormap cmap = 0;
  bool use_argb = false;
  static __thread Colormap cached_cmap = None;
  static __thread const Visual *cached_visual = NULL;
  static __thread int cached_screen = -1;
  if (XMatchVisualInfo(dpy, screen, 32, TrueColor, &vinfo)) {
    visual = vinfo.visual;
    depth = 32;
    if (cached_cmap == None || cached_screen != screen ||
        cached_visual != visual) {
      if (cached_cmap != None)
        XFreeColormap(dpy, cached_cmap);
      cached_cmap =
          XCreateColormap(dpy, RootWindow(dpy, screen), visual, AllocNone);
      cached_visual = visual;
      cached_screen = screen;
    }
    cmap = cached_cmap;
    use_argb = true;
  }
  Window win;
  Pixmap bg_pm = (GetTransparentWindow() || !x11_bg_image_enabled())
                     ? None
                     : x11_bg_image_tile(dpy, w, h);
  if (use_argb) {
    XSetWindowAttributes attrs = {0};
    unsigned int r = (bg_color >> 16) & 0xFF;
    unsigned int g = (bg_color >> 8) & 0xFF;
    unsigned int b = bg_color & 0xFF;
    unsigned int a = GetTransparentWindow() ? 0 : (unsigned int)WindowOpacity;
    unsigned long valuemask = CWColormap | CWBorderPixel;
    if (bg_pm != None) {
      attrs.background_pixmap = bg_pm;
      valuemask |= CWBackPixmap;
    } else {
      attrs.background_pixel = (a << 24) | (r << 16) | (g << 8) | b;
      valuemask |= CWBackPixel;
    }
    attrs.colormap = cmap;
    attrs.border_pixel = 0;
    if (override_redirect) {
      attrs.override_redirect = True;
      valuemask |= CWOverrideRedirect;
    }
    win = XCreateWindow(dpy, RootWindow(dpy, screen), x, y, w, h, 0, depth,
                        InputOutput, visual, valuemask, &attrs);
  } else {
    XSetWindowAttributes attrs = {0};
    unsigned long valuemask = CWBorderPixel;
    if (bg_pm != None) {
      attrs.background_pixmap = bg_pm;
      valuemask |= CWBackPixmap;
    } else {
      attrs.background_pixel = bg_color;
      valuemask |= CWBackPixel;
    }
    attrs.border_pixel = bg_color;
    if (override_redirect) {
      attrs.override_redirect = True;
      valuemask |= CWOverrideRedirect;
    }
    win = XCreateWindow(dpy, RootWindow(dpy, screen), x, y, w, h, 0,
                        DefaultDepth(dpy, screen), InputOutput,
                        DefaultVisual(dpy, screen), valuemask, &attrs);
  }
  if (!override_redirect) {
    Atom wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom wm_type_dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(dpy, win, wm_type, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&wm_type_dialog, 1);
    Atom motif_atom = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    if (motif_atom != None) {
      struct {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long input_mode;
        unsigned long status;
      } motif;
      motif.flags = 2;
      motif.functions = 0;
      motif.decorations = 0;
      motif.input_mode = 0;
      motif.status = 0;
      XChangeProperty(dpy, win, motif_atom, motif_atom, 32, PropModeReplace,
                      (unsigned char *)&motif, 5);
    }
    x11_set_net_wm_state_skip(dpy, win);
  } else if (res_name && strcmp(res_name, "flashtext") == 0) {
    Atom wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom wm_type_note =
        XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
    XChangeProperty(dpy, win, wm_type, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&wm_type_note, 1);
    x11_set_net_wm_state_skip(dpy, win);
  }
  if (res_name || res_class) {
    XClassHint *ch = XAllocClassHint();
    ch->res_name = (char *)(res_name ? res_name : "colored");
    ch->res_class = (char *)(res_class ? res_class : "colored");
    XSetClassHint(dpy, win, ch);
    XFree(ch);
  }
  XWMHints *hints = XAllocWMHints();
  hints->flags = InputHint;
  hints->input = True;
  if (res_name && strcmp(res_name, "flashtext") == 0)
    hints->input = False;
  XSetWMHints(dpy, win, hints);
  XFree(hints);
  if (override_redirect && niri_is_active()) {
    x11_set_fixed_size_hints(dpy, win, w, h);
  }
  XSelectInput(dpy, win, event_mask);
  if (RoundedCorners && !override_redirect && CornerRadius > 0) {
    x11_apply_shape_rounded(dpy, win, w, h, CornerRadius);
  }
  return win;
}

static void x11_apply_shape_rounded(Display *dpy, Window win, int w, int h,
                                    int radius) {
  if (radius <= 0 || w <= 0 || h <= 0)
    return;
  Pixmap mask = XCreatePixmap(dpy, win, w, h, 1);
  GC gc = XCreateGC(dpy, mask, 0, NULL);
  XSetForeground(dpy, gc, 0);
  XFillRectangle(dpy, mask, gc, 0, 0, w, h);
  XSetForeground(dpy, gc, 1);

  XFillRectangle(dpy, mask, gc, radius, 0, w - 2 * radius, h);
  XFillRectangle(dpy, mask, gc, 0, radius, w, h - 2 * radius);

  if (radius > w / 2)
    radius = w / 2;
  if (radius > h / 2)
    radius = h / 2;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      if (x >= radius && x < w - radius)
        continue;
      if (y >= radius && y < h - radius)
        continue;
      if (x11_corner_alpha(x, y, w, h, radius, 255) == 0)
        continue;
      XFillRectangle(dpy, mask, gc, x, y, 1, 1);
    }
  }
  XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
  XSync(dpy, False);
  XFreePixmap(dpy, mask);
  XFreeGC(dpy, gc);
}

static unsigned long x11_background_color_at(void) {
  unsigned long base = GetDrawBgColor();
  if (!base)
    base = RgbToHex(COLOR_BG);
  return ((base >> 16) & 0xFF) << 16 | ((base >> 8) & 0xFF) << 8 |
         (base & 0xFF);
}

void x11_fill_rounded_rect(Display *dpy, XftDraw *xd, Visual *vis,
                           Colormap cmap, unsigned long hex,
                           unsigned short alpha16, int x, int y, int w, int h,
                           int radius) {
  if (!RoundedCorners || radius <= 0 || w <= 0 || h <= 0) {
    XftColor c;
    x11_xft_alloc_color(dpy, vis, cmap, hex, alpha16, &c);
    XftDrawRect(xd, &c, x, y, w, h);
    XftColorFree(dpy, vis, cmap, &c);
    return;
  }
  if (radius > w / 2)
    radius = w / 2;
  if (radius > h / 2)
    radius = h / 2;
  XftColor solid;
  x11_xft_alloc_color(dpy, vis, cmap, hex, alpha16, &solid);

  XftDrawRect(xd, &solid, x + radius, y, w - 2 * radius, h);
  XftDrawRect(xd, &solid, x, y + radius, radius, h - 2 * radius);
  XftDrawRect(xd, &solid, x + w - radius, y + radius, radius, h - 2 * radius);

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
        float d_sq = ddx2 * ddx2 + ddy2 * ddy2;
        if (d_sq >= edge_sq_hi)
          continue;
        unsigned short a;
        if (d_sq <= edge_sq_lo) {
          a = alpha16;
        } else {
          float d = sqrtf(d_sq);
          float cov = (edge_hi - d);
          if (cov < 0.0f)
            cov = 0.0f;
          if (cov > 1.0f)
            cov = 1.0f;
          a = (unsigned short)((float)alpha16 * cov + 0.5f);
        }
        if (a == 0)
          continue;
        float cov = (float)a / (float)(alpha16 ? alpha16 : 1);
        if (cov > 1.0f)
          cov = 1.0f;
        unsigned long backdrop = x11_background_color_at();
        unsigned long mixed = Color_Lerp(backdrop, hex, cov);
        XftColor px_c = solid;
        px_c.color.red = (unsigned short)(((mixed >> 16) & 0xFF) * 257);
        px_c.color.green = (unsigned short)(((mixed >> 8) & 0xFF) * 257);
        px_c.color.blue = (unsigned short)((mixed & 0xFF) * 257);
        px_c.color.alpha = 0xFFFF;
        XftDrawRect(xd, &px_c, ox0 + dx, oy0 + dy, 1, 1);
      }
    }
  }
  XftColorFree(dpy, vis, cmap, &solid);
}

static unsigned int x11_corner_alpha(int x, int y, int w, int h, int R,
                                     unsigned int baseAlpha8) {
  float cx, cy;
  if (x < R && y < R) {
    cx = (float)R;
    cy = (float)R;
  } else if (x >= w - R && y < R) {
    cx = (float)(w - R - 1);
    cy = (float)R;
  } else if (x < R && y >= h - R) {
    cx = (float)R;
    cy = (float)(h - R - 1);
  } else if (x >= w - R && y >= h - R) {
    cx = (float)(w - R - 1);
    cy = (float)(h - R - 1);
  } else {
    return baseAlpha8;
  }
  const int SS = 16;
  int inside = 0;
  for (int sy = 0; sy < SS; sy++) {
    for (int sx = 0; sx < SS; sx++) {
      float px = (float)x + ((float)sx + 0.5f) / (float)SS;
      float py = (float)y + ((float)sy + 0.5f) / (float)SS;
      float dx = px - (cx + 0.5f);
      float dy = py - (cy + 0.5f);
      if (dx * dx + dy * dy <= (float)R * (float)R)
        inside++;
    }
  }
  const int total = SS * SS;
  if (inside <= 0)
    return 0;
  if (inside >= total)
    return baseAlpha8;
  return (unsigned int)((float)baseAlpha8 * ((float)inside / (float)total) +
                        0.5f);
}

static void x11_generate_background_pixels(uint32_t *img_data,
                                           int bytes_per_line, int w, int h,
                                           int R, unsigned int baseAlpha8,
                                           unsigned long base) {
  uint32_t *row;
  unsigned int r = (base >> 16) & 0xFF, g = (base >> 8) & 0xFF, b = base & 0xFF;
  for (int y = 0; y < h; y++) {
    row = (uint32_t *)((char *)img_data + y * bytes_per_line);
    for (int x = 0; x < w; x++) {
      unsigned int a = x11_corner_alpha(x, y, w, h, R, baseAlpha8);
      row[x] = a ? (a << 24) | (r << 16) | (g << 8) | b : 0;
    }
  }
}

typedef struct {
  unsigned long base;
  int radius;
  bool rounded;
  int w, h;
} BgKey;

static bool bg_slot_matches(const BgSlot *slot, const BgKey *k) {
  return slot->used && slot->dpy != NULL && slot->base == k->base &&
         slot->radius == k->radius && slot->rounded == k->rounded &&
         slot->w == k->w && slot->h == k->h;
}

static Pixmap bg_cache_find(Display *dpy, const BgKey *k) {
  for (int i = 0; i < BG_SLOT_COUNT; i++) {
    if (bg_slots[i].dpy == dpy && bg_slot_matches(&bg_slots[i], k))
      return bg_slots[i].pm;
  }
  return None;
}

static void bg_cache_store(Display *dpy, Pixmap pm, const BgKey *k) {
  BgSlot *slot = &bg_slots[bg_slot_next];
  bg_slot_next = (bg_slot_next + 1) % BG_SLOT_COUNT;
  if (slot->used && slot->pm != None)
    XFreePixmap(slot->dpy, slot->pm);
  slot->used = true;
  slot->pm = pm;
  slot->dpy = dpy;
  slot->w = k->w;
  slot->h = k->h;
  slot->base = k->base;
  slot->radius = k->radius;
  slot->rounded = k->rounded;
}

static Pixmap x11_create_background_pixmap(Display *dpy, Window win, int w,
                                           int h, const XWindowAttributes *attrs,
                                           unsigned long base,
                                           unsigned int baseAlpha8) {
  Visual *vis = attrs->visual;
  int depth = attrs->depth;
  if (depth != 32)
    return None;
  bool hasAlpha = true;
  bool rounded = RoundedCorners && CornerRadius > 0 && hasAlpha;
  int R = rounded ? CornerRadius : 0;
  if (R > w / 2)
    R = w / 2;
  if (R > h / 2)
    R = h / 2;
  Pixmap pm = XCreatePixmap(dpy, win, w, h, depth);
  if (!pm)
    return None;
  size_t bpl = (((size_t)w * (size_t)depth + 7u) / 8u + 3u) & ~(size_t)3u;
  char *pixbuf = (char *)calloc(1, bpl * (size_t)h);
  if (!pixbuf) {
    XFreePixmap(dpy, pm);
    return None;
  }
  XImage *img =
      XCreateImage(dpy, vis, depth, ZPixmap, 0, pixbuf, w, h, 32, (int)bpl);
  if (!img) {
    free(pixbuf);
    XFreePixmap(dpy, pm);
    return None;
  }

  x11_generate_background_pixels((uint32_t *)img->data, img->bytes_per_line, w,
                                 h, R, baseAlpha8, base);

  GC gc = XCreateGC(dpy, pm, 0, NULL);
  XPutImage(dpy, pm, gc, img, 0, 0, 0, 0, w, h);
  XFreeGC(dpy, gc);
  XDestroyImage(img);
  if (x11_bg_image_enabled()) {
    x11_bg_image_blit(dpy, pm, vis, w, h);
  }
  if (rounded && R > 0) {
    XImage *img2 = XGetImage(dpy, pm, 0, 0, w, h, AllPlanes, ZPixmap);
    if (img2) {
      for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
          unsigned int a = x11_corner_alpha(x, y, w, h, R, baseAlpha8);
          if (a < baseAlpha8) {
            uint32_t pixel = XGetPixel(img2, x, y);
            unsigned int alpha = (pixel >> 24) & 0xFF;
            unsigned int new_alpha = (alpha * a + 127) / 255;
            unsigned int r_c = (pixel >> 16) & 0xFF;
            unsigned int c = (pixel >> 8) & 0xFF;
            unsigned int b_c = pixel & 0xFF;
            unsigned int pm_r = (r_c * new_alpha + 127) / 255;
            unsigned int pm_g = (c * new_alpha + 127) / 255;
            unsigned int pm_b = (b_c * new_alpha + 127) / 255;
            XPutPixel(img2, x, y,
                      (new_alpha << 24) | (pm_r << 16) | (pm_g << 8) | pm_b);
          }
        }
      }
      GC gc2 = XCreateGC(dpy, pm, 0, NULL);
      XPutImage(dpy, pm, gc2, img2, 0, 0, 0, 0, w, h);
      XFreeGC(dpy, gc2);
      XDestroyImage(img2);
    }
  }
  return pm;
}

Pixmap x11_get_cached_background(Display *dpy, Window win, int w, int h) {
  if (!dpy || w <= 0 || h <= 0)
    return None;
  unsigned long base = GetDrawBgColor();
  if (!base)
    base = RgbToHex(COLOR_BG);
  int radius = CornerRadius;
  bool rounded = RoundedCorners;
  if (!rounded && radius > 0 && !x11_bg_image_enabled()) {
    return None;
  }
  BgKey k;
  k.base = base;
  k.radius = radius;
  k.rounded = rounded;
  k.w = w;
  k.h = h;
  Pixmap hit = bg_cache_find(dpy, &k);
  if (hit) {
    return hit;
  }
  XWindowAttributes attrs;
  if (!XGetWindowAttributes(dpy, win, &attrs))
    return None;
  unsigned int baseAlpha8 = (unsigned int)WindowOpacity;
  if (baseAlpha8 > 255)
    baseAlpha8 = 255;
  Pixmap pm =
      x11_create_background_pixmap(dpy, win, w, h, &attrs, base, baseAlpha8);
  if (!pm)
    return None;
  bg_cache_store(dpy, pm, &k);
  return pm;
}

void x11_draw_background_to_drawable(Display *dpy, Drawable drw, int w, int h) {
  if (!dpy || w <= 0 || h <= 0)
    return;
  unsigned long base = GetDrawBgColor();
  if (!base)
    base = RgbToHex(COLOR_BG);
  int radius = CornerRadius;
  bool rounded = RoundedCorners;
  BgKey k;
  k.base = base;
  k.radius = radius;
  k.rounded = rounded;
  k.w = w;
  k.h = h;
  Pixmap cached = bg_cache_find(dpy, &k);
  if (cached != None) {
    GC gc = XCreateGC(dpy, drw, 0, NULL);
    XCopyArea(dpy, cached, drw, gc, 0, 0, w, h, 0, 0);
    XFreeGC(dpy, gc);
    return;
  }
  int screen = DefaultScreen(dpy);
  Visual *vis = DefaultVisual(dpy, screen);
  int depth = DefaultDepth(dpy, screen);
  Colormap cmap = DefaultColormap(dpy, screen);
  if (!x11_get_drawable_visual(dpy, drw, &vis, &cmap, &depth)) {
    vis = DefaultVisual(dpy, screen);
    depth = DefaultDepth(dpy, screen);
  }
  bool hasAlpha = (depth == 32);
  bool rded = rounded && radius > 0 && hasAlpha;
  int R = rded ? radius : 0;
  if (R > w / 2)
    R = w / 2;
  if (R > h / 2)
    R = h / 2;
  unsigned int baseAlpha8 = (unsigned int)WindowOpacity;
  if (baseAlpha8 > 255)
    baseAlpha8 = 255;
  unsigned int cr0 = (base >> 16) & 0xFF, cg0 = (base >> 8) & 0xFF,
               cb0 = base & 0xFF;
  if (!hasAlpha) {
    unsigned long solid = (cr0 << 16) | (cg0 << 8) | cb0;
    GC sgc = XCreateGC(dpy, drw, 0, NULL);
    XSetForeground(dpy, sgc, solid);
    XFillRectangle(dpy, drw, sgc, 0, 0, w, h);
    XFreeGC(dpy, sgc);
    return;
  }
  size_t bpl = (((size_t)w * (size_t)depth + 7u) / 8u + 3u) & ~(size_t)3u;
  char *pixbuf = (char *)calloc(1, bpl * (size_t)h);
  if (!pixbuf) {
    return;
  }
  XImage *img =
      XCreateImage(dpy, vis, depth, ZPixmap, 0, pixbuf, w, h, 32, (int)bpl);
  if (!img) {
    free(pixbuf);
    return;
  }
  GC gc = XCreateGC(dpy, drw, 0, NULL);
  x11_generate_background_pixels((uint32_t *)img->data, img->bytes_per_line, w,
                                 h, R, baseAlpha8, base);
  XPutImage(dpy, drw, gc, img, 0, 0, 0, 0, w, h);
  XFreeGC(dpy, gc);
  XDestroyImage(img);
}

void x11_draw_background(Display *dpy, Window win, int w, int h) {
  Pixmap bg = x11_get_cached_background(dpy, win, w, h);
  if (bg) {
    GC gc = XCreateGC(dpy, win, 0, NULL);
    XCopyArea(dpy, bg, win, gc, 0, 0, w, h, 0, 0);
    XFreeGC(dpy, gc);
  } else {
    x11_draw_background_to_drawable(dpy, win, w, h);
  }
}

void x11_draw_selection_background(Display *dpy, XftDraw *xd, Visual *vis,
                                   Colormap cmap, int x, int y, int w, int h,
                                   unsigned long baseHex) {

  if (w <= 0 || h <= 0)
    return;
  XftColor c;
  x11_xft_alloc_color(dpy, vis, cmap, baseHex, OpacityAlpha16(), &c);
  XftDrawRect(xd, &c, x, y, w, h);
  XftColorFree(dpy, vis, cmap, &c);
}

void x11_draw_window_border(Display *dpy, Drawable drw, int w, int h,
                            bool no_bg) {
  if (!dpy || !drw || no_bg || !EnableBorders)
    return;
  int t = PROMPT_BORDER_W;
  if (w <= 2 * t || h <= 2 * t)
    return;
  int r = (RoundedCorners && CornerRadius > 0) ? CornerRadius : 0;
  if (r > w / 2)
    r = w / 2;
  if (r > h / 2)
    r = h / 2;
  unsigned long hex = RgbToHex(COLOR_BORDER);
  unsigned long a = ((unsigned long)((int)WindowOpacity)) & 0xFFUL;
  unsigned long px =
      (a << 24) | (hex & 0xFF0000UL) | (hex & 0xFF00UL) | (hex & 0xFFUL);
  if (r <= t)
    r = 0;
  GC gc = XCreateGC(dpy, drw, 0, NULL);
  XSetForeground(dpy, gc, px);
  XFillRectangle(dpy, drw, gc, r, 0, w - 2 * r, t);
  XFillRectangle(dpy, drw, gc, r, h - t, w - 2 * r, t);
  XFillRectangle(dpy, drw, gc, 0, r, t, h - 2 * r);
  XFillRectangle(dpy, drw, gc, w - t, r, t, h - 2 * r);
  float ri = (float)r - PROMPT_BORDER_INNER_OFF;
  if (r > t) {
    for (int y = 0; y < h; y++) {
      if (y >= r && y < h - r)
        continue;
      int cy = (y < r) ? r : h - r - 1;
      for (int x = 0; x < w; x++) {
        if (x >= r && x < w - r)
          continue;
        int cx = (x < r) ? r : w - r - 1;
        float dx = (float)x - (float)cx;
        float dy = (float)y - (float)cy;
        float d = sqrtf(dx * dx + dy * dy);
        if (d <= ri)
          continue;
        if (x11_corner_alpha(x, y, w, h, r, 255) == 0)
          continue;
        XFillRectangle(dpy, drw, gc, x, y, 1, 1);
      }
    }
  }
  XFreeGC(dpy, gc);
}

Window x11_create_prompt_window(Display *dpy, int x, int y, int w, int h) {
  unsigned long bg = RgbToHex(COLOR_BG);
  Window win;
  if (niri_is_active()) {
    log_error("niri override_redirect prompt w=%d h=%d", w, h);
    win = x11_create_window_ex_or(
        dpy, x, y, w, h, bg, "colored", "colored",
        KeyPressMask | KeyReleaseMask | ExposureMask | FocusChangeMask, true);
  } else {
    win = x11_create_window_ex(dpy, x, y, w, h, bg, "colored", "colored",
                               KeyPressMask | KeyReleaseMask | ExposureMask |
                                   FocusChangeMask);
    Window focus_win = None;
    int revert_to = 0;
    XGetInputFocus(dpy, &focus_win, &revert_to);
    if (focus_win != None && focus_win != PointerRoot && focus_win != win)
      XSetTransientForHint(dpy, win, focus_win);
    x11_set_fixed_size_hints(dpy, win, w, h);
  }
  if (RoundedCorners && CornerRadius > 0)
    x11_apply_shape_rounded(dpy, win, w, h, CornerRadius);
  return win;
}

void x11_apply_click_through(Display *dpy, Window win) {
  if (!dpy || win == None)
    return;
  XShapeCombineRectangles(dpy, win, ShapeInput, 0, 0, NULL, 0, ShapeSet,
                          Unsorted);
}

Window x11_create_display_message_window(Display *dpy, int x, int y, int w,
                                         int h, bool clickable) {
  unsigned long bg = 0;
  if (!GetTransparentWindow()) {
    bg = GetDrawBgColor();
    if (!bg)
      bg = RgbToHex(COLOR_BG);
  }
  x11_set_draw_context(dpy, NULL, NULL, 0);
  long mask = ExposureMask | (clickable ? ButtonPressMask : 0L);
  Window win = x11_create_window_ex_or(dpy, x, y, w, h, bg, "flashtext",
                                       "flashtext", mask, true);
  if (win != None && !clickable)
    x11_apply_click_through(dpy, win);

  x11_set_draw_context(NULL, NULL, NULL, 0);
  return win;
}

XftDraw *x11_create_xft_draw(Display *dpy, Window win) {
  Visual *vis = NULL;
  Colormap cmap = 0;
  int depth = 0;
  if (x11_get_drawable_visual(dpy, win, &vis, &cmap, &depth)) {
    return XftDrawCreate(dpy, win, vis, cmap);
  }
  int screen = DefaultScreen(dpy);
  return XftDrawCreate(dpy, win, DefaultVisual(dpy, screen),
                       DefaultColormap(dpy, screen));
}

bool x11_desktop_parse(const char *desktop_path, char exec_line[1024],
                       char workdir[1024], bool *is_terminal) {

  if (exec_line)
    exec_line[0] = '\0';
  if (workdir)
    workdir[0] = '\0';
  if (is_terminal)
    *is_terminal = false;
  if (!desktop_path)
    return false;
  StringList lines = File_ReadLines(desktop_path);
  if (!lines.items)
    return false;
  for (size_t i = 0; i < lines.count; i++) {
    char *p = lines.items[i];
    while (*p == ' ' || *p == '\t')
      p++;
    if (String_StartsWithCI(p, "Terminal=")) {
      char *v = p + 9;
      while (*v == ' ' || *v == '\t')
        v++;
      v[strcspn(v, "\r\n")] = '\0';
      if (is_terminal && (String_ContainsCI(v, "true") || strcmp(v, "1") == 0))
        *is_terminal = true;
    } else if (String_StartsWithCI(p, "Exec=")) {
      char *v = p + 5;
      while (*v == ' ' || *v == '\t')
        v++;
      v[strcspn(v, "\r\n")] = '\0';
      if (exec_line)
        snprintf(exec_line, 1024, "%s", v);
    } else if (String_StartsWithCI(p, "Path=")) {
      char *v = p + 5;
      while (*v == ' ' || *v == '\t')
        v++;
      v[strcspn(v, "\r\n")] = '\0';
      if (workdir)
        snprintf(workdir, 1024, "%s", v);
    }
  }
  StringList_Free(&lines);
  return true;
}

void x11_desktop_clean_exec(char exec_line[1024]) {
  if (!exec_line || !exec_line[0]) {
    return;
  }
  char cleaned[1024] = {0};
  size_t o = 0;
  for (size_t i = 0; exec_line[i] && o < sizeof(cleaned) - 1; i++) {
    if (exec_line[i] == '%' && exec_line[i + 1]) {
      char c = exec_line[i + 1];
      if (c == 'f' || c == 'F' || c == 'u' || c == 'U' || c == 'i' ||
          c == 'c' || c == 'k' || c == 'v' || c == 'm') {
        i++;
        while (exec_line[i + 1] == ' ')
          i++;
        continue;
      }
    }
    cleaned[o++] = exec_line[i];
  }
  cleaned[o] = '\0';
  char *s = cleaned;
  while (*s == ' ' || *s == '\t')
    s++;
  char *e = s + strlen(s) - 1;
  while (e > s && (*e == ' ' || *e == '\t'))
    *e-- = '\0';
  SafeCopy(exec_line, 1024, s);
}

bool x11_exe_in_path(const char *name) {
  if (!name || !name[0])
    return false;
  const char *path = getenv("PATH");
  if (!path || !path[0])
    return false;
  const char *p = path;
  while (*p) {
    const char *sep = strchr(p, ':');
    size_t len = sep ? (size_t)(sep - p) : strlen(p);
    char cand[1024];
    if (len == 0)
      snprintf(cand, sizeof(cand), "./%s", name);
    else
      snprintf(cand, sizeof(cand), "%.*s/%s", (int)len, p, name);
    if (access(cand, X_OK) == 0)
      return true;
    if (!sep)
      break;
    p = sep + 1;
  }
  return false;
}

bool x11_resolve_desktop_exec(const char *exec_line, const char *desktop_dir,
                              const char *workdir, char *out, size_t out_size) {
  if (!exec_line || !out || out_size == 0)
    return false;
  const char *p = exec_line;
  while (*p == ' ' || *p == '\t')
    p++;

  char token[1024];
  size_t t = 0;
  if (*p == '"') {
    p++;
    while (*p) {
      if (*p == '\\' && (p[1] == '"' || p[1] == '\\'))
        p++;
      else if (*p == '"') {
        p++;
        break;
      }
      if (t + 1 < sizeof(token))
        token[t++] = *p;
      p++;
    }
  } else {
    while (*p && *p != ' ' && *p != '\t') {
      if (t + 1 < sizeof(token))
        token[t++] = *p;
      p++;
    }
  }
  token[t] = '\0';
  while (*p == ' ' || *p == '\t')
    p++;
  if (!token[0])
    return false;

  const char *base = (workdir && workdir[0]) ? workdir : desktop_dir;
  char prog[1024];
  if (strchr(token, '/')) {
    if (token[0] == '/')
      snprintf(prog, sizeof(prog), "%.1023s", token);
    else
      snprintf(prog, sizeof(prog), "%.511s/%.511s", base, token);
  } else if (x11_exe_in_path(token)) {
    snprintf(prog, sizeof(prog), "%.1023s", token);
  } else {
    snprintf(prog, sizeof(prog), "%.511s/%.511s", desktop_dir, token);
  }

  if (strchr(prog, '/')) {
    if (access(prog, X_OK) != 0) {
      log_info("desktop exec resolve: '%s' not executable", prog);
      return false;
    }
  } else if (!x11_exe_in_path(prog)) {
    return false;
  }

  char q[2200];
  size_t n = 0;
  q[n++] = '\'';
  for (const char *s = prog; *s && n + 5 < sizeof(q); s++) {
    if (*s == '\'') {
      memcpy(q + n, "'\\''", 4);
      n += 4;
    } else
      q[n++] = *s;
  }
  q[n++] = '\'';
  q[n] = '\0';

  snprintf(out, out_size, "%s%s%s", q, p[0] ? " " : "", p);
  return true;
}

int x11_run_status(char *const argv[], const X11SessionEnv *env,
                   bool apply_env) {
  pid_t pid = fork();
  if (pid < 0)
    return -1;
  if (pid == 0) {
    if (apply_env && env)
      x11_env_apply_to_current_process(env);
    execvp(argv[0], argv);
    _exit(127);
  }
  int st = 0;
  if (waitpid(pid, &st, 0) < 0)
    return -1;
  if (!WIFEXITED(st))
    return -1;
  return WEXITSTATUS(st);
}
