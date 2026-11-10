#include <pthread.h>
#include "helpers/helpers_fs.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include "deps/log.h"
#include "helpers/helpers_input_state.h"
#include "config_values.h"
#include "core/types.h"
#include "core/types/window.h"
#include <X11/extensions/Xrandr.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <glob.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <sys/file.h>
#include <fontconfig/fontconfig.h>
#include <curl/curl.h>
#include "helpers/x11/display_x11.h"
#include "platform/platform.h"
#include "helpers/x11/misc_x11.h"
#include "core/context.h"
#include "helpers/helpers_owner.h"
#include "core/thread_registry.h"
#include "core/prompt_geometry.h"
#include "helpers/helpers_text.h"
#include "core/feature_path.h"
#include "helpers/x11/env_helpers_x11.h"
#include "wm/wm.h"
#include <X11/extensions/XTest.h>
#include "helpers/x11/clipboard_x11.h"
#include "platform/x11/clipboard_backend_x11.h"
#include "helpers/x11/platform_x11.h"

typedef struct {
  Display *dpy;
  int size;
  char *name;
  XftFont *xft;
  unsigned long last_used;
} X11FontSlot;

#define X11_FONT_CACHE_SLOTS 8

X11FontSlot g_font_slots[X11_FONT_CACHE_SLOTS];
unsigned long g_font_tick = 0;
pthread_mutex_t g_font_cache_mtx = PTHREAD_MUTEX_INITIALIZER;
#define X11_FONT_RETIRED 16
Display *g_font_retired_dpy[X11_FONT_RETIRED];
XftFont *g_font_retired_xft[X11_FONT_RETIRED];
int g_font_retired_next = 0;

static void x11_font_retire(Display *dpy, XftFont *xft) {

  g_font_retired_dpy[g_font_retired_next] = dpy;
  g_font_retired_xft[g_font_retired_next] = xft;
  g_font_retired_next = (g_font_retired_next + 1) % X11_FONT_RETIRED;
}

bool x11_font_was_retired(Display *dpy, XftFont *xft) {

  for (int i = 0; i < X11_FONT_RETIRED; i++) {
    if (g_font_retired_xft[i] == xft && g_font_retired_dpy[i] == dpy)
      return true;
  }
  return false;
}

static bool x11_font_slot_matches(const X11FontSlot *s, Display *dpy, int size,
                                  const char *face_name) {

  if (!s->xft || s->dpy != dpy || s->size != size)
    return false;
  if (!s->name || !face_name)
    return s->name == face_name;
  return strcmp(s->name, face_name) == 0;
}

static void x11_font_slot_clear(X11FontSlot *s) {

  if (s->xft) {
    XftFontClose(s->dpy, s->xft);
    x11_font_retire(s->dpy, s->xft);
  }
  if (s->name)
    free(s->name);
  s->dpy = NULL;
  s->size = 0;
  s->name = NULL;
  s->xft = NULL;
  s->last_used = 0;
}

bool x11_font_is_cached(Display *dpy, XftFont *xft) {

  for (int i = 0; i < X11_FONT_CACHE_SLOTS; i++) {
    if (g_font_slots[i].xft == xft && g_font_slots[i].dpy == dpy)
      return true;
  }
  return false;
}

FontHandle x11_create_font_on_display(Display *dpy, int size,
                                      const char *face_name) {

  if (!dpy)
    return FH_NULL;

  pthread_mutex_lock(&g_font_cache_mtx);

  for (int i = 0; i < X11_FONT_CACHE_SLOTS; i++) {
    if (x11_font_slot_matches(&g_font_slots[i], dpy, size, face_name)) {
      g_font_slots[i].last_used = ++g_font_tick;
      FontHandle hit = {.xfont = g_font_slots[i].xft, .dpy = dpy};
      pthread_mutex_unlock(&g_font_cache_mtx);
      return hit;
    }
  }

  int screen = DefaultScreen(dpy);
  XLockDisplay(dpy);

  XftFont *xf = XftFontOpen(dpy, screen, FC_FAMILY, FcTypeString, face_name,
                            FC_PIXEL_SIZE, FcTypeDouble, (double)size, NULL);
  if (!xf) {
    xf = XftFontOpen(dpy, screen, FC_FAMILY, FcTypeString, "sans",
                     FC_PIXEL_SIZE, FcTypeDouble, (double)size, NULL);
  }
  if (!xf) {
    xf = XftFontOpenName(dpy, screen, "fixed");
  }

  XUnlockDisplay(dpy);

  if (!xf) {
    pthread_mutex_unlock(&g_font_cache_mtx);
    return FH_NULL;
  }

  int slot = -1;
  unsigned long oldest = ~0UL;
  for (int i = 0; i < X11_FONT_CACHE_SLOTS; i++) {
    if (!g_font_slots[i].xft) {
      slot = i;
      break;
    }
    if (g_font_slots[i].last_used < oldest) {
      oldest = g_font_slots[i].last_used;
      slot = i;
    }
  }
  if (slot < 0) {
    XftFontClose(dpy, xf);
    pthread_mutex_unlock(&g_font_cache_mtx);
    return FH_NULL;
  }
  x11_font_slot_clear(&g_font_slots[slot]);

  g_font_slots[slot].dpy = dpy;
  g_font_slots[slot].size = size;
  g_font_slots[slot].name = face_name ? strdup(face_name) : NULL;
  g_font_slots[slot].xft = xf;
  g_font_slots[slot].last_used = ++g_font_tick;

  FontHandle made = {.xfont = xf, .dpy = dpy};
  pthread_mutex_unlock(&g_font_cache_mtx);
  return made;
}

void x11_cleanup_per_feature_edit_map(void) {}

unsigned char x11_mask_extract(unsigned long pixel, unsigned long mask) {

  if (!mask)
    return 0;
  int shift = 0;
  while (!(mask & 1ul)) {
    mask >>= 1;
    shift++;
  }
  unsigned long bits = 0;
  for (unsigned long m = mask; m; m >>= 1)
    bits++;
  unsigned long v = (pixel >> shift) & ((1ul << bits) - 1ul);
  if (bits == 0)
    return 0;
  if (bits >= 8)
    return (unsigned char)(v >> (bits - 8));
  return (unsigned char)((v * 255ul) / ((1ul << bits) - 1ul));
}

int x11_button(int button) {
  switch (button) {
  case 1:
    return 1;
  case 2:
    return 3;
  case 3:
    return 2;
  case 4:
    return 8;
  case 5:
    return 9;
  case 6:
    return 8;
  case 7:
    return 9;
  default:
    return 0;
  }
}
