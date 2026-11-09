#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/types/window.h"
#include "platform/platform.h"
#include "platform/widget.h"
#include "platform/render_thread.h"
#include "platform/draw.h"

#define HOVER_WIDGET_MAX 8

typedef struct {
  char key[64];
  PlatformWidget *w;
  void (*render)(void *ud);
  void *ud;
  void (*release)(void *ud);
  void *release_ud;
  int fps;
  bool no_bg;
  unsigned long bg_hex;
  int x, y, w_px, h_px;
  bool dead;
} HoverWidgetSlot;

static HoverWidgetSlot g_hover_widgets[HOVER_WIDGET_MAX];

static int HoverWidgetFind(const char *key) {
  for (int i = 0; i < HOVER_WIDGET_MAX; i++) {
    if (g_hover_widgets[i].w && !g_hover_widgets[i].dead &&
        strcmp(g_hover_widgets[i].key, key) == 0)
      return i;
  }
  return -1;
}

static void HoverWidgetFrame(void *ud) {
  HoverWidgetSlot *s = (HoverWidgetSlot *)ud;
  if (!s || !s->w || s->dead) {
    return;
  }
  if (s->render)
    s->render(s->ud);
}

typedef struct {
  char key[64];
  void (*render)(void *ud);
  void *ud;
  int x, y, w, h, fps;
  bool no_bg;
  unsigned long bg_hex;
  void (*release)(void *ud);
  void *release_ud;
  int op;
} HoverWidgetRequest;

#define HOVER_OP_SHOW 1
#define HOVER_OP_HIDE 2

static void hover_widget_apply(const char *key, void (*render)(void *ud),
                               void *ud, int x, int y, int w, int h, int fps,
                               bool no_bg, unsigned long bg_hex,
                               void (*release)(void *ud), void *release_ud) {
  int idx = HoverWidgetFind(key);
  if (idx >= 0) {
    HoverWidgetSlot *s = &g_hover_widgets[idx];
    void (*old_release)(void *) = s->release;
    void *old_release_ud = s->release_ud;
    s->render = render;
    s->ud = ud;
    s->release = release;
    s->release_ud = release_ud;
    bool geom_changed =
        (s->x != x || s->y != y || s->w_px != w || s->h_px != h);
    s->x = x;
    s->y = y;
    s->w_px = w;
    s->h_px = h;
    PlatformWidget *pw = s->w;
    int ax = x - w / 2;
    int ay = y - h / 2;
    if (geom_changed) {
      Platform_Widget_Resize(pw, w, h);
      Platform_Widget_Move(pw, ax, ay);
    }
    if (s->bg_hex != bg_hex || s->no_bg != no_bg) {
      s->bg_hex = bg_hex;
      s->no_bg = no_bg;
      Platform_Widget_SetBgColor(pw, no_bg ? 0 : bg_hex);
      Platform_Widget_SetNoBg(pw, no_bg);
    }
    Platform_Widget_Invalidate(pw);
    if (old_release && old_release != release)
      old_release(old_release_ud);
    return;
  }
  idx = -1;
  for (int i = 0; i < HOVER_WIDGET_MAX; i++) {
    if (!g_hover_widgets[i].w) {
      idx = i;
      break;
    }
  }
  if (idx < 0) {
    if (release)
      release(release_ud);
    return;
  }
  HoverWidgetSlot *s = &g_hover_widgets[idx];
  memset(s, 0, sizeof(*s));
  snprintf(s->key, sizeof(s->key), "%s", key);
  s->render = render;
  s->ud = ud;
  s->release = release;
  s->release_ud = release_ud;
  s->fps = fps;
  s->no_bg = no_bg;
  s->bg_hex = bg_hex;
  s->x = x;
  s->y = y;
  s->w_px = w;
  s->h_px = h;

  PlatformWidget *pw =
      Platform_Widget_Create(x - w / 2, y - h / 2, w, h, fps, no_bg, NULL, key);
  if (!pw) {
    memset(s, 0, sizeof(*s));
    if (release)
      release(release_ud);
    return;
  }
  s->w = pw;
  Platform_Widget_SetBgColor(pw, no_bg ? 0 : bg_hex);
  Platform_Widget_SetNoBg(pw, no_bg);
  Platform_Widget_SetFrameCallback(pw, HoverWidgetFrame, s);
}

static void hover_widget_hide_apply(const char *key) {
  int idx = HoverWidgetFind(key);
  if (idx < 0) {
    return;
  }
  PlatformWidget *w = g_hover_widgets[idx].w;
  void (*release)(void *) = g_hover_widgets[idx].release;
  void *release_ud = g_hover_widgets[idx].release_ud;
  memset(&g_hover_widgets[idx], 0, sizeof(g_hover_widgets[idx]));
  if (w) {
    Platform_Widget_SetFrameCallback(w, NULL, NULL);
    Platform_Widget_Destroy(w);
    Platform_Widget_FlushDestruction();
  }
  if (release)
    release(release_ud);
}

static void hover_widget_dispatch(void *ud) {
  HoverWidgetRequest *r = (HoverWidgetRequest *)ud;
  if (!r) {
    return;
  }
  if (r->op == HOVER_OP_HIDE) {
    hover_widget_hide_apply(r->key);
  } else {
    hover_widget_apply(r->key, r->render, r->ud, r->x, r->y, r->w, r->h,
                       r->fps, r->no_bg, r->bg_hex, r->release, r->release_ud);
  }
  free(r);
}

static bool hover_widget_post(int op, const char *key, void (*render)(void *ud),
                              void *ud, int x, int y, int w, int h, int fps,
                              bool no_bg, unsigned long bg_hex,
                              void (*release)(void *ud), void *release_ud) {
  if (Platform_RenderThread_IsCurrentThread()) {
    if (op == HOVER_OP_HIDE)
      hover_widget_hide_apply(key);
    else
      hover_widget_apply(key, render, ud, x, y, w, h, fps, no_bg, bg_hex,
                         release, release_ud);
    return true;
  }
  if (!Platform_RenderThread_IsRunning())
    return false;
  HoverWidgetRequest *r = (HoverWidgetRequest *)calloc(1, sizeof(*r));
  if (!r)
    return false;
  snprintf(r->key, sizeof(r->key), "%s", key);
  r->op = op;
  r->render = render;
  r->ud = ud;
  r->x = x;
  r->y = y;
  r->w = w;
  r->h = h;
  r->fps = fps;
  r->no_bg = no_bg;
  r->bg_hex = bg_hex;
  r->release = release;
  r->release_ud = release_ud;
  if (Platform_RenderThread_Post(hover_widget_dispatch, r))
    return true;
  free(r);
  return false;
}

void Platform_HoverWidgetHide(const char *key) {
  if (!key || !key[0]) {
    return;
  }
  if (HoverWidgetFind(key) < 0) {
    return;
  }
  if (!hover_widget_post(HOVER_OP_HIDE, key, NULL, NULL, 0, 0, 0, 0, 0, false,
                         0, NULL, NULL)) {
    hover_widget_hide_apply(key);
  }
}

void Platform_HoverWidgetShow(PlHoverWidgetShowSpec spec) {
  const char *key = spec.key;
  void (*render)(void *ud) = spec.render;
  void *ud = spec.ud;
  int x = spec.x;
  int y = spec.y;
  int w = spec.w;
  int h = spec.h;
  int fps = spec.fps;
  bool no_bg = spec.no_bg;
  unsigned long bg_hex = spec.bg_hex;
  void (*release)(void *ud) = spec.release;
  void *release_ud = spec.release_ud;

  if (!key || !key[0] || w <= 0 || h <= 0) {
    return;
  }
  if (!hover_widget_post(HOVER_OP_SHOW, key, render, ud, x, y, w, h, fps,
                         no_bg, bg_hex, release, release_ud)) {
    if (release)
      release(release_ud);
  }
}
