#include "helpers/x11/bg_image_x11.h"
#include "helpers/x11/drawing_x11.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <time.h>
#include <X11/extensions/render.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "deps/nanosvg.h"
#include "deps/stb_image.h"
#pragma GCC diagnostic pop
#include "config_values.h"
#include "helpers/x11/env_helpers_x11.h"
#include "core/types/window.h"
#include "helpers/helpers_ui.h"

#define BG_TILE_SLOTS 32
#define BG_REVALIDATE_MS 1000
#define BG_RETRY_MS 2000

typedef struct {
  Pixmap pm;
  Picture pic;
  int w;
  int h;
  char path[512];
  struct stat st;
  bool has_stat;
  unsigned long stamp;
  unsigned long checked_ms;
  bool failed;
} BgTileSlot;

static BgTileSlot bg_tiles[BG_TILE_SLOTS];
static unsigned long bg_tick = 0;
static pthread_mutex_t bg_mtx = PTHREAD_MUTEX_INITIALIZER;

static unsigned long bg_now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned long)(ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL);
}

static bool bg_has_ext(const char *path, const char *ext) {
  size_t pl = strlen(path), el = strlen(ext);
  if (pl < el)
    return false;
  for (size_t i = 0; i < el; i++) {
    char a = path[pl - el + i];
    if (a >= 'A' && a <= 'Z')
      a = (char)(a - 'A' + 'a');
    if (a != ext[i])
      return false;
  }
  return true;
}

static const char *bg_resolve(const char *in, char *out, size_t cap) {
  if (!in || !in[0] || !out || cap == 0)
    return NULL;
  if (in[0] != '~')
    return in;
  if (in[1] != 0 && in[1] != '/' && in[1] != '\\')
    return in;
  const char *home = x11_env_home_dir();
  if (!home || !home[0])
    home = getenv("USERPROFILE");
  if (!home || !home[0]) {
    const struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir && pw->pw_dir[0])
      home = pw->pw_dir;
  }
  if (!home || !home[0])
    return NULL;
  snprintf(out, cap, "%s%s", home, in[1] ? in + 1 : "");
  return out;
}

static bool bg_src_dims(const char *path, int *out_w, int *out_h) {
  if (bg_has_ext(path, ".svg")) {
    NSVGimage *img = nsvgParseFromFile(path, "px", 96.0f);
    if (!img)
      return false;
    if (img->width <= 0.0f || img->height <= 0.0f) {
      nsvgDelete(img);
      return false;
    }
    if (out_w)
      *out_w = (int)(img->width + 0.5f);
    if (out_h)
      *out_h = (int)(img->height + 0.5f);
    nsvgDelete(img);
    return true;
  }

  int pw = 0, ph = 0, pc = 0;
  if (!stbi_info(path, &pw, &ph, &pc))
    return false;
  if (pw <= 0 || ph <= 0)
    return false;
  if (out_w)
    *out_w = pw;
  if (out_h)
    *out_h = ph;
  return true;
}

static int bg_slot_empty(const void *s) {
  return ((const BgTileSlot *)s)->pm == 0;
}
static unsigned long bg_slot_stamp(const void *s) {
  return ((const BgTileSlot *)s)->stamp;
}

static void bg_slot_release(Display *dpy, BgTileSlot *t) {
  if (!t)
    return;
  if (t->pic) {
    XRenderFreePicture(dpy, t->pic);
    t->pic = 0;
  }
  if (t->pm) {
    XFreePixmap(dpy, t->pm);
    t->pm = 0;
  }
  t->w = 0;
  t->h = 0;
  t->has_stat = false;
  t->failed = false;
  t->path[0] = 0;
}

bool x11_bg_image_enabled(void) {
  const char *p = BackgroundImage;
  return p && p[0];
}

static bool bg_tile_compose(Display *dpy, const char *path, int w, int h,
                            Pixmap *out_pm, Picture *out_pic) {
  if (!dpy || !path || !path[0] || w <= 0 || h <= 0)
    return false;

  struct stat pst;
  if (stat(path, &pst) != 0) {
    return false;
  }

  int sw = 0, sh = 0;
  if (!bg_src_dims(path, &sw, &sh)) {
    return false;
  }
  if (sw <= 0 || sh <= 0)
    return false;

  int dw = 0, dh = 0;
  if (!cover_scale(sw, sh, w, h, &dw, &dh))
    return false;

  int cw = dw, ch = dh;
  unsigned char *full = x11_decode_image_argb(path, &cw, &ch);
  if (!full) {
    return false;
  }

  int oy = (h - dh) / 2;
  unsigned char *crop = (unsigned char *)malloc((size_t)w * (size_t)h * 4);
  if (!crop) {
    free(full);
    return false;
  }
  for (int dy = 0; dy < h; dy++) {
    int sy = dy - oy;
    if (sy < 0)
      sy = 0;
    if (sy >= ch)
      sy = ch - 1;
    memcpy(crop + (size_t)dy * (size_t)w * 4,
           full + (size_t)sy * (size_t)cw * 4, (size_t)w * 4);
  }
  free(full);

  Pixmap pm = 0;
  Picture pic = 0;
  bool ok = x11_upload_argb(dpy, DefaultRootWindow(dpy), crop, w, h, &pm, &pic);
  free(crop);
  if (!ok)
    return false;
  *out_pm = pm;
  *out_pic = pic;
  return true;
}

static bool bg_slot_valid(BgTileSlot *t, const char *path, int w, int h) {
  if (!t->pm || !t->pic)
    return false;
  if (t->w != w || t->h != h)
    return false;
  if (strcmp(t->path, path) != 0)
    return false;
  struct stat st;
  if (stat(path, &st) != 0)
    return true;
  if (!t->has_stat)
    return true;
  return t->st.st_mtime == st.st_mtime && t->st.st_size == st.st_size;
}

Pixmap x11_bg_image_tile(Display *dpy, int w, int h) {
  if (!x11_bg_image_enabled() || !dpy || w <= 0 || h <= 0)
    return 0;
  char rbuf[512];
  const char *path = bg_resolve(BackgroundImage, rbuf, sizeof(rbuf));
  if (!path)
    return 0;
  unsigned long now = bg_now_ms();

  pthread_mutex_lock(&bg_mtx);

  BgTileSlot *fresh = NULL;
  for (int i = 0; i < BG_TILE_SLOTS; i++) {
    BgTileSlot *t = &bg_tiles[i];
    if (t->failed && strcmp(t->path, path) == 0 && t->w == w && t->h == h &&
        now - t->checked_ms < BG_RETRY_MS) {
      pthread_mutex_unlock(&bg_mtx);
      return 0;
    }
    if (bg_slot_valid(t, path, w, h)) {
      if (now - t->checked_ms < BG_REVALIDATE_MS) {
        t->stamp = ++bg_tick;
        Pixmap pm = t->pm;
        pthread_mutex_unlock(&bg_mtx);
        return pm;
      }
      struct stat st;
      if (stat(path, &st) == 0 && t->has_stat &&
          t->st.st_mtime == st.st_mtime && t->st.st_size == st.st_size) {
        t->checked_ms = now;
        t->stamp = ++bg_tick;
        Pixmap pm = t->pm;
        pthread_mutex_unlock(&bg_mtx);
        return pm;
      }
    } else if (t->pm) {
      fresh = t;
      break;
    }
  }

  BgTileSlot *slot = fresh;
  if (!slot) {
    slot =
        &bg_tiles[bg_tile_lru_pick(bg_tiles, BG_TILE_SLOTS, sizeof(bg_tiles[0]),
                                   bg_slot_empty, bg_slot_stamp)];
  }

  bg_slot_release(dpy, slot);

  Pixmap pm = 0;
  Picture pic = 0;
  bool ok = bg_tile_compose(dpy, path, w, h, &pm, &pic);
  snprintf(slot->path, sizeof(slot->path), "%s", path);
  slot->w = w;
  slot->h = h;
  slot->checked_ms = now;
  slot->stamp = ++bg_tick;
  slot->has_stat = (stat(path, &slot->st) == 0);
  if (ok) {
    slot->pm = pm;
    slot->pic = pic;
    slot->failed = false;
  } else {
    slot->failed = true;
  }
  Pixmap result = ok ? pm : 0;
  pthread_mutex_unlock(&bg_mtx);
  return result;
}

bool x11_bg_image_blit(Display *dpy, Drawable dst, Visual *vis, int w, int h) {
  if (!x11_bg_image_enabled() || !dpy || !dst || w <= 0 || h <= 0)
    return false;
  Pixmap pm = x11_bg_image_tile(dpy, w, h);
  if (!pm)
    return false;

  pthread_mutex_lock(&bg_mtx);
  Picture src = 0;
  for (int i = 0; i < BG_TILE_SLOTS; i++) {
    if (bg_tiles[i].pm == pm) {
      src = bg_tiles[i].pic;
      break;
    }
  }
  bool ok = false;
  if (src) {
    if (!vis)
      vis = DefaultVisualOfScreen(DefaultScreenOfDisplay(dpy));
    XRenderPictFormat *dfmt = XRenderFindVisualFormat(dpy, vis);
    Picture dp = dfmt ? XRenderCreatePicture(dpy, dst, dfmt, 0, NULL) : 0;
    if (dp) {
      XRenderComposite(dpy, PictOpOver, src, None, dp, 0, 0, 0, 0, 0, 0, w, h);
      XRenderFreePicture(dpy, dp);
      ok = true;
    }
  }
  pthread_mutex_unlock(&bg_mtx);
  return ok;
}

int x11_bg_image_prewarm(Display *dpy, int w, int h) {
  if (!x11_bg_image_enabled() || !dpy || w <= 0 || h <= 0)
    return 0;
  return x11_bg_image_tile(dpy, w, h) ? 1 : 0;
}
