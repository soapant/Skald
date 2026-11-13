#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/widget_registry.h"
#include "deps/log.h"
#include "platform/platform.h"
#include "platform/wm_platform.h"
#include "helpers/helpers_monitor.h"
#include "platform/widget.h"
#include "helpers/helpers_owner.h"
#include "core/hover_widget.h"
#include "helpers/helpers_render_thread.h"
#include "platform/draw.h"
#include "helpers/helpers_ui.h"
#include "platform/lib.h"
#include "helpers/helpers_input_state.h"
#include "config_values.h"
#include "core/types/window.h"

static Widget *widgets[WIDGET_MAX];
static int widget_count = 0;
static int widget_seq = 0;
static unsigned widget_gen = 1;

static void *widget_mutex = NULL;
static void widget_mutex_init(void) {
  if (!widget_mutex)
    widget_mutex = Platform_MutexCreate();
}
#define W_LOCK()                                                               \
  do {                                                                         \
    widget_mutex_init();                                                       \
    Platform_MutexLock(widget_mutex);                                          \
  } while (0)
#define W_UNLOCK() Platform_MutexUnlock(widget_mutex)

static void widget_monitor_geometry(int monitor, int *x, int *y, int *w,
                                    int *h) {
  int count = Monitor_Count();
  int idx = (monitor > 0 && monitor <= count) ? monitor : 0;
  if (Monitor_GeometryAt(idx, x, y, w, h))
    return;
  Platform_GetMonitorGeometry(x, y, w, h);
}

unsigned Widget_BumpGeneration(void) { return ++widget_gen; }

unsigned Widget_CurrentGeneration(void) { return widget_gen; }

static Widget *widget_find_by_name_locked(const char *name) {
  if (!name || !name[0])
    return NULL;
  for (int i = 0; i < widget_count; i++) {
    if (widgets[i] && strcmp(widgets[i]->name, name) == 0)
      return widgets[i];
  }
  return NULL;
}

static void widget_relayout(void);

static PlPos hover_pos = {0, 0};
static PlTimer hover_at_timer = {0};
static bool hover_valid = false;

static void widget_hover_poll(void) {
  PlTimer now = Platform_StartTimer();
  if (hover_valid && Platform_EndTimer(hover_at_timer) < 400.0) {
    return;
  }
  int mx, my;
  Platform_GetMousePos(&mx, &my);
  hover_pos = (PlPos){mx, my};
  hover_at_timer = now;
  hover_valid = true;
}

typedef struct {
  Widget *w;
  int epoch;
} HoverJob;

static void *widget_hover_worker(void *ud) {
  HoverJob *job = (HoverJob *)ud;
  if (!job)
    return NULL;
  Widget *w = job->w;
  if (w && w->on_hover)
    w->on_hover();
  if (w) {
    W_LOCK();
    if (w->hover_epoch == job->epoch)
      w->hover_running = false;
    W_UNLOCK();
  }
  free(job);
  return NULL;
}

static void widget_render_frame(void *ud) {
  Widget *widget = (Widget *)ud;
  if (!widget)
    return;
  W_LOCK();
  bool dead = widget->dead;
  WidgetRenderFn render = dead ? NULL : widget->render;
  int x = widget->x, y = widget->y, w = widget->w, h = widget->h;
  int pad = widget->pad;
  bool auto_w = widget->auto_w, auto_h = widget->auto_h;
  void (*on_hover)(void) = widget->on_hover;
  bool hover_running = widget->hover_running;
  W_UNLOCK();
  if (dead || !render)
    return;
  widget->frame++;
  bool hoverable = (on_hover != NULL);
  if (hoverable) {
    widget_hover_poll();
    bool inside = (x >= 0x7fffffff)
                      ? false
                      : (hover_pos.x >= x && hover_pos.x < x + w &&
                         hover_pos.y >= y && hover_pos.y < y + h);
    if (inside && !widget->hovered && on_hover && !hover_running) {
      HoverJob *job = (HoverJob *)calloc(1, sizeof(*job));
      if (job) {
        W_LOCK();
        widget->hover_epoch++;
        job->w = widget;
        job->epoch = widget->hover_epoch;
        widget->hover_running = true;
        W_UNLOCK();
        widget->hover_thread = Platform_ThreadCreate(widget_hover_worker, job);
        if (!widget->hover_thread) {
          W_LOCK();
          widget->hover_running = false;
          W_UNLOCK();
          free(job);
        }
      }
    }
    widget->hovered = inside;
    if (!inside) {
      Platform_HoverMessageRelease(widget->name);
      HoverWidgetHide(widget->name);
    }
  }
  WidgetState st;
  st.width = w;
  st.height = h;
  st.frame = widget->frame;
  st.elapsed = Platform_EndTimer(widget->created_at);
  st.hovered = widget->hovered;

  SetDrawOffset(-widget->origin_x, 0);
  BeginMeasure();
  render(&st);
  int mw = 0, mh = 0, mx = 0, my = 0;
  EndMeasure(&mw, &mh);
  GetLastMeasureOrigin(&mx, &my);
  mx += widget->origin_x;

  if (widget->origin_x != mx || widget->origin_y != my) {
    widget->origin_x = mx;
    widget->origin_y = my;
    Widget_Invalidate(widget->win);
  }

  if (auto_w || auto_h) {
    int nw = (auto_w && mw > 0) ? mw + pad : w;
    int nh = (auto_h && mh > 0) ? mh + pad : h;

    if (nw != w || nh != h) {
      W_LOCK();
      widget->w = nw;
      widget->h = nh;
      W_UNLOCK();
      if (Widget_IsValid(widget->win)) {
        Platform_Widget_Resize(widget->win, nw, nh);
      }
      widget_relayout();
    }
  }
}

static void widget_owner_path(void *addr, char *out, size_t n) {
  Platform_GetModuleOwnerPath(addr, out, n);
}

static bool widget_owner_matches(const Widget *widget, const char *a,
                                 const char *b) {
  if (!widget->owner)
    return false;
  if (a && a[0] && strcmp(widget->owner, a) == 0)
    return true;
  if (b && b[0] && strcmp(widget->owner, b) == 0)
    return true;
  char ab[512], bb[512], wb[512];
  Owner_ModuleBasename(widget->owner, wb, sizeof(wb));
  if (a && a[0]) {
    Owner_ModuleBasename(a, ab, sizeof(ab));
    if (strcmp(wb, ab) == 0)
      return true;
  }
  if (b && b[0]) {
    Owner_ModuleBasename(b, bb, sizeof(bb));
    if (strcmp(wb, bb) == 0)
      return true;
  }
  return false;
}

static void widget_destroy_window(void *ud) {
  Widget *widget = (Widget *)ud;
  if (!widget)
    return;
  widget->hovered = false;
  Platform_HoverMessageRelease(widget->name);
  HoverWidgetHide(widget->name);
  if (widget->hover_thread) {
    Platform_ThreadJoin(widget->hover_thread);
    widget->hover_thread = NULL;
    widget->hover_running = false;
  }
  if (Widget_IsValid(widget->win)) {
    Platform_Widget_Destroy(widget->win);
  }
  widget->win = NULL;
}

static void widget_reap(void *ud) {
  Widget *w = (Widget *)ud;
  if (!w)
    return;
  int n = 0;
  W_LOCK();
  for (int i = 0; i < widget_count; i++) {
    if (widgets[i] == w) {
      if (w->owner)
        free(w->owner);
      w->owner = NULL;
      continue;
    }
    widgets[n++] = widgets[i];
  }
  widget_count = n;
  W_UNLOCK();
  free(w);
}

void Widget_Destroy(Widget *widget) {
  if (!widget)
    return;
  widget->dead = true;
  Platform_HoverMessageRelease(widget->name);
  HoverWidgetHide(widget->name);
  RenderThread_RemoveFrameCallback(widget_render_frame, widget);
  bool posted = RenderThread_Post(widget_destroy_window, widget) &&
                RenderThread_Post(widget_reap, widget);
  RenderThread_Sync();

  if (!posted) {
    int n = 0;
    W_LOCK();
    for (int i = 0; i < widget_count; i++) {
      if (widgets[i] == widget) {
        if (widget->owner)
          free(widget->owner);
        widget->owner = NULL;
        continue;
      }
      widgets[n++] = widgets[i];
    }
    widget_count = n;
    W_UNLOCK();
  }
  if (widget_count > 0)
    widget_relayout();
  Platform_Widget_FlushDestruction();
}

void Widget_PurgeOwner(const char *path_a, const char *path_b,
                       unsigned generation) {
  if (Owner_PurgeInactive(path_a, path_b))
    return;

  bool matched = false;
  W_LOCK();
  for (int i = 0; i < widget_count; i++) {
    Widget *w = widgets[i];
    if (!w)
      continue;
    if (!widget_owner_matches(w, path_a, path_b))
      continue;
    if (w->gen > generation)
      continue;
    w->dead = true;
    matched = true;
    HoverWidgetHide(w->name);
    RenderThread_RemoveFrameCallback(widget_render_frame, w);
    if (!RenderThread_Post(widget_destroy_window, w)) {
      log_error("Widget_PurgeOwner: failed to enqueue destroy for '%s'",
                w->name ? w->name : "?");
    }
    if (!RenderThread_Post(widget_reap, w)) {
      log_error("Widget_PurgeOwner: failed to enqueue reap for '%s'",
                w->name ? w->name : "?");
    }
  }
  W_UNLOCK();

  if (!matched)
    return;

  RenderThread_Sync();

  int n = 0;
  W_LOCK();
  for (int i = 0; i < widget_count; i++) {
    Widget *w = widgets[i];
    if (!w)
      continue;
    if (widget_owner_matches(w, path_a, path_b) && w->gen <= generation) {
      continue;
    }
    widgets[n++] = w;
  }
  widget_count = n;
  W_UNLOCK();
  if (widget_count > 0)
    widget_relayout();
  Platform_Widget_FlushDestruction();
}

bool Widget_OwnerActive(const char *path) {
  if (!path || !path[0])
    return false;
  W_LOCK();
  for (int i = 0; i < widget_count; i++) {
    Widget *w = widgets[i];
    if (w && widget_owner_matches(w, path, NULL)) {
      W_UNLOCK();
      return true;
    }
  }
  W_UNLOCK();
  return false;
}

static void widget_attach_window(void *ud) {
  Widget *widget = (Widget *)ud;
  if (!widget || widget->dead)
    return;
  bool created = false;
  if (!Widget_IsValid(widget->win)) {
    SetDrawBgColor(widget->has_bg ? widget->bg_color : 0);
    widget->win = Platform_Widget_Create(widget->x, widget->y, widget->w,
                                         widget->h, widget->fps, widget->no_bg,
                                         widget->on_click, widget->name);
    SetDrawBgColor(0);
    created = (widget->win != NULL);
  } else {
    Platform_Widget_Move(widget->win, widget->x, widget->y);
  }
  if (widget->win) {
    Platform_Widget_SetBgColor(widget->win,
                               widget->has_bg ? widget->bg_color : 0);
    Platform_Widget_SetNoBg(widget->win, widget->no_bg);
    Widget_SetFps(widget->win, widget->fps);
    if (created)
      Widget_SetFrameCallback(widget->win, widget_render_frame, widget);
  }
}

static void widget_apply_geometry(Widget *widget, int x, int y) {
  W_LOCK();
  widget->x = x;
  widget->y = y;
  W_UNLOCK();
  if (RenderThread_IsCurrentThread()) {
    widget_attach_window(widget);
    return;
  }
  if (!RenderThread_EnsureStarted(widget->fps))
    return;
  if (!RenderThread_Post(widget_attach_window, widget))
    return;
}

static void widget_relayout(void) {
  typedef struct {
    Widget *widget;
    int w, h;
    int slot;
    int monitor;
    int gap;
    int key;
  } LayoutItem;
  LayoutItem items[WIDGET_MAX];
  int n = 0;
  W_LOCK();
  for (int i = 0; i < widget_count; i++) {
    Widget *w = widgets[i];
    if (w && !w->free_pos) {
      LayoutItem *it = &items[n++];
      it->widget = w;
      it->w = w->w;
      it->h = w->h;
      it->slot = w->slot;
      it->monitor = w->monitor;
      it->gap = w->gap_override;
      it->key = w->order_set ? w->order : w->seq;
    }
  }
  W_UNLOCK();
  for (int i = 1; i < n; i++) {
    LayoutItem key = items[i];
    int k = i - 1;
    while (k >= 0 && items[k].key > key.key) {
      items[k + 1] = items[k];
      k--;
    }
    items[k + 1] = key;
  }

  typedef struct {
    int x, y, w, h;
    int cursor;
    bool known;
  } MonSlot;
  MonSlot ms[WIDGET_SLOT_COUNT][WIDGET_MAX_MONITORS] = {{{0}}};

  for (int i = 0; i < n; i++) {
    LayoutItem *it = &items[i];
    if (it->slot < 0 || it->slot >= WIDGET_SLOT_COUNT)
      continue;
    bool from_right = (it->slot == WIDGET_SLOT_TOP_RIGHT ||
                       it->slot == WIDGET_SLOT_BOTTOM_RIGHT);
    bool from_top =
        (it->slot == WIDGET_SLOT_TOP_LEFT || it->slot == WIDGET_SLOT_TOP_RIGHT);
    int m = (it->monitor > 0 && it->monitor < WIDGET_MAX_MONITORS) ? it->monitor
                                                                   : 0;
    MonSlot *st = &ms[it->slot][m];
    if (!st->known) {
      if (!Monitor_GeometryAt(m, &st->x, &st->y, &st->w, &st->h))
        Platform_GetMonitorGeometry(&st->x, &st->y, &st->w, &st->h);
      st->cursor = from_right ? st->x + st->w : st->x;
      st->known = true;
    }
    int g = it->gap > 0 ? it->gap : WidgetGap;
    int px = from_right ? st->cursor - it->w : st->cursor;
    int py = from_top ? st->y : st->y + st->h - it->h;
    widget_apply_geometry(it->widget, px, py);
    st->cursor = from_right ? px - g : px + it->w + g;
  }
}

Widget *Widget_Create(const WidgetSpec *spec) {
  if (!spec || !spec->name || !spec->name[0]) {
    log_warn("Widget_Create: name is required");
    return NULL;
  }
  W_LOCK();
  Widget *widget = widget_find_by_name_locked(spec->name);
  bool fresh = (widget == NULL);
  if (fresh) {
    if (widget_count >= WIDGET_MAX) {
      W_UNLOCK();
      log_warn("Widget_Create: widget table full");
      return NULL;
    }
    widget = (Widget *)calloc(1, sizeof(*widget));
    if (widget) {
      snprintf(widget->name, WIDGET_NAME_MAX, "%s", spec->name);
      widget->seq = widget_seq++;
      widgets[widget_count++] = widget;
    }
  }
  if (!widget) {
    W_UNLOCK();
    return NULL;
  }

  {
    char owner_path[512];
    widget_owner_path(__builtin_return_address(0), owner_path,
                      sizeof(owner_path));
    if (owner_path[0]) {
      if (widget->owner)
        free(widget->owner);
      widget->owner = strdup(owner_path);
    }
  }

  widget->gen = widget_gen;
  widget->has_bg = spec->color != 0;
  widget->bg_color = widget->has_bg ? spec->color : 0;
  widget->no_bg = spec->no_bg;
  widget->auto_w = spec->w <= 0;
  widget->auto_h = spec->h <= 0;
  widget->pad = spec->pad > 0 ? spec->pad : WIDGET_DEFAULT_PAD;
  if (fresh || !widget->auto_w)
    widget->w = widget->auto_w ? WIDGET_PROVISIONAL_W : spec->w;
  if (fresh || !widget->auto_h)
    widget->h = widget->auto_h ? WIDGET_PROVISIONAL_H : spec->h;
  widget->gap_override = spec->gap > 0 ? spec->gap : 0;
  widget->fps = spec->fps ? spec->fps : 1;
  widget->slot = (spec->slot >= WIDGET_SLOT_TOP_LEFT &&
                  spec->slot <= WIDGET_SLOT_BOTTOM_RIGHT)
                     ? spec->slot
                     : WIDGET_SLOT_BOTTOM_LEFT;
  widget->monitor = spec->monitor > 0 ? spec->monitor : 0;
  widget->order = spec->order;
  widget->order_set = spec->order > 0;
  widget->render = spec->render;
  widget->on_click = spec->on_click;
  widget->on_hover = spec->on_hover;
  widget->free_pos = (spec->slot == WIDGET_FREE);
  widget->frame = 0;
  widget->created_at = Platform_StartTimer();
  W_UNLOCK();

  if (widget->free_pos) {
    int x = spec->x;
    int y = spec->y;
    if (x < 0 && y < 0) {
      int mx = 0, my = 0, mw = 1920, mh = 1080;
      widget_monitor_geometry(widget->monitor, &mx, &my, &mw, &mh);
      x = mx + (mw - widget->w) / 2;
      y = my + (mh - widget->h) / 2;
    }
    widget_apply_geometry(widget, x, y);
    Widget_SetFrameCallback(widget->win, widget_render_frame, widget);
  } else {
    widget_relayout();
    Widget_SetFrameCallback(widget->win, widget_render_frame, widget);
  }
  return widget;
}

bool Widget_Valid(Widget *widget) { return widget != NULL; }

PlPos Widget_GetPos(const char *name) {
  PlPos r = {0, 0};
  W_LOCK();
  Widget *w = widget_find_by_name_locked(name);
  if (w && w->x < 0x7fffffff) {
    r.x = w->x + w->w / 2;
    r.y = w->y + w->h / 2;
  }
  W_UNLOCK();
  return r;
}

static void widget_apply_show(void *ud) {
  Widget *widget = (Widget *)ud;
  if (widget)
    Platform_Widget_Show(widget->win);
}

static void widget_apply_hide(void *ud) {
  Widget *widget = (Widget *)ud;
  if (widget)
    Platform_Widget_Hide(widget->win);
}

void Widget_Show(Widget *widget) {
  if (!Widget_Valid(widget))
    return;
  if (widget->mapped)
    return;
  widget->mapped = true;
  RenderThread_Post(widget_apply_show, widget);
}

void Widget_Hide(Widget *widget) {
  if (!Widget_Valid(widget))
    return;
  if (!widget->mapped)
    return;
  widget->mapped = false;
  RenderThread_Post(widget_apply_hide, widget);
}

void Widget_SetRender(Widget *widget, WidgetRenderFn render) {
  if (!Widget_Valid(widget))
    return;
  widget->render = render;
  if (Widget_IsValid(widget->win)) {
    Widget_Invalidate(widget->win);
  }
}

static bool Widget_ApplyFps(PlatformWidget *widget, int fps) {
  PlatformWidgetBase *base = (PlatformWidgetBase *)widget;
  if (!widget || base->dead)
    return false;
  if (base->fps == fps)
    return false;
  base->fps = fps;
  base->last_draw_ms = 0;
  base->dirty = true;
  return true;
}

void Widget_SetFps(PlatformWidget *window, int fps) {
  if (fps <= 0)
    fps = 1;
  if (!Widget_ApplyFps(window, fps))
    return;
  RenderThread_EnsureStarted(fps);
  RenderThread_Wake();
}

bool Widget_IsValid(const PlatformWidget *widget) {
  const PlatformWidgetBase *base = (const PlatformWidgetBase *)widget;
  return widget && !base->dead;
}

void Widget_SetFrameCallback(PlatformWidget *widget, RenderFrameFn fn,
                             void *ud) {
  PlatformWidgetBase *base = (PlatformWidgetBase *)widget;
  if (!widget || base->dead) {
    return;
  }
  Platform_RenderThreadLock();
  base->fn = fn;
  base->ud = ud;
  Platform_RenderThreadUnlock();
}

void Widget_Invalidate(PlatformWidget *widget) {
  PlatformWidgetBase *base = (PlatformWidgetBase *)widget;
  if (!widget || base->dead) {
    return;
  }
  base->dirty = true;
  RenderThread_Wake();
}

void Widget_InitBase(PlatformWidgetBase *base, int x, int y, int w, int h,
                     int fps, bool no_bg, void (*on_click)(void),
                     const char *name) {
  if (!base)
    return;
  base->x = x;
  base->y = y;
  base->w = w;
  base->h = h;
  base->fps = fps;
  base->no_bg = no_bg;
  base->on_click = on_click;
  snprintf(base->name, sizeof(base->name), "%s", name ? name : "?");
  base->dirty = true;
  base->last_draw_ms = 0;
}

bool Widget_BaseCheck(PlatformWidgetBase *base) { return base && !base->dead; }

bool Widget_BaseMove(PlatformWidgetBase *base, int x, int y) {
  if (!base || base->dead)
    return false;
  if (base->x == x && base->y == y)
    return false;
  base->x = x;
  base->y = y;
  return true;
}

bool Widget_BaseResize(PlatformWidgetBase *base, int w, int h) {
  if (!base || base->dead)
    return false;
  if (w <= 0 || h <= 0)
    return false;
  if (base->w == w && base->h == h)
    return false;
  base->w = w;
  base->h = h;
  return true;
}

bool Widget_BaseSetNoBg(PlatformWidgetBase *base, bool no_bg) {
  if (!base || base->dead)
    return false;
  base->no_bg = no_bg;
  return true;
}

bool Widget_BaseSetBgColor(PlatformWidgetBase *base, ColorRef bg_color) {
  if (!base || base->dead)
    return false;
  base->has_bg = bg_color != 0;
  base->bg_color = bg_color;
  return true;
}
