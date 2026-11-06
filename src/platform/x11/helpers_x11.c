#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#include <X11/extensions/shape.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <X11/X.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/render.h>
#include <X11/extensions/shapeconst.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "platform/x11/display_x11.h"
#include "deps/log.h"
#include "core/window_manager.h"
#include "platform/x11/bg_image_x11.h"
#include "core/prompt_geometry.h"
#include "platform/platform.h"
#include "platform/draw.h"
#include "helpers/helpers_ui.h"
#include "wm/niri_wm.h"
#include "config_values.h"
#include "core/types.h"
#include "core/types/window.h"
#include "freetype/freetype.h"

#define NANOSVG_IMPLEMENTATION
#include "deps/nanosvg.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "deps/nanosvgrast.h"

#define STB_IMAGE_IMPLEMENTATION
#include "deps/stb_image.h"

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

static __thread BgSlot g_bg_slots[BG_SLOT_COUNT];
static __thread int g_bg_slot_next = 0;
static __thread Visual *g_dblVisual = NULL;
static __thread Colormap g_dblColormap = None;
static __thread int g_dblDepth = 0;
static __thread Display *g_dblDpy = NULL;

void x11_set_dbl_visual(Display *dpy, Visual *vis, Colormap cmap, int depth) {
  g_dblVisual = vis;
  g_dblColormap = cmap;
  g_dblDepth = depth;
  g_dblDpy = dpy;
}
void x11_clear_dbl_visual(void) {
  g_dblVisual = NULL;
  g_dblColormap = None;
  g_dblDepth = 0;
  g_dblDpy = NULL;
}

bool x11_is_own_window_name(const char *name) {
  if (!name)
    return false;
  return strncmp(name, "flint-", 6) == 0;
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
  if (g_dblVisual && g_dblDpy == dpy) {
    if (visOut)
      *visOut = g_dblVisual;
    if (cmapOut)
      *cmapOut = g_dblColormap;
    if (depthOut)
      *depthOut = g_dblDepth;
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
static X11FbSlot g_fb_cache[X11_FB_CACHE];
static int g_fb_next = 0;
static FcConfig *g_fb_config = NULL;
static pthread_mutex_t g_fb_mtx = PTHREAD_MUTEX_INITIALIZER;

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
    FcConfigSubstitute(g_fb_config, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);
    FcResult res = FcResultNoMatch;
    FcPattern *match = FcFontMatch(g_fb_config, pat, &res);
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
  if (!g_fb_config)
    g_fb_config = FcInitLoadConfigAndFonts();
  if (!g_fb_config)
    return NULL;
  for (int i = 0; i < X11_FB_CACHE; i++) {
    if (g_fb_cache[i].f && g_fb_cache[i].dpy == dpy && g_fb_cache[i].cp == cp &&
        g_fb_cache[i].px == px)
      return g_fb_cache[i].f;
  }
  XftFont *fb = x11_open_fallback_at_locked(dpy, cp, px > 0 ? px : FontSize);
  if (!fb)
    return NULL;
  if (g_fb_cache[g_fb_next].f) {
    Display *odpy = g_fb_cache[g_fb_next].dpy;
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

static XftFont *x11_fallback_for(Display *dpy, FcChar32 cp, int px) {
  pthread_mutex_lock(&g_fb_mtx);
  XftFont *f = x11_fallback_for_locked(dpy, cp, px);
  pthread_mutex_unlock(&g_fb_mtx);
  return f;
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
  int total = 0;
  p = b;
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
      total +=
          xft_width(dpy, runFont, (const char *)runStart, (int)(p - runStart));
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
        total += xft_width(dpy, fb, (const char *)fs, covered);
      else
        p += cl;
    } else {
      p += cl;
    }
  }
  return total;
}

int Platform_GetGlyphInkHeight(const char *text, int fontsize) {
  if (!text || !text[0] || fontsize <= 0)
    return fontsize > 0 ? fontsize : 0;
  Display *dpy = get_display();
  if (!dpy)
    return fontsize;
  FontHandle fh = Platform_CreateFontOnDisplay(dpy, fontsize, UsedFont);
  XftFont *xf = FH_TO_XFT(fh);
  if (!xf)
    return fontsize;
  const unsigned char *b = (const unsigned char *)text;
  const unsigned char *end = b + strlen(text);
  int clen = 1;
  FcChar32 cp = x11_utf8_cp(b, end, &clen);
  XftFont *use =
      x11_font_has(xf, cp) ? xf : x11_fallback_for(dpy, cp, fontsize);
  int ink = fontsize;
  fprintf(stderr, "[INK_X11] text='%s' fontsize=%d\n", text, fontsize);
  if (use) {
    XGlyphInfo e;
    XftTextExtentsUtf8(dpy, use, (FcChar8 *)b, clen, &e);
    int h = e.height;
    if (h > 0)
      ink = h;
  }
  Platform_DestroyFont(fh);
  fprintf(stderr, "[INK_X11_RET] text='%s' fontsize=%d ink=%d ret=%d\n", text, fontsize, ink, ink + PROMPT_GLYPH_INK_PAD);
  return ink + PROMPT_GLYPH_INK_PAD;
}

int x11_draw_utf8_fallback(Display *dpy, XftDraw *xd, XftColor *color, int x,
                           int y, XftFont *xf, const char *s, int n, int px) {
  if (!xf || !xd || !s || n <= 0)
    return 0;
  px = x11_font_px(xf, px);
  const unsigned char *b = (const unsigned char *)s;
  const unsigned char *end = b + n;
  const unsigned char *p = b;
  int cx = x;
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
    if (p > runStart) {
      int rl = (int)(p - runStart);
      XftDrawStringUtf8(xd, color, runFont, cx, y, (FcChar8 *)runStart, rl);
      cx += xft_width(dpy, runFont, (const char *)runStart, rl);
    }
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
      if (covered > 0) {
        XftDrawStringUtf8(xd, color, fb, cx, y, (FcChar8 *)fs, covered);
        cx += xft_width(dpy, fb, (const char *)fs, covered);
      } else {
        p += cl;
      }
    } else {
      p += cl;
    }
  }
  return cx - x;
}

void draw_wrapped_text(Display *dpy, XftDraw *xd, XftColor *color,
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
      int lw = x11_measure_utf8_fallback(dpy, xf, lineStart,
                                         (int)(word_start - lineStart), 0);

      int drawX = 0;
      if (alignment == 1)
        drawX =
            boxWidth > 0 ? (maxWidth - lw) / 2 : (maxWidth - lw) / 2 + xOffset;
      else if (alignment == 2)
        drawX = boxWidth > 0 ? maxWidth - lw : maxWidth - lw - xOffset;
      else
        drawX = xOffset;

      x11_draw_utf8_fallback(dpy, xd, color, drawX, y, xf, lineStart,
                             (int)(word_start - lineStart), 0);
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
    int lw =
        x11_measure_utf8_fallback(dpy, xf, lineStart, (int)(p - lineStart), 0);

    int drawX = 0;
    if (alignment == 1)
      drawX =
          boxWidth > 0 ? (maxWidth - lw) / 2 : xOffset + (maxWidth - lw) / 2;
    else if (alignment == 2)
      drawX = boxWidth > 0 ? maxWidth - lw : maxWidth - lw - xOffset;
    else
      drawX = xOffset;

    x11_draw_utf8_fallback(dpy, xd, color, drawX, y, xf, lineStart,
                           (int)(p - lineStart), 0);
  }
}

void x11_xft_alloc_color(Display *dpy, Visual *vis, Colormap cmap,
                         unsigned long hex, unsigned short alpha,
                         XftColor *out) {
  XRenderColor rc = {((hex >> 16) & 0xFF) * 0x101, ((hex >> 8) & 0xFF) * 0x101,
                     (hex & 0xFF) * 0x101, alpha};
  XftColorAllocValue(dpy, vis, cmap, &rc, out);
}
int x11_font_row_height(int font_size) {
  return font_size + PROMPT_ROW_H_BONUS;
}

int x11_font_list_top(int ascent, int descent, int font_size) {
  return font_size + PROMPT_LIST_TOP_BONUS;
}

int x11_font_row_height_of(FontHandle font) {
  XftFont *xf = FH_TO_XFT(font);
  return x11_font_row_height(FontSize);
}

int x11_font_list_top_of(FontHandle font) {
  XftFont *xf = FH_TO_XFT(font);
  return x11_font_list_top(xf ? xf->ascent : 0, xf ? xf->descent : 0, FontSize);
}

const char *ListBoxItemDisplayText(ListBoxInfo *lb, int idx, char *trunc,
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
static pthread_mutex_t g_img_cache_mtx = PTHREAD_MUTEX_INITIALIZER;

void x11_img_cache_free_all(void) {
  pthread_mutex_lock(&g_img_cache_mtx);
  for (int i = 0; i < IMG_CACHE_SLOTS; i++) {
    if (g_img_cache[i].argb)
      free(g_img_cache[i].argb);
    memset(&g_img_cache[i], 0, sizeof(g_img_cache[i]));
  }
  pthread_mutex_unlock(&g_img_cache_mtx);
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
    ImgCacheSlot *s = &g_img_cache[i];
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

bool Platform_DrawImageScaled(Display *dpy, XftDraw *xd, Visual *vis,
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
  pthread_mutex_lock(&g_img_cache_mtx);
  argb = x11_img_cache_get(path, cw, ch);
  if (argb) {
    pthread_mutex_unlock(&g_img_cache_mtx);
    Drawable drw = XftDrawDrawable(xd);
    bool ok = x11_blit_argb(dpy, drw, vis, argb, cw, ch, x, y);
    return ok;
  }
  pthread_mutex_unlock(&g_img_cache_mtx);
  struct stat st;
  if (stat(path, &st) != 0)
    return false;
  argb = x11_decode_image_argb(path, &cw, &ch);
  if (!argb)
    return false;
  pthread_mutex_lock(&g_img_cache_mtx);
  x11_img_cache_put(path, cw, ch, (long long)st.st_mtime, (long long)st.st_size,
                    argb);
  pthread_mutex_unlock(&g_img_cache_mtx);
  Drawable drw = XftDrawDrawable(xd);
  bool ok = x11_blit_argb(dpy, drw, vis, argb, cw, ch, x, y);
  return ok;
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
  memset(&img, 0, sizeof(img));
  img.width = w;
  img.height = h;
  img.xoffset = 0;
  img.format = ZPixmap;
  img.data = (char *)argb;
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
  /* read whole file */
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
    if (ncolors < 1) {
      free(buf);
      return NULL;
    }
  }
  if (w <= 0 || h <= 0) {
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
    strncpy(keys[ci], keybuf, 15);
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
    char *endq = strchr(p, '"');
    if (endq)
      p = endq + 1;
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

static DecodedCacheSlot g_decoded_cache[32];
static int g_decoded_count = 0;

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
  bool has_stat = (stat(path, &st) == 0);

  for (int i = 0; i < g_decoded_count; i++) {
    DecodedCacheSlot *slot = &g_decoded_cache[i];
    if (slot->dst_w == dst_w && slot->dst_h == dst_h &&
        strcmp(slot->path, path) == 0) {
      bool fresh = !has_stat ||
                   (slot->has_stat &&
                    slot->st.st_mtime == st.st_mtime &&
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
  bool _is_svg = _plen > 4 && Platform_StringContainsCaseInsensitive(
                                  path + _plen - 4, ".svg");

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
    nsvgRasterize(rast, image, 0, 0, (float)dst_w / image->width, argb,
                  dst_w, dst_h, dst_w * 4);
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
    bool _is_xpm = _plen > 4 && Platform_StringContainsCaseInsensitive(
                                    path + _plen - 4, ".xpm");
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

  int idx = g_decoded_count;
  if (idx >= 32) {
    free(g_decoded_cache[0].argb);
    for (int i = 0; i < 31; i++)
      g_decoded_cache[i] = g_decoded_cache[i + 1];
    idx = 31;
  } else {
    g_decoded_count++;
  }
  snprintf(g_decoded_cache[idx].path, sizeof(g_decoded_cache[idx].path), "%s",
           path);
  g_decoded_cache[idx].argb = argb;
  g_decoded_cache[idx].w = dst_w;
  g_decoded_cache[idx].h = dst_h;
  g_decoded_cache[idx].dst_w = dst_w;
  g_decoded_cache[idx].dst_h = dst_h;
  g_decoded_cache[idx].st = st;
  g_decoded_cache[idx].has_stat = has_stat;

  unsigned char *copy = (unsigned char *)malloc((size_t)dst_w * dst_h * 4);
  if (!copy)
    return NULL;
  memcpy(copy, argb, (size_t)dst_w * dst_h * 4);
  *out_w = dst_w;
  *out_h = dst_h;
  return copy;
}

void DrawListBoxItemText(Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap,
                         FontHandle font, XftColor *num_color,
                         XftColor *body_color, XftColor *plain_color,
                         ListBoxInfo *lb, char *icon_path, int idx, int y,
                         int text_x) {
  XftFont *xf = FH_TO_XFT(font);
  if (!xf)
    return;
  int XOffset = PROMPT_ITEM_TEXT_X;
  int row_h = x11_font_row_height_of(font);
  int pad_top = (row_h - (xf->ascent + xf->descent)) / 2;
  if (pad_top < 0)
    pad_top = 0;
  int text_y = y + pad_top;
  if (icon_path) {
    int iconX = XOffset;
    int iconY = y + (x11_font_row_height_of(font) - IconSize) / 2;
    XOffset += IconSize + XOffset;
    Platform_DrawImageScaled(dpy, xd, vis, icon_path, iconX, iconY, IconSize,
                             IconSize);
  }
  char trunc[512];
  ListBoxItemDisplayText(lb, idx, trunc, sizeof(trunc));

  unsigned long pcol = plain_color ? plain_color->pixel : 0;
  if (!pcol)
    pcol = rgb_to_hex(COLOR_TEXT);
  Platform_SetDrawContext(dpy, xd, vis, cmap);
  Platform_DrawTextRaw((PlDrawTextRawSpec){.x = XOffset,
                                           .y = text_y,
                                           .text = trunc,
                                           .color = pcol,
                                           .font = font,
                                           .align = "LEFT",
                                           .pad = XOffset,
                                           .fontsize = FontSize});
}

Window x11_create_window_ex_or(Display *dpy, int x, int y, int w, int h,
                               unsigned long bg_color, const char *res_name,
                               const char *res_class, long event_mask,
                               bool override_redirect);

Window x11_create_window_ex(Display *dpy, int x, int y, int w, int h,
                            unsigned long bg_color, const char *res_name,
                            const char *res_class, long event_mask) {
  return x11_create_window_ex_or(dpy, x, y, w, h, bg_color, res_name, res_class,
                                 event_mask, false);
}

static void x11_apply_shape_rounded(Display *dpy, Window win, int w, int h,
                                    int radius);
static unsigned int CornerAlpha(int x, int y, int w, int h, int R,
                                unsigned int baseAlpha8);

Window x11_create_window_ex_or(Display *dpy, int x, int y, int w, int h,
                               unsigned long bg_color, const char *res_name,
                               const char *res_class, long event_mask,
                               bool override_redirect) {
  int screen = DefaultScreen(dpy);
  XVisualInfo vinfo;
  Visual *visual = NULL;
  int depth = 0;
  Colormap cmap = 0;
  bool use_argb = false;
  static __thread Colormap cached_cmap = None;
  static __thread Visual *cached_visual = NULL;
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
  Pixmap bg_pm = (Platform_GetTransparentWindow() || !x11_bg_image_enabled())
                     ? None
                     : x11_bg_image_tile(dpy, w, h);
  if (use_argb) {
    XSetWindowAttributes attrs = {0};
    unsigned int r = (bg_color >> 16) & 0xFF;
    unsigned int g = (bg_color >> 8) & 0xFF;
    unsigned int b = bg_color & 0xFF;
    unsigned int a =
        Platform_GetTransparentWindow() ? 0 : (unsigned int)WindowOpacity;
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
    Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom state_skip_taskbar =
        XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom state_skip_pager = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
    Atom states[2] = {state_skip_taskbar, state_skip_pager};
    XChangeProperty(dpy, win, wm_state, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)states, 2);
  } else if (res_name && strcmp(res_name, "flashtext") == 0) {
    Atom wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom wm_type_note =
        XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
    XChangeProperty(dpy, win, wm_type, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&wm_type_note, 1);
    Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom state_skip_taskbar =
        XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom state_skip_pager = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
    Atom states[2] = {state_skip_taskbar, state_skip_pager};
    XChangeProperty(dpy, win, wm_state, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)states, 2);
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
    XSizeHints *sh = XAllocSizeHints();
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
      if (CornerAlpha(x, y, w, h, radius, 255) == 0)
        continue;
      XFillRectangle(dpy, mask, gc, x, y, 1, 1);
    }
  }
  XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
  XSync(dpy, False);
  XFreePixmap(dpy, mask);
  XFreeGC(dpy, gc);
}

static unsigned long x11_background_color_at(int x, int y, int w, int h);

void x11_fill_rounded_rect(Display *dpy, XftDraw *xd, Visual *vis,
                           Colormap cmap, unsigned long hex,
                           unsigned short alpha16, int x, int y, int w, int h,
                           int radius, int win_w, int win_h) {
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
        unsigned long backdrop =
            x11_background_color_at(ox0 + dx, oy0 + dy, win_w, win_h);
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

static unsigned int x11_bg_channel_at(float c, int x, int y) {
  int v = (int)(c);
  if (v < 0)
    v = 0;
  if (v > 255)
    v = 255;
  return (unsigned int)v;
}

static unsigned long x11_background_color_at(int x, int y, int w, int h) {
  unsigned long base = Platform_GetDrawBgColor();
  if (!base)
    base = rgb_to_hex(COLOR_BG);
  unsigned int r = (base >> 16) & 0xFF, g = (base >> 8) & 0xFF, b = base & 0xFF;
  unsigned long col = base;
  unsigned int cr = x11_bg_channel_at((float)((col >> 16) & 0xFF), x, y);
  unsigned int cg = x11_bg_channel_at((float)((col >> 8) & 0xFF), x, y);
  unsigned int cb = x11_bg_channel_at((float)(col & 0xFF), x, y);
  return ((col >> 16) & 0xFF) << 16 | ((col >> 8) & 0xFF) << 8 | (col & 0xFF);
}

static unsigned int CornerAlpha(int x, int y, int w, int h, int R,
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
                                           unsigned long base,
                                           unsigned long fill) {
  uint32_t *row;
  unsigned int r = (base >> 16) & 0xFF, g = (base >> 8) & 0xFF, b = base & 0xFF;
  for (int y = 0; y < h; y++) {
    row = (uint32_t *)((char *)img_data + y * bytes_per_line);
    for (int x = 0; x < w; x++) {
      unsigned int a = CornerAlpha(x, y, w, h, R, baseAlpha8);
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
    if (g_bg_slots[i].dpy == dpy && bg_slot_matches(&g_bg_slots[i], k))
      return g_bg_slots[i].pm;
  }
  return None;
}

static void bg_cache_store(Display *dpy, Pixmap pm, const BgKey *k) {
  BgSlot *slot = &g_bg_slots[g_bg_slot_next];
  g_bg_slot_next = (g_bg_slot_next + 1) % BG_SLOT_COUNT;
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
                                           int h, XWindowAttributes *attrs,
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

  unsigned int r = (base >> 16) & 0xFF, g = (base >> 8) & 0xFF, b = base & 0xFF;
  unsigned long fill = (baseAlpha8 << 24) | (r << 16) | (g << 8) | b;
  x11_generate_background_pixels((uint32_t *)img->data, img->bytes_per_line, w,
                                 h, R, baseAlpha8, base, fill);

  GC gc = XCreateGC(dpy, pm, 0, NULL);
  XPutImage(dpy, pm, gc, img, 0, 0, 0, 0, w, h);
  XFreeGC(dpy, gc);
  XDestroyImage(img);
  if (x11_bg_image_enabled()) {
    x11_bg_image_blit(dpy, pm, vis, depth, w, h);
  }
  if (rounded && R > 0) {
    XImage *img2 = XGetImage(dpy, pm, 0, 0, w, h, AllPlanes, ZPixmap);
    if (img2) {
      for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
          unsigned int a = CornerAlpha(x, y, w, h, R, baseAlpha8);
          if (a < baseAlpha8) {
            uint32_t pixel = XGetPixel(img2, x, y);
            unsigned int alpha = (pixel >> 24) & 0xFF;
            unsigned int new_alpha = (alpha * a + 127) / 255;
            unsigned int r_c = (pixel >> 16) & 0xFF;
            unsigned int g_c = (pixel >> 8) & 0xFF;
            unsigned int b_c = pixel & 0xFF;
            unsigned int pm_r = (r_c * new_alpha + 127) / 255;
            unsigned int pm_g = (g_c * new_alpha + 127) / 255;
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
  unsigned long base = Platform_GetDrawBgColor();
  if (!base)
    base = rgb_to_hex(COLOR_BG);
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
  unsigned long base = Platform_GetDrawBgColor();
  if (!base)
    base = rgb_to_hex(COLOR_BG);
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
  unsigned int r = cr0, g = cg0, b = cb0;
  unsigned long fill = (baseAlpha8 << 24) | (r << 16) | (g << 8) | b;
  GC gc = XCreateGC(dpy, drw, 0, NULL);
  x11_generate_background_pixels((uint32_t *)img->data, img->bytes_per_line, w,
                                 h, R, baseAlpha8, base, fill);
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
  unsigned long hex = rgb_to_hex(COLOR_BORDER);
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
        if (CornerAlpha(x, y, w, h, r, 255) == 0)
          continue;
        XFillRectangle(dpy, drw, gc, x, y, 1, 1);
      }
    }
  }
  XFreeGC(dpy, gc);
}

Window x11_create_prompt_window(Display *dpy, int x, int y, int w, int h) {
  unsigned long bg = rgb_to_hex(COLOR_BG);
  Window win;
  if (niri_is_active()) {
    log_error("niri override_redirect prompt w=%d h=%d", w, h);
    win = x11_create_window_ex_or(dpy, x, y, w, h, bg, "colored", "colored",
                                  KeyPressMask | ExposureMask | FocusChangeMask,
                                  true);
  } else {
    win = x11_create_window_ex(dpy, x, y, w, h, bg, "colored", "colored",
                               KeyPressMask | ExposureMask | FocusChangeMask);
    Window focus_win = None;
    int revert_to = 0;
    XGetInputFocus(dpy, &focus_win, &revert_to);
    if (focus_win != None && focus_win != PointerRoot && focus_win != win)
      XSetTransientForHint(dpy, win, focus_win);
    char unique_title[64];
    snprintf(unique_title, sizeof(unique_title), "flint-%lu-%d",
             (unsigned long)win, (int)getpid());
    XStoreName(dpy, win, unique_title);
    XSizeHints *sh = XAllocSizeHints();
    sh->flags = PMinSize | PMaxSize;
    sh->min_width = w;
    sh->min_height = h;
    sh->max_width = w;
    sh->max_height = h;
    XSetWMNormalHints(dpy, win, sh);
    XFree(sh);
  }
  if (RoundedCorners && CornerRadius > 0)
    x11_apply_shape_rounded(dpy, win, w, h, CornerRadius);
  return win;
}

void Platform_PrewarmBackgroundPixmap(void) {
  Display *dpy = get_display();
  if (!dpy) {
    return;
  }
  x11_bg_image_prewarm(dpy, PROMPT_W, PROMPT_H);
  Window _pw = x11_create_prompt_window(dpy, 0, 0, PROMPT_W, PROMPT_H);
  if (_pw != None) {
    x11_get_cached_background(dpy, _pw, PROMPT_W, PROMPT_H);
    XDestroyWindow(dpy, _pw);
  }
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
  if (!Platform_GetTransparentWindow()) {
    bg = Platform_GetDrawBgColor();
    if (!bg)
      bg = rgb_to_hex(COLOR_BG);
  }
  Platform_SetDrawContext(dpy, NULL, NULL, 0);
  long mask = ExposureMask | (clickable ? ButtonPressMask : 0L);
  Window win = x11_create_window_ex_or(dpy, x, y, w, h, bg, "flashtext",
                                       "flashtext", mask, true);
  if (win != None && !clickable)
    x11_apply_click_through(dpy, win);

  Platform_SetDrawContext(NULL, NULL, NULL, 0);
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

void UpdateTextCaret(void) {
  WindowManager *wm = WindowManager_Get();
  Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
  if (top && top->vtable && top->vtable->on_render)
    top->vtable->on_render(top);
}
