#include "core/widget_registry.h"
#include "core/flapi.h"
#include "deps/log.h"
#include "platform/platform.h"
#include "platform/ui.h"
#include "platform/wm_platform.h"
#include "platform/widget.h"
#include "platform/render_thread.h"
#include "platform/draw.h"
#include "platform/lib.h"
#include "platform/input_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FlWidget *g_widgets[FL_WIDGET_MAX];
static int g_widget_count = 0;
static int g_widget_seq = 0;
static unsigned g_widget_gen = 1;

static FlTimer mon_at_timer;
static bool mon_valid = false;
static int mon_x, mon_y, mon_w, mon_h;

static void widget_monitor_geometry(int *x, int *y, int *w, int *h) {
  if (!mon_valid || Platform_EndTimer(mon_at_timer) > 2000) {
    Platform_WM_GetMonitorGeometry(&mon_x, &mon_y, &mon_w, &mon_h);
    mon_at_timer = Platform_StartTimer();
    mon_valid = true;
  }
  *x = mon_x;
  *y = mon_y;
  *w = mon_w;
  *h = mon_h;
}

unsigned widget_bump_generation(void) { return ++g_widget_gen; }

unsigned widget_current_generation(void) { return g_widget_gen; }

static FlWidget *widget_find_by_name(const char *name) {
  if (!name || !name[0])
    return NULL;
  for (int i = 0; i < g_widget_count; i++) {
    if (g_widgets[i] && strcmp(g_widgets[i]->name, name) == 0)
      return g_widgets[i];
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
  hover_pos = (FlPos){mx, my};
  hover_at_timer = now;
  hover_valid = true;
}

static void *widget_hover_worker(void *ud) {
  FlWidget *w = (FlWidget *)ud;
  if (w && w->on_hover)
    w->on_hover();
  if (w)
    w->hover_running = false;
  return NULL;
}

static void widget_render_frame(void *ud) {
  FlWidget *widget = (FlWidget *)ud;
  if (!widget || !widget->render || widget->dead)
    return;
  widget->frame++;
  bool hoverable = (widget->on_hover != NULL);
  if (hoverable) {
    widget_hover_poll();
    bool inside =
        (widget->x >= 0x7fffffff)
            ? false
            : (hover_pos.x >= widget->x &&
               hover_pos.x < widget->x + widget->w &&
               hover_pos.y >= widget->y && hover_pos.y < widget->y + widget->h);
    if (inside && !widget->hovered && widget->on_hover &&
        !widget->hover_running) {
      widget->hover_running = true;
      widget->hover_thread = Platform_ThreadCreate(widget_hover_worker, widget);
      if (!widget->hover_thread)
        widget->hover_running = false;
    }
    widget->hovered = inside;
    if (!inside) {
      Platform_HoverMessageRelease(widget->name);
      Platform_HoverWidgetHide(widget->name);
    }
  }
  FlWidgetState st;
  st.width = widget->w;
  st.height = widget->h;
  st.frame = widget->frame;
  st.elapsed = Platform_EndTimer(widget->created_at);
  st.hovered = widget->hovered;

  Platform_SetDrawOffset(-widget->origin_x, 0);
  Platform_BeginMeasure();
  widget->render(&st);
  int mw = 0, mh = 0, mx = 0, my = 0;
  Platform_EndMeasure(&mw, &mh);
  Platform_GetLastMeasureOrigin(&mx, &my);
  mx += widget->origin_x;

  if (widget->origin_x != mx || widget->origin_y != my) {
    widget->origin_x = mx;
    widget->origin_y = my;
    Platform_Widget_Invalidate(widget->win);
  }

  if (widget->auto_w || widget->auto_h) {
    int nw = (widget->auto_w && mw > 0) ? mw + widget->pad : widget->w;
    int nh = (widget->auto_h && mh > 0) ? mh + widget->pad : widget->h;

    if (nw != widget->w || nh != widget->h) {
      widget->w = nw;
      widget->h = nh;
      if (Platform_Widget_IsValid(widget->win)) {
        Platform_Widget_Resize(widget->win, nw, nh);
      }
      widget_relayout();
    }
  }
}

static void widget_owner_path(void *addr, char *out, size_t n) {
  Platform_GetModuleOwnerPath(addr, out, n);
}

static const char *widget_owner_basename(const char *p, char *out, size_t n) {
  if (!p || !p[0]) {
    if (n)
      out[0] = '\0';
    return out;
  }
  const char *s = p;
  for (const char *q = p; *q; q++)
    if (*q == '/' || *q == '\\')
      s = q + 1;
  snprintf(out, n, "%s", s);
  return out;
}

static bool widget_owner_matches(const FlWidget *widget, const char *a,
                                 const char *b) {
  if (!widget->owner)
    return false;
  if (a && a[0] && strcmp(widget->owner, a) == 0)
    return true;
  if (b && b[0] && strcmp(widget->owner, b) == 0)
    return true;
  char ab[512], bb[512], wb[512];
  widget_owner_basename(widget->owner, wb, sizeof(wb));
  if (a && a[0]) {
    widget_owner_basename(a, ab, sizeof(ab));
    if (strcmp(wb, ab) == 0)
      return true;
  }
  if (b && b[0]) {
    widget_owner_basename(b, bb, sizeof(bb));
    if (strcmp(wb, bb) == 0)
      return true;
  }
  return false;
}

static void widget_destroy_window(void *ud) {
  FlWidget *widget = (FlWidget *)ud;
  if (!widget)
    return;
  widget->hovered = false;
  Platform_HoverMessageRelease(widget->name);
  Platform_HoverWidgetHide(widget->name);
  if (widget->hover_thread) {
    Platform_ThreadJoin(widget->hover_thread);
    widget->hover_thread = NULL;
    widget->hover_running = false;
  }
  if (Platform_Widget_IsValid(widget->win)) {
    Platform_Widget_Destroy(widget->win);
  }
  widget->win = NULL;
}

void widget_purge_owner(const char *path_a, const char *path_b,
                        unsigned generation) {
  if ((!path_a || !path_a[0]) && (!path_b || !path_b[0]))
    return;

  for (int i = 0; i < g_widget_count; i++) {
    FlWidget *w = g_widgets[i];
    if (!w)
      continue;
    if (!widget_owner_matches(w, path_a, path_b))
      continue;
    if (w->gen > generation)
      continue;
    w->dead = true;
    Platform_HoverMessageRelease(w->name);
    Platform_HoverWidgetHide(w->name);
    Platform_RenderThread_RemoveFrameCallback(widget_render_frame, w);
    Platform_RenderThread_Post(widget_destroy_window, w);
    log_info("widget '%s' purged (owner unloaded)", w->name);
  }

  Platform_RenderThread_Sync();

  int n = 0;
  for (int i = 0; i < g_widget_count; i++) {
    FlWidget *w = g_widgets[i];
    if (!w)
      continue;
    if (widget_owner_matches(w, path_a, path_b) && w->gen <= generation) {
      if (w->owner)
        free(w->owner);
      free(w);
      continue;
    }
    g_widgets[n++] = w;
  }
  g_widget_count = n;
  if (g_widget_count > 0)
    widget_relayout();
  Platform_Widget_FlushDestruction();
}

bool widget_owner_active(const char *path) {
  if (!path || !path[0])
    return false;
  for (int i = 0; i < g_widget_count; i++) {
    FlWidget *w = g_widgets[i];
    if (w && widget_owner_matches(w, path, NULL))
      return true;
  }
  return false;
}

static void widget_attach_window(void *ud) {
  FlWidget *widget = (FlWidget *)ud;
  if (!widget || widget->dead)
    return;
  bool created = false;
  if (!Platform_Widget_IsValid(widget->win)) {
    Platform_SetDrawBgColor(widget->has_bg ? widget->bg_color : 0);
    widget->win = Platform_Widget_Create(widget->x, widget->y, widget->w,
                                         widget->h, widget->fps, widget->no_bg,
                                         widget->on_click, widget->name);
    Platform_SetDrawBgColor(0);
    created = (widget->win != NULL);
  } else {
    Platform_Widget_Move(widget->win, widget->x, widget->y);
  }
  if (widget->win) {
    Platform_Widget_SetBgColor(widget->win,
                               widget->has_bg ? widget->bg_color : 0);
    Platform_Widget_SetNoBg(widget->win, widget->no_bg);
    Platform_Widget_SetFps(widget->win, widget->fps);
    if (created)
      Platform_Widget_SetFrameCallback(widget->win, widget_render_frame,
                                       widget);
  }
}

static void widget_apply_geometry(FlWidget *widget, int x, int y) {
  widget->x = x;
  widget->y = y;
  if (Platform_RenderThread_IsCurrentThread()) {
    widget_attach_window(widget);
    return;
  }
  if (!Platform_RenderThread_EnsureStarted(widget->fps))
    return;
  if (!Platform_RenderThread_Post(widget_attach_window, widget))
    return;
}

static int widget_sort_key(const FlWidget *widget) {
  return widget->order_set ? widget->order : widget->seq;
}

static void widget_relayout(void) {
  int mx = 0, my = 0, mw = 1920, mh = 1080;
  widget_monitor_geometry(&mx, &my, &mw, &mh);

  FlWidget *sorted[FL_WIDGET_MAX];
  int n = 0;
  for (int i = 0; i < g_widget_count; i++) {
    if (g_widgets[i] && !g_widgets[i]->free_pos)
      sorted[n++] = g_widgets[i];
  }
  for (int i = 1; i < n; i++) {
    FlWidget *key = sorted[i];
    int k = i - 1;
    while (k >= 0 && widget_sort_key(sorted[k]) > widget_sort_key(key)) {
      sorted[k + 1] = sorted[k];
      k--;
    }
    sorted[k + 1] = key;
  }

  for (int s = 0; s < 4; s++) {
    bool from_right =
        (s == FL_WIDGET_SLOT_TOP_RIGHT || s == FL_WIDGET_SLOT_BOTTOM_RIGHT);
    bool from_top =
        (s == FL_WIDGET_SLOT_TOP_LEFT || s == FL_WIDGET_SLOT_TOP_RIGHT);
    int cursor = from_right ? mx + mw : mx;
    for (int i = 0; i < n; i++) {
      FlWidget *w = sorted[i];
      if (w->slot != s)
        continue;
      int g = w->gap_override > 0 ? w->gap_override : WidgetGap;
      int x = from_right ? cursor - w->w : cursor;
      int y = from_top ? my : my + mh - w->h;
      widget_apply_geometry(w, x, y);
      cursor += from_right ? -(w->w + g) : (w->w + g);
    }
  }
}

FlWidget *widget_create(const FlWidgetSpec *spec) {
  if (!spec || !spec->name || !spec->name[0]) {
    log_warn("widget_create: name is required");
    return NULL;
  }
  FlWidget *widget = widget_find_by_name(spec->name);
  bool fresh = (widget == NULL);
  if (fresh) {
    if (g_widget_count >= FL_WIDGET_MAX) {
      log_warn("widget_create: widget table full");
      return NULL;
    }
    widget = (FlWidget *)calloc(1, sizeof(*widget));
    if (!widget)
      return NULL;
    snprintf(widget->name, FL_WIDGET_NAME_MAX, "%s", spec->name);
    widget->seq = g_widget_seq++;
    g_widgets[g_widget_count++] = widget;
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

  widget->gen = g_widget_gen;
  widget->has_bg = spec->color != 0;
  widget->bg_color = widget->has_bg ? spec->color : 0;
  widget->no_bg = spec->no_bg;
  widget->auto_w = spec->w <= 0;
  widget->auto_h = spec->h <= 0;
  widget->pad = spec->pad > 0 ? spec->pad : FL_WIDGET_DEFAULT_PAD;
  if (fresh || !widget->auto_w)
    widget->w = widget->auto_w ? FL_WIDGET_PROVISIONAL_W : spec->w;
  if (fresh || !widget->auto_h)
    widget->h = widget->auto_h ? FL_WIDGET_PROVISIONAL_H : spec->h;
  widget->gap_override = spec->gap > 0 ? spec->gap : 0;
  widget->fps = spec->fps ? spec->fps : 1;
  widget->slot = (spec->slot >= FL_WIDGET_SLOT_TOP_LEFT &&
                  spec->slot <= FL_WIDGET_SLOT_BOTTOM_RIGHT)
                     ? spec->slot
                     : FL_WIDGET_SLOT_BOTTOM_LEFT;
  widget->order = spec->order;
  widget->order_set = spec->order > 0;
  widget->render = spec->render;
  widget->on_click = spec->on_click;
  widget->on_hover = spec->on_hover;
  widget->free_pos = (spec->slot == FL_WIDGET_FREE);
  widget->frame = 0;
  widget->created_at = Platform_StartTimer();

  if (widget->free_pos) {
    int x = spec->x;
    int y = spec->y;
    if (x < 0 && y < 0) {
      int mx = 0, my = 0, mw = 1920, mh = 1080;
      widget_monitor_geometry(&mx, &my, &mw, &mh);
      x = mx + (mw - widget->w) / 2;
      y = my + (mh - widget->h) / 2;
    }
    widget_apply_geometry(widget, x, y);
    Platform_Widget_SetFrameCallback(widget->win, widget_render_frame, widget);
  } else {
    widget_relayout();
    Platform_Widget_SetFrameCallback(widget->win, widget_render_frame, widget);
  }

  return widget;
}

bool widget_valid(FlWidget *widget) { return widget != NULL; }

FlPos widget_get_pos(const char *name) {
  FlPos r = {0, 0};
  FlWidget *w = widget_find_by_name(name);
  if (w && w->x < 0x7fffffff) {
    r.x = w->x + w->w / 2;
    r.y = w->y + w->h / 2;
  }
  return r;
}

static void widget_apply_show(void *ud) {
  FlWidget *widget = (FlWidget *)ud;
  if (widget)
    Platform_Widget_Show(widget->win);
}

static void widget_apply_hide(void *ud) {
  FlWidget *widget = (FlWidget *)ud;
  if (widget)
    Platform_Widget_Hide(widget->win);
}

void widget_show(FlWidget *widget) {
  if (!widget_valid(widget))
    return;
  if (widget->mapped)
    return;
  widget->mapped = true;
  Platform_RenderThread_Post(widget_apply_show, widget);
}

void widget_hide(FlWidget *widget) {
  if (!widget_valid(widget))
    return;
  if (!widget->mapped)
    return;
  widget->mapped = false;
  Platform_RenderThread_Post(widget_apply_hide, widget);
}

void widget_set_render(FlWidget *widget, FlWidgetRenderFn render) {
  if (!widget_valid(widget))
    return;
  widget->render = render;
  if (Platform_Widget_IsValid(widget->win)) {
    Platform_Widget_Invalidate(widget->win);
  }
}
