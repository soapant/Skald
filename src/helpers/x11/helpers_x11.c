#include "core/draw_state.h"
#include "helpers/helpers_text.h"
#include "helpers/helpers_fs.h"
#include "helpers/helpers_misc.h"
#include <stdint.h>
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
static __thread Visual *dbl_visual = NULL;
static __thread Colormap dbl_colormap = None;
static __thread int dbl_depth = 0;
static __thread Display *dbl_dpy = NULL;

void x11_set_dbl_visual(Display *dpy, Visual *vis, Colormap cmap, int depth) {
  dbl_visual = vis;
  dbl_colormap = cmap;
  dbl_depth = depth;
  dbl_dpy = dpy;
}

void x11_clear_dbl_visual(void) {
  dbl_visual = NULL;
  dbl_colormap = None;
  dbl_depth = 0;
  dbl_dpy = NULL;
}

bool x11_get_drawable_visual(X11GetDrawableVisual *argspec) {
  XWindowAttributes attrs;
  if (XGetWindowAttributes(argspec->dpy, argspec->d, &attrs)) {
    if (argspec->vis_out)
      *argspec->vis_out = attrs.visual;
    if (argspec->cmap_out)
      *argspec->cmap_out = attrs.colormap;
    if (argspec->depth_out)
      *argspec->depth_out = attrs.depth;
    return true;
  }
  if (dbl_visual && dbl_dpy == argspec->dpy) {
    if (argspec->vis_out)
      *argspec->vis_out = dbl_visual;
    if (argspec->cmap_out)
      *argspec->cmap_out = dbl_colormap;
    if (argspec->depth_out)
      *argspec->depth_out = dbl_depth;
    return true;
  }
  return false;
}

static int xft_width(Display *dpy, XftFont *xf, const char *text, int len) {
  XGlyphInfo ext;
  XftTextExtentsUtf8(dpy, xf, (FcChar8 *)text, len, &ext);
  return ext.xOff;
}

#define X11_FB_CACHE 512
#define X11_FB_FAMILY_MAX 64
#define X11_FB_FONT_CACHE 24

typedef struct {
  FcChar32 cp;
  bool valid;
  char family[X11_FB_FAMILY_MAX];
} X11FbSlot;

typedef struct {
  Display *dpy;
  int px;
  char family[X11_FB_FAMILY_MAX];
  XftFont *f;
  unsigned long stamp;
} X11FbFontSlot;

static X11FbSlot fb_cache[X11_FB_CACHE];
static X11FbFontSlot fb_fonts[X11_FB_FONT_CACHE];
static unsigned long fb_font_stamp;
static FcConfig *fb_config = NULL;
static pthread_mutex_t fb_mtx = PTHREAD_MUTEX_INITIALIZER;

static size_t x11_fb_slot_index(FcChar32 cp) {
  return ((uint32_t)cp * 2654435761u) % X11_FB_CACHE;
}

static const char *x11_fb_family_find(FcChar32 cp) {
  size_t start = x11_fb_slot_index(cp);
  for (int i = 0; i < X11_FB_CACHE; i++) {
    X11FbSlot *s = &fb_cache[(start + (size_t)i) % X11_FB_CACHE];
    if (s->valid && s->cp == cp)
      return s->family;
  }
  return NULL;
}

static void x11_fb_family_store(FcChar32 cp, const char *family) {
  if (!family || !family[0])
    return;
  size_t start = x11_fb_slot_index(cp);
  for (int i = 0; i < X11_FB_CACHE; i++) {
    X11FbSlot *s = &fb_cache[(start + (size_t)i) % X11_FB_CACHE];
    if (!s->valid || s->cp == cp) {
      s->valid = true;
      s->cp = cp;
      snprintf(s->family, sizeof(s->family), "%s", family);
      return;
    }
  }
  X11FbSlot *s = &fb_cache[start];
  s->valid = true;
  s->cp = cp;
  snprintf(s->family, sizeof(s->family), "%s", family);
}

static XftFont *x11_fb_font_get_locked(Display *dpy, const char *family,
                                       int px) {
  for (int i = 0; i < X11_FB_FONT_CACHE; i++) {
    X11FbFontSlot *s = &fb_fonts[i];
    if (s->f && s->dpy == dpy && s->px == px &&
        strcmp(s->family, family) == 0) {
      s->stamp = ++fb_font_stamp;
      return s->f;
    }
  }
  int screen = DefaultScreen(dpy);
  XLockDisplay(dpy);
  XftFont *f = XftFontOpen(dpy, screen, FC_FAMILY, FcTypeString, family,
                           FC_PIXEL_SIZE, FcTypeDouble, (double)px, NULL);
  if (!f)
    f = XftFontOpen(dpy, screen, FC_FAMILY, FcTypeString, "sans-serif",
                    FC_PIXEL_SIZE, FcTypeDouble, (double)px, NULL);
  XUnlockDisplay(dpy);
  if (!f)
    return NULL;
  X11FbFontSlot *victim = &fb_fonts[0];
  for (int i = 0; i < X11_FB_FONT_CACHE; i++) {
    X11FbFontSlot *s = &fb_fonts[i];
    if (!s->f) {
      victim = s;
      break;
    }
    if (s->stamp < victim->stamp)
      victim = s;
  }
  if (victim->f) {
    XLockDisplay(victim->dpy);
    XftFontClose(victim->dpy, victim->f);
    XUnlockDisplay(victim->dpy);
  }
  victim->dpy = dpy;
  victim->px = px;
  snprintf(victim->family, sizeof(victim->family), "%s", family);
  victim->f = f;
  victim->stamp = ++fb_font_stamp;
  return f;
}

static void x11_fb_font_put_locked(Display *dpy, const char *family, int px,
                                   XftFont *f) {
  if (!f || !family)
    return;
  for (int i = 0; i < X11_FB_FONT_CACHE; i++) {
    X11FbFontSlot *s = &fb_fonts[i];
    if (s->f && s->dpy == dpy && s->px == px &&
        strcmp(s->family, family) == 0) {
      if (s->f != f) {
        XLockDisplay(s->dpy);
        XftFontClose(s->dpy, s->f);
        XUnlockDisplay(s->dpy);
      }
      s->f = f;
      s->stamp = ++fb_font_stamp;
      return;
    }
  }
  X11FbFontSlot *victim = &fb_fonts[0];
  for (int i = 0; i < X11_FB_FONT_CACHE; i++) {
    X11FbFontSlot *s = &fb_fonts[i];
    if (!s->f) {
      victim = s;
      break;
    }
    if (s->stamp < victim->stamp)
      victim = s;
  }
  if (victim->f) {
    XLockDisplay(victim->dpy);
    XftFontClose(victim->dpy, victim->f);
    XUnlockDisplay(victim->dpy);
  }
  victim->dpy = dpy;
  victim->px = px;
  snprintf(victim->family, sizeof(victim->family), "%s", family);
  victim->f = f;
  victim->stamp = ++fb_font_stamp;
}

void x11_fallback_font_cache_free_all(void) {
  pthread_mutex_lock(&fb_mtx);
  for (int i = 0; i < X11_FB_FONT_CACHE; i++) {
    X11FbFontSlot *s = &fb_fonts[i];
    if (s->f) {
      XLockDisplay(s->dpy);
      XftFontClose(s->dpy, s->f);
      XUnlockDisplay(s->dpy);
    }
  }
  memset(fb_fonts, 0, sizeof(fb_fonts));
  memset(fb_cache, 0, sizeof(fb_cache));
  fb_font_stamp = 0;
  if (fb_config) {
    FcConfigDestroy(fb_config);
    fb_config = NULL;
  }
  pthread_mutex_unlock(&fb_mtx);
}

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

#define X11_MEASURE_SLOTS 256
#define X11_MEASURE_TEXT_MAX 160
#define X11_MEASURE_PROBE 4

typedef struct {
  XftFont *xf;
  int len;
  int width;
  unsigned long epoch;
  unsigned long stamp;
  char text[X11_MEASURE_TEXT_MAX];
} MeasureSlot;

static MeasureSlot measure_slots[X11_MEASURE_SLOTS];
static unsigned long measure_stamp = 0;
static pthread_mutex_t measure_mtx = PTHREAD_MUTEX_INITIALIZER;
static unsigned long g_measure_epoch = 1;

void x11_bump_measure_epoch(void) {
  pthread_mutex_lock(&measure_mtx);
  g_measure_epoch++;
  pthread_mutex_unlock(&measure_mtx);
}

static unsigned long x11_str_hash(const char *s, int n) {
  unsigned long h = 14695981039346656037ULL;
  for (int i = 0; i < n; i++) {
    h ^= (unsigned char)s[i];
    h *= 1099511628211ULL;
  }
  return h;
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
  const char *cached_family = x11_fb_family_find(cp);
  bool had_family = cached_family != NULL;
  if (cached_family) {
    XftFont *fb = x11_fb_font_get_locked(dpy, cached_family, px);
    if (fb && x11_font_has(fb, cp))
      return fb;
  }
  XftFont *fb = NULL;
  char matched_family[X11_FB_FAMILY_MAX] = {0};
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
      FcChar8 *fc_family = NULL;
      if (FcPatternGetString(match, FC_FAMILY, 0, &fc_family) ==
              FcResultMatch &&
          fc_family)
        snprintf(matched_family, sizeof(matched_family), "%s",
                 (const char *)fc_family);
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
  if (fb && matched_family[0]) {
    bool family_changed =
        !had_family || strcmp(matched_family, cached_family) != 0;
    if (family_changed) {
      x11_fb_font_put_locked(dpy, matched_family, px, fb);
      x11_fb_family_store(cp, matched_family);
    }
  }
  return fb;
}

static XftFont *x11_fallback_for_locked(Display *dpy, FcChar32 cp, int px) {
  if (!fb_config)
    fb_config = FcInitLoadConfigAndFonts();
  if (!fb_config)
    return NULL;
  if (px <= 0)
    px = FontSize;
  const char *family = x11_fb_family_find(cp);
  if (family) {
    XftFont *fb = x11_fb_font_get_locked(dpy, family, px);
    if (fb && x11_font_has(fb, cp))
      return fb;
  }
  return x11_open_fallback_at_locked(dpy, cp, px);
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
    const unsigned char *run_start = p;
    XftFont *run_font = xf;
    while (p < end) {
      int cl = 1;
      FcChar32 cp = x11_utf8_cp(p, end, &cl);
      if (!x11_font_has(run_font, cp))
        break;
      p += cl;
    }
    if (p > run_start)
      total += run_cb(dpy, run_font, run_start, (int)(p - run_start), ud);
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

static bool measure_slot_find(XftFont *xf, const char *s, int n,
                              unsigned long epoch, unsigned long *out_width) {
  unsigned long h = x11_str_hash(s, n) % X11_MEASURE_SLOTS;
  for (int k = 0; k < X11_MEASURE_PROBE; k++) {
    MeasureSlot *ms = &measure_slots[(h + k) % X11_MEASURE_SLOTS];
    if (ms->xf == xf && ms->len == n && ms->epoch == epoch &&
        !memcmp(ms->text, s, (size_t)n)) {
      measure_stamp++;
      ms->stamp = measure_stamp;
      *out_width = (unsigned long)ms->width;
      return true;
    }
  }
  return false;
}

static void measure_slot_store(XftFont *xf, const char *s, int n, int width,
                               unsigned long epoch) {
  unsigned long h = x11_str_hash(s, n) % X11_MEASURE_SLOTS;
  int best = -1;
  unsigned long oldest = ~0UL;
  for (int k = 0; k < X11_MEASURE_PROBE; k++) {
    int i = (int)((h + k) % X11_MEASURE_SLOTS);
    MeasureSlot *ms = &measure_slots[i];
    if (!ms->xf || ms->epoch != epoch) {
      best = i;
      break;
    }
    if (ms->stamp < oldest) {
      oldest = ms->stamp;
      best = i;
    }
  }
  MeasureSlot *ms = &measure_slots[best];
  memset(ms, 0, sizeof(*ms));
  ms->xf = xf;
  ms->len = n;
  ms->width = width;
  ms->epoch = epoch;
  measure_stamp++;
  ms->stamp = measure_stamp;
  memcpy(ms->text, s, (size_t)n);
}

int x11_measure_utf8_fallback(X11MeasureUtf8Fallback *argspec) {
  if (!argspec->xf || !argspec->s || argspec->n <= 0)
    return 0;
  argspec->px = x11_font_px(argspec->xf, argspec->px);
  if (argspec->n < X11_MEASURE_TEXT_MAX) {
    pthread_mutex_lock(&measure_mtx);
    unsigned long epoch = g_measure_epoch;
    unsigned long hit = 0;
    if (measure_slot_find(argspec->xf, argspec->s, argspec->n, epoch, &hit)) {
      pthread_mutex_unlock(&measure_mtx);
      return (int)hit;
    }
    pthread_mutex_unlock(&measure_mtx);
    int width = -1;
    {
      const unsigned char *b0 = (const unsigned char *)argspec->s;
      const unsigned char *e0 = b0 + argspec->n;
      bool all = true;
      const unsigned char *p0 = b0;
      while (p0 < e0) {
        int cl = 1;
        FcChar32 cp0 = x11_utf8_cp(p0, e0, &cl);
        if (!x11_font_has(argspec->xf, cp0)) {
          all = false;
          break;
        }
        p0 += cl;
      }
      if (all)
        width = xft_width(argspec->dpy, argspec->xf, argspec->s, argspec->n);
      else
        width = x11_utf8_fallback_runs(
            argspec->dpy, argspec->xf, (const unsigned char *)argspec->s,
            argspec->n, argspec->px, x11_measure_run, NULL);
    }
    pthread_mutex_lock(&measure_mtx);
    if (epoch == g_measure_epoch)
      measure_slot_store(argspec->xf, argspec->s, argspec->n, width, epoch);
    pthread_mutex_unlock(&measure_mtx);
    return width;
  }
  const unsigned char *b = (const unsigned char *)argspec->s;
  const unsigned char *end = b + argspec->n;
  bool all = true;
  const unsigned char *p = b;
  while (p < end) {
    int cl = 1;
    FcChar32 cp = x11_utf8_cp(p, end, &cl);
    if (!x11_font_has(argspec->xf, cp)) {
      all = false;
      break;
    }
    p += cl;
  }
  if (all)
    return xft_width(argspec->dpy, argspec->xf, argspec->s, argspec->n);
  return x11_utf8_fallback_runs(argspec->dpy, argspec->xf, b, argspec->n,
                                argspec->px, x11_measure_run, NULL);
}

int x11_draw_utf8_fallback(X11DrawUtf8Fallback *argspec) {
  if (!argspec->xf || !argspec->xd || !argspec->s || argspec->n <= 0)
    return 0;
  argspec->px = x11_font_px(argspec->xf, argspec->px);
  X11DrawRunCtx ctx = {argspec->xd, argspec->color, argspec->y, argspec->x};
  x11_utf8_fallback_runs(argspec->dpy, argspec->xf,
                         (const unsigned char *)argspec->s, argspec->n,
                         argspec->px, x11_draw_run, &ctx);
  return ctx.cx - argspec->x;
}

static void x11_wrap_flush_line(Display *dpy, XftDraw *xd, XftColor *color,
                                XftFont *xf, const char *line_start,
                                int line_len, int alignment, int box_width,
                                int max_width, int x_offset, int y) {
  int lw = x11_measure_utf8_fallback(&(X11MeasureUtf8Fallback){
      .dpy = dpy, .xf = xf, .s = line_start, .n = line_len, .px = 0});
  int draw_x = 0;
  if (alignment == 1)
    draw_x =
        box_width > 0 ? (max_width - lw) / 2 : x_offset + (max_width - lw) / 2;
  else if (alignment == 2)
    draw_x = box_width > 0 ? max_width - lw : max_width - lw - x_offset;
  else
    draw_x = x_offset;
  x11_draw_utf8_fallback(&(X11DrawUtf8Fallback){.dpy = dpy,
                                                .xd = xd,
                                                .color = color,
                                                .x = draw_x,
                                                .y = y,
                                                .xf = xf,
                                                .s = line_start,
                                                .n = line_len,
                                                .px = 0});
}

void x11_draw_wrapped_text(X11DrawWrappedText *argspec) {
  if (!argspec->text) {
    return;
  }
  XftFont *xf = FH_TO_XFT(argspec->font);
  if (!xf) {
    return;
  }
  int line_height = xf->ascent + xf->descent;
  int space_width = xft_width(argspec->dpy, xf, " ", 1);
  int y = argspec->start_y + xf->ascent;
  int text_len = strlen(argspec->text);
  int max_width = 0;
  int alignment = 0;

  if (argspec->centered_to != NULL) {
    if (string_contains_ci(argspec->centered_to, "CENTER")) {
      alignment = 1;
      max_width =
          argspec->box_width > 0 ? argspec->box_width : PROMPT_WRAP_BOX_CENTER;
    } else if (string_contains_ci(argspec->centered_to, "RIGHT")) {
      alignment = 2;
      max_width =
          argspec->box_width > 0 ? argspec->box_width : PROMPT_WRAP_BOX_RIGHT;
    } else {
      alignment = 0;
      max_width = 1 << 20;
    }
  } else {
    alignment = 0;
    max_width = 1 << 20;
  }

  const char *p = argspec->text;
  const char *line_start = argspec->text;
  int line_width = 0;

  while (*p) {
    const char *word_start = p;
    while (*p && *p != ' ')
      p++;
    int word_width = x11_measure_utf8_fallback(
        &(X11MeasureUtf8Fallback){.dpy = argspec->dpy,
                                  .xf = xf,
                                  .s = word_start,
                                  .n = (int)(p - word_start),
                                  .px = 0});
    int add = word_width + (line_width > 0 ? space_width : 0);

    if (line_width > 0 && line_width + add > max_width) {
      x11_wrap_flush_line(argspec->dpy, argspec->xd, argspec->color, xf,
                          line_start, (int)(word_start - line_start), alignment,
                          argspec->box_width, max_width, argspec->x_offset, y);
      y += line_height;
      line_start = word_start;
      line_width = word_width;
    } else {
      line_width += add;
    }
    while (*p == ' ')
      p++;
  }

  if (p > line_start) {
    x11_wrap_flush_line(argspec->dpy, argspec->xd, argspec->color, xf,
                        line_start, (int)(p - line_start), alignment,
                        argspec->box_width, max_width, argspec->x_offset, y);
  }
}

void x11_init_argb_image(X11InitArgbImage *argspec) {
  memset(argspec->img, 0, sizeof(*argspec->img));
  argspec->img->width = argspec->w;
  argspec->img->height = argspec->h;
  argspec->img->xoffset = 0;
  argspec->img->format = ZPixmap;
  argspec->img->data = (char *)argspec->data;
  argspec->img->byte_order = ImageByteOrder(argspec->dpy);
  argspec->img->bitmap_unit = BitmapUnit(argspec->dpy);
  argspec->img->bitmap_bit_order = BitmapBitOrder(argspec->dpy);
  argspec->img->bitmap_pad = 32;
  argspec->img->depth = 32;
  argspec->img->bytes_per_line = argspec->w * 4;
  argspec->img->bits_per_pixel = 32;
  argspec->img->red_mask = argspec->red_mask;
  argspec->img->green_mask = argspec->green_mask;
  argspec->img->blue_mask = argspec->blue_mask;
  argspec->img->obdata = NULL;
}

#define XFT_COLOR_CACHE_SLOTS 64

typedef struct {
  bool used;
  Display *dpy;
  Colormap cmap;
  Visual *vis;
  unsigned long hex;
  unsigned short alpha;
  XftColor color;
  unsigned long stamp;
} XftColorSlot;

static XftColorSlot xft_color_slots[XFT_COLOR_CACHE_SLOTS];
static unsigned long xft_color_tick;
static pthread_mutex_t xft_color_cache_mtx = PTHREAD_MUTEX_INITIALIZER;

bool x11_xft_color_cached(X11XftColorCached *argspec) {
  if (!argspec->dpy || !argspec->vis || !argspec->out)
    return false;
  pthread_mutex_lock(&xft_color_cache_mtx);
  int i, slot = -1;
  unsigned long oldest = ~0UL;
  for (i = 0; i < XFT_COLOR_CACHE_SLOTS; i++) {
    XftColorSlot *s = &xft_color_slots[i];
    if (s->used && s->dpy == argspec->dpy && s->vis == argspec->vis &&
        s->cmap == argspec->cmap && s->hex == argspec->hex &&
        s->alpha == argspec->alpha) {
      s->stamp = ++xft_color_tick;
      *argspec->out = s->color;
      pthread_mutex_unlock(&xft_color_cache_mtx);
      return true;
    }
    if (!s->used) {
      if (slot < 0)
        slot = i;
    } else if (s->stamp < oldest) {
      oldest = s->stamp;
      slot = i;
    }
  }
  if (slot < 0)
    slot = 0;
  XftColorSlot *s = &xft_color_slots[slot];
  XRenderColor rc = {((argspec->hex >> 16) & 0xFF) * 0x101,
                     ((argspec->hex >> 8) & 0xFF) * 0x101,
                     (argspec->hex & 0xFF) * 0x101, argspec->alpha};
  memset(&s->color, 0, sizeof(s->color));
  if (!XftColorAllocValue(argspec->dpy, argspec->vis, argspec->cmap, &rc,
                          &s->color)) {
    pthread_mutex_unlock(&xft_color_cache_mtx);
    return false;
  }
  s->used = true;
  s->dpy = argspec->dpy;
  s->vis = argspec->vis;
  s->cmap = argspec->cmap;
  s->hex = argspec->hex;
  s->alpha = argspec->alpha;
  s->stamp = ++xft_color_tick;
  *argspec->out = s->color;
  pthread_mutex_unlock(&xft_color_cache_mtx);
  return true;
}

const char *x11_listbox_item_display_text(ListBox *lb, int idx, char *trunc,
                                          size_t trunc_size) {
  const char *base_text = lb ? listbox_info_row(lb, idx) : NULL;
  snprintf(trunc, trunc_size, "%.80s", base_text ? base_text : "");
  return trunc;
}

#define IMG_CACHE_SLOTS 256
#define X11_IMG_REVALIDATE_MS 1000

static bool x11_blit_cached(Display *dpy, Drawable drw, Visual *vis,
                            Picture src, int w, int h, int x, int y);

typedef struct {
  char key[512];
  int w, h;
  long long mtime;
  long long size;
  Display *dpy;
  Pixmap pm;
  Picture pic;
  unsigned long stamp;
  long long checked_ms;
} ImgCacheSlot;

static long long x11_now_ms(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    return 0;
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static ImgCacheSlot img_cache[IMG_CACHE_SLOTS];
static unsigned long img_cache_tick = 0;
static pthread_mutex_t img_cache_mtx = PTHREAD_MUTEX_INITIALIZER;

static void x11_release_pixmap_picture(Display *dpy, Pixmap pm, Picture pic) {
  if (pic && dpy)
    XRenderFreePicture(dpy, pic);
  if (pm && dpy)
    XFreePixmap(dpy, pm);
}

static void x11_img_slot_release(ImgCacheSlot *s) {
  x11_release_pixmap_picture(s->dpy, s->pm, s->pic);
  s->pic = 0;
  s->pm = 0;
  s->dpy = NULL;
}

void x11_img_cache_free_all(void) {
  pthread_mutex_lock(&img_cache_mtx);
  for (int i = 0; i < IMG_CACHE_SLOTS; i++) {
    x11_img_slot_release(&img_cache[i]);
    memset(&img_cache[i], 0, sizeof(img_cache[i]));
  }
  pthread_mutex_unlock(&img_cache_mtx);
}

static bool x11_img_cache_get(const char *key, int w, int h, Pixmap *out_pm,
                              Picture *out_pic) {
  long long now = x11_now_ms();
  for (int i = 0; i < IMG_CACHE_SLOTS; i++) {
    ImgCacheSlot *s = &img_cache[i];
    if (!s->pm || !s->pic)
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
        x11_img_slot_release(s);
        memset(s, 0, sizeof(*s));
        continue;
      }
    }
    s->stamp = ++img_cache_tick;
    *out_pm = s->pm;
    *out_pic = s->pic;
    return true;
  }
  return false;
}

static void x11_img_cache_put(Display *dpy, const char *key, int w, int h,
                              long long mtime, long long size, Pixmap pm,
                              Picture pic) {
  if (!pm || !pic)
    return;
  int slot = -1;
  unsigned long oldest = ~0UL;
  for (int i = 0; i < IMG_CACHE_SLOTS; i++) {
    const ImgCacheSlot *s = &img_cache[i];
    if (!s->pm && !s->pic) {
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
  ImgCacheSlot *s = &img_cache[slot];
  x11_img_slot_release(s);
  memset(s, 0, sizeof(*s));
  snprintf(s->key, sizeof(s->key), "%s", key);
  s->w = w;
  s->h = h;
  s->mtime = mtime;
  s->size = size;
  s->dpy = dpy;
  s->pm = pm;
  s->pic = pic;
  s->checked_ms = x11_now_ms();
  s->stamp = ++img_cache_tick;
}

static pthread_mutex_t decode_mtx;
static unsigned char *decode_locked(const char *path, int *out_w, int *out_h);

bool x11_upload_argb(X11UploadArgb *argspec) {
  if (!argspec->dpy || !argspec->argb || argspec->w <= 0 || argspec->h <= 0)
    return false;
  if (!argspec->ref)
    argspec->ref = DefaultRootWindow(argspec->dpy);
  Pixmap pm =
      XCreatePixmap(argspec->dpy, argspec->ref, argspec->w, argspec->h, 32);
  if (!pm)
    return false;
  XImage img;
  x11_init_argb_image(&(X11InitArgbImage){.dpy = argspec->dpy,
                                          .img = &img,
                                          .data = argspec->argb,
                                          .w = argspec->w,
                                          .h = argspec->h,
                                          .red_mask = 0xff0000,
                                          .green_mask = 0x00ff00,
                                          .blue_mask = 0x0000ff});
  if (!XInitImage(&img)) {
    XFreePixmap(argspec->dpy, pm);
    return false;
  }
  GC gc = XCreateGC(argspec->dpy, pm, 0, NULL);
  XPutImage(argspec->dpy, pm, gc, &img, 0, 0, 0, 0, argspec->w, argspec->h);
  XFreeGC(argspec->dpy, gc);
  Picture pic = XRenderCreatePicture(
      argspec->dpy, pm,
      XRenderFindStandardFormat(argspec->dpy, PictStandardARGB32), 0, NULL);
  if (!pic) {
    XFreePixmap(argspec->dpy, pm);
    return false;
  }
  if (argspec->out_pm)
    *argspec->out_pm = pm;
  if (argspec->out_pic)
    *argspec->out_pic = pic;
  return true;
}

#define ARGB_DISK_MAGIC 0x534b415242000001ull

typedef struct {
  uint64_t magic;
  uint64_t mtime;
  uint64_t size;
  int32_t w;
  int32_t h;
} ArgbDiskHeader;

static uint64_t argb_disk_hash(const char *path, long long mtime,
                               long long size, int w, int h) {
  uint64_t hash = 1469598103934665603ull;
  for (const unsigned char *p = (const unsigned char *)path; *p; p++) {
    hash ^= *p;
    hash *= 1099511628211ull;
  }
  const uint64_t fields[4] = {(uint64_t)mtime, (uint64_t)size, (uint64_t)w,
                              (uint64_t)h};
  for (int i = 0; i < 4; i++) {
    hash ^= fields[i];
    hash *= 1099511628211ull;
  }
  return hash;
}

static bool argb_disk_path(char *out, size_t out_len, const char *path,
                           long long mtime, long long size, int w, int h) {
  static bool ready = false;
  char dir[512];
  char cache_dir[640];
  if (!ready) {
    if (!data_dir(dir, sizeof(dir)))
      return false;
    snprintf(cache_dir, sizeof(cache_dir), "%s/imgcache", dir);
    Platform_MkdirIfNotExists(cache_dir);
    ready = true;
  }
  if (!data_dir(dir, sizeof(dir)))
    return false;
  snprintf(out, out_len, "%s/imgcache/%016llx.argb", dir,
           (unsigned long long)argb_disk_hash(path, mtime, size, w, h));
  return true;
}

static bool argb_disk_load(const char *path, long long mtime, long long size,
                           int w, int h, unsigned char **out_argb) {
  char cache_path[768];
  if (!argb_disk_path(cache_path, sizeof(cache_path), path, mtime, size, w, h))
    return false;
  FILE *f = fopen(cache_path, "rb");
  if (!f)
    return false;
  ArgbDiskHeader header = {0};
  if (fread(&header, sizeof(header), 1, f) != 1) {
    fclose(f);
    return false;
  }
  if (header.magic != ARGB_DISK_MAGIC || header.mtime != (uint64_t)mtime ||
      header.size != (uint64_t)size || header.w != (int32_t)w ||
      header.h != (int32_t)h) {
    fclose(f);
    return false;
  }
  size_t bytes = (size_t)w * (size_t)h * 4;
  unsigned char *buf = malloc(bytes);
  if (!buf) {
    fclose(f);
    return false;
  }
  if (fread(buf, 1, bytes, f) != bytes) {
    free(buf);
    fclose(f);
    return false;
  }
  fclose(f);
  *out_argb = buf;
  return true;
}

static void argb_disk_store(const char *path, long long mtime, long long size,
                            int w, int h, const unsigned char *argb) {
  char cache_path[768];
  if (!argb_disk_path(cache_path, sizeof(cache_path), path, mtime, size, w, h))
    return;
  char tmp_path[800];
  snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", cache_path);
  FILE *f = fopen(tmp_path, "wb");
  if (!f)
    return;
  ArgbDiskHeader header = {ARGB_DISK_MAGIC, (uint64_t)mtime, (uint64_t)size,
                           (int32_t)w, (int32_t)h};
  fwrite(&header, sizeof(header), 1, f);
  fwrite(argb, 1, (size_t)w * (size_t)h * 4, f);
  fclose(f);
  rename(tmp_path, cache_path);
}

static bool image_cache_store(Display *dpy, Drawable ref, const char *path,
                              int w, int h, Picture *out_pic) {
  if (!dpy || !path || !path[0] || w <= 0 || h <= 0)
    return false;
  struct stat st;
  memset(&st, 0, sizeof(st));
  if (stat(path, &st) != 0)
    return false;
  if (!ref)
    ref = DefaultRootWindow(dpy);
  long long mtime = (long long)st.st_mtime;
  long long size = (long long)st.st_size;
  pthread_mutex_lock(&decode_mtx);
  unsigned char *argb = NULL;
  bool from_disk = argb_disk_load(path, mtime, size, w, h, &argb);
  if (!from_disk)
    argb = decode_locked(path, &w, &h);
  if (!argb) {
    pthread_mutex_unlock(&decode_mtx);
    return false;
  }
  if (!from_disk)
    argb_disk_store(path, mtime, size, w, h, argb);
  Pixmap pm = 0;
  Picture pic = 0;
  bool uploaded = x11_upload_argb(&(X11UploadArgb){.dpy = dpy,
                                                   .ref = ref,
                                                   .argb = argb,
                                                   .w = w,
                                                   .h = h,
                                                   .out_pm = &pm,
                                                   .out_pic = &pic});
  if (from_disk)
    free(argb);
  pthread_mutex_unlock(&decode_mtx);
  if (!uploaded)
    return false;
  pthread_mutex_lock(&img_cache_mtx);
  x11_img_cache_put(dpy, path, w, h, mtime, size, pm, pic);
  if (out_pic)
    *out_pic = pic;
  return true;
}

bool x11_prewarm_image(Display *dpy, const char *path) {
  if (!dpy || !path || !path[0])
    return false;
  int size = IconSize > 0 ? IconSize : 32;
  Pixmap pm = 0;
  Picture cached = 0;
  pthread_mutex_lock(&img_cache_mtx);
  bool hit = x11_img_cache_get(path, size, size, &pm, &cached);
  pthread_mutex_unlock(&img_cache_mtx);
  if (hit)
    return true;
  Picture pic = 0;
  bool ok =
      image_cache_store(dpy, DefaultRootWindow(dpy), path, size, size, &pic);
  if (!ok)
    return false;
  pthread_mutex_unlock(&img_cache_mtx);
  return true;
}

bool x11_draw_image_scaled(X11DrawImageScaled *argspec) {
  if (!argspec->dpy || !argspec->xd || !argspec->path || !argspec->path[0])
    return false;
  if (argspec->tw <= 0)
    argspec->tw = IconSize;
  if (argspec->th <= 0)
    argspec->th = IconSize;
  int cw = argspec->tw;
  int ch = argspec->th;
  Pixmap cpm = 0;
  Picture cpic = 0;
  pthread_mutex_lock(&img_cache_mtx);
  bool hit = x11_img_cache_get(argspec->path, cw, ch, &cpm, &cpic);
  Drawable drw = XftDrawDrawable(argspec->xd);
  if (hit) {
    bool ok = x11_blit_cached(argspec->dpy, drw, argspec->vis, cpic, cw, ch,
                              argspec->x, argspec->y);
    pthread_mutex_unlock(&img_cache_mtx);
    return ok;
  }
  pthread_mutex_unlock(&img_cache_mtx);
  bool ok = image_cache_store(argspec->dpy, drw, argspec->path, cw, ch, &cpic);
  if (!ok) {
    return false;
  }
  ok = x11_blit_cached(argspec->dpy, drw, argspec->vis, cpic, cw, ch,
                       argspec->x, argspec->y);
  pthread_mutex_unlock(&img_cache_mtx);
  return ok;
}

static bool x11_blit_cached(Display *dpy, Drawable drw, Visual *vis,
                            Picture src, int w, int h, int x, int y) {
  if (!dpy || !src || w <= 0 || h <= 0)
    return false;
  if (!vis)
    vis = DefaultVisualOfScreen(DefaultScreenOfDisplay(dpy));
  XRenderPictFormat *dst_fmt = XRenderFindVisualFormat(dpy, vis);
  if (!dst_fmt)
    return false;
  Picture dst = XRenderCreatePicture(dpy, drw, dst_fmt, 0, NULL);
  if (!dst)
    return false;
  const DrawCanvas *ds = draw_state_get();
  if (ds->clip_y1 > ds->clip_y0) {
    XRectangle clip = {0, (short)ds->clip_y0, 65535,
                       (unsigned short)(ds->clip_y1 - ds->clip_y0)};
    XRenderSetPictureClipRectangles(dpy, dst, 0, 0, &clip, 1);
  }
  XRenderComposite(dpy, PictOpOver, src, None, dst, 0, 0, 0, 0, x, y, w, h);
  XRenderFreePicture(dpy, dst);
  return true;
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
    safe_copy(keys[ci], 16, keybuf);
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
static pthread_mutex_t decode_mtx = PTHREAD_MUTEX_INITIALIZER;

static unsigned char *decode_locked(const char *path, int *out_w, int *out_h) {
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
      *out_w = dst_w;
      *out_h = dst_h;
      return slot->argb;
    }
  }

  size_t _plen = strlen(path);
  bool _is_svg = _plen > 4 && string_contains_ci(path + _plen - 4, ".svg");

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
    bool _is_xpm = _plen > 4 && string_contains_ci(path + _plen - 4, ".xpm");
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

  *out_w = dst_w;
  *out_h = dst_h;
  return decoded_cache[idx].argb;
}

bool x11_get_image_size(const char *path, int *out_w, int *out_h) {
  if (!path || !path[0] || !out_w || !out_h)
    return false;
  int w = 0, h = 0, channels = 0;
  if (svg_file_size(path, &w, &h))
    goto done;
  if (!stbi_info(path, &w, &h, &channels) || w <= 0 || h <= 0)
    return false;
done:
  *out_w = w;
  *out_h = h;
  return true;
}

unsigned char *x11_decode_image_argb(const char *path, int *out_w, int *out_h) {
  pthread_mutex_lock(&decode_mtx);
  int dst_w = *out_w;
  int dst_h = *out_h;
  if (dst_w <= 0)
    dst_w = 16;
  if (dst_h <= 0)
    dst_h = 16;
  unsigned char *borrowed = decode_locked(path, out_w, out_h);
  if (!borrowed) {
    pthread_mutex_unlock(&decode_mtx);
    return NULL;
  }
  size_t bytes = (size_t)(*out_w) * (size_t)(*out_h) * 4;
  unsigned char *copy = (unsigned char *)malloc(bytes);
  if (copy)
    memcpy(copy, borrowed, bytes);
  pthread_mutex_unlock(&decode_mtx);
  return copy;
}

void x11_draw_listbox_item_text(X11DrawListboxItemText *argspec) {
  const XftFont *xf = FH_TO_XFT(argspec->font);
  if (!xf)
    return;
  int XOffset = PROMPT_ITEM_TEXT_X;
  int row_h = prompt_row_height();
  int pad_top = (row_h - (xf->ascent + xf->descent)) / 2;
  if (pad_top < 0)
    pad_top = 0;
  int text_y = argspec->y + pad_top;
  if (argspec->icon_path) {
    int iconX = XOffset;
    int icon_y = argspec->y + (prompt_row_height() - IconSize) / 2;
    XOffset += IconSize + XOffset;
    x11_draw_image_scaled(&(X11DrawImageScaled){.dpy = argspec->dpy,
                                                .xd = argspec->xd,
                                                .vis = argspec->vis,
                                                .path = argspec->icon_path,
                                                .x = iconX,
                                                .y = icon_y,
                                                .tw = IconSize,
                                                .th = IconSize});
  }
  char trunc[512];
  x11_listbox_item_display_text(argspec->lb, argspec->idx, trunc,
                                sizeof(trunc));

  const char *draw_text = trunc;
  int draw_x = XOffset;
  int h_scroll = (argspec->lb && argspec->idx == argspec->lb->selected_index)
                     ? argspec->lb->text_hscroll
                     : 0;
  if (h_scroll > 0)
    draw_x -= h_scroll;

  unsigned long pcol = argspec->plain_color ? argspec->plain_color->pixel : 0;
  if (!pcol)
    pcol = rgb_to_hex(COLOR_TEXT);
  x11_set_draw_context(argspec->dpy, argspec->xd, argspec->vis, argspec->cmap);
  Platform_DrawTextRaw((DrawTextRaw){.x = draw_x,
                                     .y = text_y,
                                     .text = draw_text,
                                     .color = pcol,
                                     .font = argspec->font,
                                     .align = "LEFT",
                                     .pad = XOffset,
                                     .font_size = FontSize});
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

void x11_set_fixed_size_hints(Display *dpy, Window win, int w, int h) {
  XSizeHints *sh = XAllocSizeHints();
  sh->flags = PMinSize | PMaxSize;
  sh->min_width = w;
  sh->min_height = h;
  sh->max_width = w;
  sh->max_height = h;
  XSetWMNormalHints(dpy, win, sh);
  XFree(sh);
}

Window x11_create_window_ex(X11CreateWindowEx *argspec) {
  return x11_create_window_ex_or(argspec->dpy, argspec->x, argspec->y,
                                 argspec->w, argspec->h, argspec->bg_color,
                                 argspec->res_name, argspec->res_class,
                                 argspec->event_mask, false);
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
  Pixmap bg_pm = (get_transparent_window() || !x11_bg_image_enabled())
                     ? None
                     : x11_bg_image_tile(dpy, w, h);
  if (use_argb) {
    XSetWindowAttributes attrs = {0};
    unsigned int r = (bg_color >> 16) & 0xFF;
    unsigned int g = (bg_color >> 8) & 0xFF;
    unsigned int b = bg_color & 0xFF;
    unsigned int a = get_transparent_window() ? 0 : (unsigned int)WindowOpacity;
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
  if (RoundedCorners && CornerRadius > 0) {
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
      if (x11_corner_alpha(x, y, w, h, radius, 255) < 128)
        continue;
      XFillRectangle(dpy, mask, gc, x, y, 1, 1);
    }
  }
  int shape_kind = x11_env_is_wayland() ? ShapeInput : ShapeBounding;
  XShapeCombineMask(dpy, win, shape_kind, 0, 0, mask, ShapeSet);
  XSync(dpy, False);
  XFreePixmap(dpy, mask);
  XFreeGC(dpy, gc);
}

static unsigned long x11_background_color_at(void) {
  unsigned long base = get_draw_bg_color();
  if (!base)
    base = rgb_to_hex(COLOR_BG);
  return ((base >> 16) & 0xFF) << 16 | ((base >> 8) & 0xFF) << 8 |
         (base & 0xFF);
}

#define ROUND_MASK_MAX_RADIUS 128
static unsigned char *round_masks[4][ROUND_MASK_MAX_RADIUS + 1];
static pthread_mutex_t round_mask_mutex = PTHREAD_MUTEX_INITIALIZER;

static void x11_round_mask_fill(unsigned char *out, int corner, int radius) {
  const float edge_lo = (float)radius - 0.5f;
  const float edge_hi = (float)radius + 0.5f;
  const float edge_sq_lo = edge_lo * edge_lo;
  const float edge_sq_hi = edge_hi * edge_hi;
  for (int dy = 0; dy < radius; dy++) {
    float ddy = (corner & 2) ? (float)(dy + 1) : (float)(dy - radius);
    for (int dx = 0; dx < radius; dx++) {
      float ddx = (corner & 1) ? (float)(dx + 1) : (float)(dx - radius);
      float d_sq = ddx * ddx + ddy * ddy;
      unsigned char cov = 0;
      if (d_sq < edge_sq_hi) {
        if (d_sq <= edge_sq_lo) {
          cov = 255;
        } else {
          float d = sqrtf(d_sq);
          float f = edge_hi - d;
          if (f < 0.0f)
            f = 0.0f;
          if (f > 1.0f)
            f = 1.0f;
          cov = (unsigned char)(f * 255.0f + 0.5f);
        }
      }
      out[dy * radius + dx] = cov;
    }
  }
}

static const unsigned char *x11_round_mask(int corner, int radius) {
  if (radius < 1 || radius > ROUND_MASK_MAX_RADIUS)
    return NULL;
  pthread_mutex_lock(&round_mask_mutex);
  unsigned char *mask = round_masks[corner][radius];
  if (!mask) {
    mask = (unsigned char *)malloc((size_t)radius * (size_t)radius);
    if (mask)
      x11_round_mask_fill(mask, corner, radius);
    round_masks[corner][radius] = mask;
  }
  pthread_mutex_unlock(&round_mask_mutex);
  return mask;
}

void x11_fill_rounded_rect(X11FillRoundedRect *argspec) {
  const DrawCanvas *ds = draw_state_get();
  bool clipped = argspec->xd && ds->clip_y1 > ds->clip_y0;
  if (clipped) {
    int clip_h = ds->clip_y1 - ds->clip_y0;
    XRectangle clip = {0, (short)ds->clip_y0, 65535,
                       (unsigned short)(clip_h > 65535 ? 65535 : clip_h)};
    XftDrawSetClipRectangles(argspec->xd, 0, 0, &clip, 1);
  }
  if (!RoundedCorners || argspec->radius <= 0 || argspec->w <= 0 ||
      argspec->h <= 0) {
    XftColor c;
    x11_xft_color_cached(&(X11XftColorCached){.dpy = argspec->dpy,
                                              .vis = argspec->vis,
                                              .cmap = argspec->cmap,
                                              .hex = argspec->hex,
                                              .alpha = argspec->alpha16,
                                              .out = &c});
    XftDrawRect(argspec->xd, &c, argspec->x, argspec->y, argspec->w,
                argspec->h);
    if (clipped)
      XftDrawSetClip(argspec->xd, None);
    return;
  }
  if (argspec->radius > argspec->w / 2)
    argspec->radius = argspec->w / 2;
  if (argspec->radius > argspec->h / 2)
    argspec->radius = argspec->h / 2;
  XftColor solid;
  x11_xft_color_cached(&(X11XftColorCached){.dpy = argspec->dpy,
                                            .vis = argspec->vis,
                                            .cmap = argspec->cmap,
                                            .hex = argspec->hex,
                                            .alpha = argspec->alpha16,
                                            .out = &solid});

  XftDrawRect(argspec->xd, &solid, argspec->x + argspec->radius, argspec->y,
              argspec->w - 2 * argspec->radius, argspec->h);
  XftDrawRect(argspec->xd, &solid, argspec->x, argspec->y + argspec->radius,
              argspec->radius, argspec->h - 2 * argspec->radius);
  XftDrawRect(argspec->xd, &solid, argspec->x + argspec->w - argspec->radius,
              argspec->y + argspec->radius, argspec->radius,
              argspec->h - 2 * argspec->radius);

  const unsigned long backdrop = x11_background_color_at();
  XftColor corner_colors[256];
  bool corner_color_ready[256] = {false};

  for (int corner = 0; corner < 4; corner++) {
    int ox0 =
        (corner & 1) ? argspec->x + argspec->w - argspec->radius : argspec->x;
    int oy0 =
        (corner & 2) ? argspec->y + argspec->h - argspec->radius : argspec->y;
    const unsigned char *mask = x11_round_mask(corner, argspec->radius);
    unsigned char *local_mask = NULL;
    if (!mask && argspec->radius > ROUND_MASK_MAX_RADIUS) {
      local_mask = (unsigned char *)malloc((size_t)argspec->radius *
                                           (size_t)argspec->radius);
      if (local_mask) {
        x11_round_mask_fill(local_mask, corner, argspec->radius);
        mask = local_mask;
      }
    }
    if (!mask)
      continue;
    for (int dy = 0; dy < argspec->radius; dy++) {
      int run_start = 0;
      unsigned char run_cov = mask[dy * argspec->radius];
      for (int dx = 1; dx <= argspec->radius; dx++) {
        unsigned char cov =
            (dx < argspec->radius) ? mask[dy * argspec->radius + dx] : 0;
        if (cov == run_cov)
          continue;
        if (run_cov) {
          if (!corner_color_ready[run_cov]) {
            unsigned long mixed =
                color_lerp(backdrop, argspec->hex, (float)run_cov / 255.0f);
            XftColor c = solid;
            c.color.red = (unsigned short)(((mixed >> 16) & 0xFF) * 257);
            c.color.green = (unsigned short)(((mixed >> 8) & 0xFF) * 257);
            c.color.blue = (unsigned short)((mixed & 0xFF) * 257);
            c.color.alpha = 0xFFFF;
            corner_colors[run_cov] = c;
            corner_color_ready[run_cov] = true;
          }
          XftDrawRect(argspec->xd, &corner_colors[run_cov], ox0 + run_start,
                      oy0 + dy, dx - run_start, 1);
        }
        run_start = dx;
        run_cov = cov;
      }
    }
    free(local_mask);
  }
  if (clipped)
    XftDrawSetClip(argspec->xd, None);
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
                                           int h,
                                           const XWindowAttributes *attrs,
                                           unsigned long base,
                                           unsigned int baseAlpha8) {
  Visual *vis = attrs->visual;
  int depth = attrs->depth;
  if (depth != 32)
    return None;
  bool has_alpha = true;
  bool rounded = RoundedCorners && CornerRadius > 0 && has_alpha;
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
    x11_bg_image_blit(
        &(X11BgImageBlit){.dpy = dpy, .dst = pm, .vis = vis, .w = w, .h = h});
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
  unsigned long base = get_draw_bg_color();
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
  unsigned long base = get_draw_bg_color();
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
  if (!x11_get_drawable_visual(&(X11GetDrawableVisual){.dpy = dpy,
                                                       .d = drw,
                                                       .vis_out = &vis,
                                                       .cmap_out = &cmap,
                                                       .depth_out = &depth})) {
    vis = DefaultVisual(dpy, screen);
    depth = DefaultDepth(dpy, screen);
  }
  bool has_alpha = (depth == 32);
  bool rded = rounded && radius > 0 && has_alpha;
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
  if (!has_alpha) {
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

void x11_draw_selection_background(X11DrawSelectionBackground *argspec) {

  if (argspec->w <= 0 || argspec->h <= 0)
    return;
  const DrawCanvas *ds = draw_state_get();
  bool clipped = argspec->xd && ds->clip_y1 > ds->clip_y0;
  if (clipped) {
    int clip_h = ds->clip_y1 - ds->clip_y0;
    XRectangle clip = {0, (short)ds->clip_y0, 65535,
                       (unsigned short)(clip_h > 65535 ? 65535 : clip_h)};
    XftDrawSetClipRectangles(argspec->xd, 0, 0, &clip, 1);
  }
  XftColor c;
  x11_xft_color_cached(&(X11XftColorCached){.dpy = argspec->dpy,
                                            .vis = argspec->vis,
                                            .cmap = argspec->cmap,
                                            .hex = argspec->baseHex,
                                            .alpha = opacity_alpha16(),
                                            .out = &c});
  XftDrawRect(argspec->xd, &c, argspec->x, argspec->y, argspec->w, argspec->h);
  if (clipped)
    XftDrawSetClip(argspec->xd, None);
}

#define BORDER_CACHE_SLOTS 4

typedef struct {
  bool used;
  Display *dpy;
  int w, h, t, r;
  unsigned long hex;
  unsigned long a8;
  unsigned long stamp;
  Pixmap pm;
  Picture pic;
} BorderSlot;

static BorderSlot border_slots[BORDER_CACHE_SLOTS];
static unsigned long border_tick;
static pthread_mutex_t border_mtx = PTHREAD_MUTEX_INITIALIZER;

static void border_slot_release(BorderSlot *s) {
  x11_release_pixmap_picture(s->dpy, s->pm, s->pic);
  s->pic = 0;
  s->pm = 0;
  s->used = false;
}

void x11_border_cache_free_all(void) {
  pthread_mutex_lock(&border_mtx);
  for (int i = 0; i < BORDER_CACHE_SLOTS; i++) {
    border_slot_release(&border_slots[i]);
    memset(&border_slots[i], 0, sizeof(border_slots[i]));
  }
  pthread_mutex_unlock(&border_mtx);
}

static Picture border_slot_get_or_create(Display *dpy, int w, int h) {
  int t = PROMPT_BORDER_W;
  if (w <= 2 * t || h <= 2 * t)
    return 0;
  int r = (RoundedCorners && CornerRadius > 0) ? CornerRadius : 0;
  if (r > w / 2)
    r = w / 2;
  if (r > h / 2)
    r = h / 2;
  if (r <= t)
    r = 0;
  unsigned long hex = rgb_to_hex(COLOR_BORDER);
  unsigned long a8 = ((unsigned long)((int)WindowOpacity)) & 0xFFUL;
  int i, slot = -1;
  unsigned long oldest = ~0UL;
  for (i = 0; i < BORDER_CACHE_SLOTS; i++) {
    BorderSlot *s = &border_slots[i];
    if (s->used && s->dpy == dpy && s->w == w && s->h == h && s->t == t &&
        s->r == r && s->hex == hex && s->a8 == a8) {
      s->stamp = ++border_tick;
      return s->pic;
    }
    if (!s->used) {
      if (slot < 0)
        slot = i;
    } else if (s->stamp < oldest) {
      oldest = s->stamp;
      slot = i;
    }
  }
  unsigned char *buf = (unsigned char *)calloc((size_t)w * (size_t)h * 4, 1);
  if (!buf)
    return 0;
  unsigned int a = (unsigned int)a8;
  unsigned int hr = (unsigned int)((hex >> 16) & 0xFF),
               hg = (unsigned int)((hex >> 8) & 0xFF),
               hb = (unsigned int)(hex & 0xFF);
  unsigned int pr = hr * a / 255, pg = hg * a / 255, pb = hb * a / 255;
  for (int y = 0; y < h; y++) {
    unsigned char *row = buf + (size_t)y * (size_t)w * 4;
    for (int x = 0; x < w; x++) {
      bool edge = (x < t) || (x >= w - t) || (y < t) || (y >= h - t);
      if (!edge)
        continue;
      if (r > t && ((x < r) || (x >= w - r)) && ((y < r) || (y >= h - r)))
        continue;
      uint32_t px = (uint32_t)((a << 24) | (pr << 16) | (pg << 8) | pb);
      memcpy(row + (size_t)x * 4, &px, 4);
    }
  }
  if (r > t) {
    float ri = (float)r - PROMPT_BORDER_INNER_OFF;
    for (int y = 0; y < h; y++) {
      bool yband = (y < r) || (y >= h - r);
      if (!yband)
        continue;
      int cy = (y < r) ? r : h - r - 1;
      for (int x = 0; x < w; x++) {
        bool xband = (x < r) || (x >= w - r);
        if (!xband)
          continue;
        int cx = (x < r) ? r : w - r - 1;
        float dx = (float)x - (float)cx;
        float dy = (float)y - (float)cy;
        if (dx * dx + dy * dy <= ri * ri)
          continue;
        unsigned int ca = x11_corner_alpha(x, y, w, h, r, 255);
        if (!ca)
          continue;
        unsigned int a2 = a * ca / 255;
        if (!a2)
          continue;
        unsigned int pr2 = hr * a2 / 255, pg2 = hg * a2 / 255,
                     pb2 = hb * a2 / 255;
        uint32_t px = (uint32_t)((a2 << 24) | (pr2 << 16) | (pg2 << 8) | pb2);
        memcpy(buf + ((size_t)y * (size_t)w + (size_t)x) * 4, &px, 4);
      }
    }
  }
  Pixmap pm = 0;
  Picture pic = 0;
  bool ok = x11_upload_argb(&(X11UploadArgb){.dpy = dpy,
                                             .ref = DefaultRootWindow(dpy),
                                             .argb = buf,
                                             .w = w,
                                             .h = h,
                                             .out_pm = &pm,
                                             .out_pic = &pic});
  free(buf);
  if (!ok)
    return 0;
  if (slot < 0)
    slot = 0;
  BorderSlot *s = &border_slots[slot];
  border_slot_release(s);
  memset(s, 0, sizeof(*s));
  s->used = true;
  s->dpy = dpy;
  s->w = w;
  s->h = h;
  s->t = t;
  s->r = r;
  s->hex = hex;
  s->a8 = a8;
  s->pm = pm;
  s->pic = pic;
  s->stamp = ++border_tick;
  return pic;
}

void x11_draw_window_border(X11DrawWindowBorder *argspec) {
  if (!argspec->dpy || !argspec->drw || argspec->no_bg || !EnableBorders) {
    return;
  }
  Visual *use_vis = argspec->vis;
  if (!use_vis) {
    XWindowAttributes attrs;
    if (XGetWindowAttributes(argspec->dpy, argspec->drw, &attrs))
      use_vis = attrs.visual;
  }
  XRenderPictFormat *dst_fmt =
      use_vis ? XRenderFindVisualFormat(argspec->dpy, use_vis) : NULL;
  pthread_mutex_lock(&border_mtx);
  Picture src = border_slot_get_or_create(argspec->dpy, argspec->w, argspec->h);
  if (dst_fmt && src) {
    Picture dst =
        XRenderCreatePicture(argspec->dpy, argspec->drw, dst_fmt, 0, NULL);
    if (dst) {
      XRenderComposite(argspec->dpy, PictOpOver, src, None, dst, 0, 0, 0, 0, 0,
                       0, argspec->w, argspec->h);
      XRenderFreePicture(argspec->dpy, dst);
    }
  }
  pthread_mutex_unlock(&border_mtx);
}

Window x11_create_prompt_window(X11CreatePromptWindow *argspec) {
  unsigned long bg = rgb_to_hex(COLOR_BG);
  Window win;
  if (niri_is_active()) {
    log_error("niri override_redirect prompt w=%d h=%d", argspec->w,
              argspec->h);
    win = x11_create_window_ex_or(
        argspec->dpy, argspec->x, argspec->y, argspec->w, argspec->h, bg,
        "colored", "colored",
        KeyPressMask | KeyReleaseMask | ExposureMask | FocusChangeMask, true);
  } else {
    win = x11_create_window_ex(
        &(X11CreateWindowEx){.dpy = argspec->dpy,
                             .x = argspec->x,
                             .y = argspec->y,
                             .w = argspec->w,
                             .h = argspec->h,
                             .bg_color = bg,
                             .res_name = "colored",
                             .res_class = "colored",
                             .event_mask = KeyPressMask | KeyReleaseMask |
                                           ExposureMask | FocusChangeMask});
    Window focus_win = None;
    int revert_to = 0;
    XGetInputFocus(argspec->dpy, &focus_win, &revert_to);
    if (focus_win != None && focus_win != PointerRoot && focus_win != win)
      XSetTransientForHint(argspec->dpy, win, focus_win);
    x11_set_fixed_size_hints(argspec->dpy, win, argspec->w, argspec->h);
  }
  if (RoundedCorners && CornerRadius > 0)
    x11_apply_shape_rounded(argspec->dpy, win, argspec->w, argspec->h,
                            CornerRadius);
  return win;
}

void x11_apply_shape_rounded_window(Display *dpy, Window win, int w, int h) {
  if (!dpy || win == None || !RoundedCorners || CornerRadius <= 0)
    return;
  x11_apply_shape_rounded(dpy, win, w, h, CornerRadius);
}

void x11_apply_shape(X11ApplyShape *argspec) {
  if (!argspec->dpy || argspec->win == None || argspec->w <= 0 ||
      argspec->h <= 0)
    return;
  if (argspec->rounded && argspec->radius > 0) {
    x11_apply_shape_rounded(argspec->dpy, argspec->win, argspec->w, argspec->h,
                            argspec->radius);
    return;
  }
  Pixmap mask =
      XCreatePixmap(argspec->dpy, argspec->win, argspec->w, argspec->h, 1);
  if (!mask)
    return;
  GC gc = XCreateGC(argspec->dpy, mask, 0, NULL);
  if (!gc) {
    XFreePixmap(argspec->dpy, mask);
    return;
  }
  XSetForeground(argspec->dpy, gc, 1);
  XFillRectangle(argspec->dpy, mask, gc, 0, 0, argspec->w, argspec->h);
  XShapeCombineMask(argspec->dpy, argspec->win, ShapeBounding, 0, 0, mask,
                    ShapeSet);
  XFreeGC(argspec->dpy, gc);
  XFreePixmap(argspec->dpy, mask);
}

void x11_apply_click_through(Display *dpy, Window win) {
  if (!dpy || win == None)
    return;
  XShapeCombineRectangles(dpy, win, ShapeInput, 0, 0, NULL, 0, ShapeSet,
                          Unsorted);
}

Window
x11_create_display_message_window_ex(X11CreateDisplayMessageWindowEx *argspec) {
  unsigned long bg = 0;
  if (!get_transparent_window()) {
    bg = get_draw_bg_color();
    if (!bg)
      bg = rgb_to_hex(COLOR_BG);
  }
  x11_set_draw_context(argspec->dpy, NULL, NULL, 0);
  long mask = ExposureMask | (argspec->clickable ? ButtonPressMask : 0L);
  Window win = x11_create_window_ex_or(argspec->dpy, argspec->x, argspec->y,
                                       argspec->w, argspec->h, bg, "flashtext",
                                       "flashtext", mask, true);
  if (win != None && !argspec->clickable)
    x11_apply_click_through(argspec->dpy, win);

  x11_set_draw_context(NULL, NULL, NULL, 0);
  if (win != None) {
    bool want_rounded = argspec->rounded ? argspec->rounded : RoundedCorners;
    x11_apply_shape(&(X11ApplyShape){
        .dpy = argspec->dpy,
        .win = win,
        .w = argspec->w,
        .h = argspec->h,
        .rounded = want_rounded,
        .radius = argspec->radius > 0 ? argspec->radius : CornerRadius});
  }
  return win;
}

Window
x11_create_display_message_window(X11CreateDisplayMessageWindow *argspec) {
  return x11_create_display_message_window_ex(
      &(X11CreateDisplayMessageWindowEx){.dpy = argspec->dpy,
                                         .x = argspec->x,
                                         .y = argspec->y,
                                         .w = argspec->w,
                                         .h = argspec->h,
                                         .clickable = argspec->clickable,
                                         .rounded = RoundedCorners,
                                         .radius = CornerRadius});
}

XftDraw *x11_create_xft_draw(Display *dpy, Window win) {
  Visual *vis = NULL;
  Colormap cmap = 0;
  int depth = 0;
  if (x11_get_drawable_visual(&(X11GetDrawableVisual){.dpy = dpy,
                                                      .d = win,
                                                      .vis_out = &vis,
                                                      .cmap_out = &cmap,
                                                      .depth_out = &depth})) {
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
    if (string_starts_with_ci(p, "Terminal=")) {
      char *v = p + 9;
      while (*v == ' ' || *v == '\t')
        v++;
      v[strcspn(v, "\r\n")] = '\0';
      if (is_terminal && (string_contains_ci(v, "true") || strcmp(v, "1") == 0))
        *is_terminal = true;
    } else if (string_starts_with_ci(p, "Exec=")) {
      char *v = p + 5;
      while (*v == ' ' || *v == '\t')
        v++;
      v[strcspn(v, "\r\n")] = '\0';
      if (exec_line)
        snprintf(exec_line, 1024, "%s", v);
    } else if (string_starts_with_ci(p, "Path=")) {
      char *v = p + 5;
      while (*v == ' ' || *v == '\t')
        v++;
      v[strcspn(v, "\r\n")] = '\0';
      if (workdir)
        snprintf(workdir, 1024, "%s", v);
    }
  }
  string_list_free(&lines);
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
  safe_copy(exec_line, 1024, s);
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

bool x11_resolve_desktop_exec(X11ResolveDesktopExec *argspec) {
  if (!argspec->exec_line || !argspec->out || argspec->out_size == 0)
    return false;
  const char *p = argspec->exec_line;
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

  const char *base = (argspec->workdir && argspec->workdir[0])
                         ? argspec->workdir
                         : argspec->desktop_dir;
  char prog[1024];
  if (strchr(token, '/')) {
    if (token[0] == '/')
      snprintf(prog, sizeof(prog), "%.1023s", token);
    else
      snprintf(prog, sizeof(prog), "%.511s/%.511s", base, token);
  } else if (x11_exe_in_path(token)) {
    snprintf(prog, sizeof(prog), "%.1023s", token);
  } else {
    snprintf(prog, sizeof(prog), "%.511s/%.511s", argspec->desktop_dir, token);
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

  snprintf(argspec->out, argspec->out_size, "%s%s%s", q, p[0] ? " " : "", p);
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
