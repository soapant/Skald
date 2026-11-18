#include "core/slider.h"

#include <math.h>
#include <string.h>

#include "deps/log.h"
#include "core/types/color.h"
#include "core/types/window.h"
#include "core/window_manager.h"
#include "helpers/helpers_ui.h"
#include "platform/draw.h"
#include "helpers/helpers_input_state.h"
#include "platform/platform.h"
#include "config_values.h"
#include "core/context.h"

#define SLIDER_MAX 1024
#define SLIDER_MIN_SPAN 1e-6f
#define SLIDER_WHITE RGB(155, 155, 155)
#define SLIDER_GRAY RGB(68, 68, 68)
#define SLIDER_THUMB RGB(240, 240, 240)

static SliderSpec sliders[SLIDER_MAX];
static int slider_count = 0;

static struct {
  bool valid;
  int mx, my;
  bool down;
} sl_mouse;

void slider_frame_begin(void) { sl_mouse.valid = false; }

static void slider_mouse(int *out_x, int *out_y, bool *out_down) {

  if (!sl_mouse.valid) {
    int mx = 0, my = 0;
    if (Platform_GetMousePos(&mx, &my)) {
      int wx = 0, wy = 0;
      WindowManager *wm = WindowManager_Get();
      Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
      bool have_origin = false;
      if (top)
        have_origin = Platform_WindowToRoot(top->handle, &wx, &wy);
      if (!have_origin) {
        have_origin = Platform_WindowToRoot(
            SkaldContext_Get()->window.hPopupWnd, &wx, &wy);
      }
      if (have_origin) {
        mx -= wx;
        my -= wy;
      }
    } else {
      mx = -9999;
      my = -9999;
    }
    sl_mouse.mx = mx;
    sl_mouse.my = my;
    sl_mouse.down = input_mouse_state_down(MOUSE_BTN_LEFT);
    sl_mouse.valid = true;
  }
  if (out_x)
    *out_x = sl_mouse.mx;
  if (out_y)
    *out_y = sl_mouse.my;
  if (out_down)
    *out_down = sl_mouse.down;
}

static SliderSpec *slider_find(const char *name) {

  if (!name)
    return NULL;
  for (int i = 0; i < slider_count; i++) {
    if (strcmp(sliders[i].name, name) == 0)
      return &sliders[i];
  }
  return NULL;
}

static SliderSpec *slider_intern(const char *name, float seed,
                                 bool *out_fresh) {

  if (out_fresh)
    *out_fresh = false;
  SliderSpec *sl = slider_find(name);
  if (sl)
    return sl;
  if (slider_count >= SLIDER_MAX) {
    log_error("slider table full (%d), refusing '%s'", SLIDER_MAX, name);
    return NULL;
  }
  sl = &sliders[slider_count++];
  memset(sl, 0, sizeof(*sl));
  sl->orientation = SLIDER_HORIZONTAL;
  sl->min_value = 0.0f;
  sl->max_value = 100.0f;
  sl->value = seed;
  sl->thickness = 4;
  sl->border = true;
  sl->name = strdup(name);
  if (!sl->name) {
    slider_count--;
    return NULL;
  }
  if (out_fresh)
    *out_fresh = true;
  return sl;
}

static float slider_clamp01(float v) {
  return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static float slider_snap(const SliderSpec *sl, float v) {
  float range = sl->max_value - sl->min_value;
  if (range < SLIDER_MIN_SPAN)
    return sl->min_value;
  if (v < sl->min_value)
    v = sl->min_value;
  if (v > sl->max_value)
    v = sl->max_value;
  if (sl->step > 0.0f) {
    float rel = (v - sl->min_value) / sl->step;
    v = sl->min_value + floorf(rel + 0.5f) * sl->step;
    if (v < sl->min_value)
      v = sl->min_value;
    if (v > sl->max_value)
      v = sl->max_value;
  }
  return v;
}

static float slider_fraction_of(const SliderSpec *sl) {
  float range = sl->max_value - sl->min_value;
  if (range < SLIDER_MIN_SPAN)
    return 0.0f;
  return slider_clamp01((sl->value - sl->min_value) / range);
}

static float slider_value_of_axis(const SliderSpec *sl, int axis, int span) {
  if (span <= 1)
    return sl->min_value;
  float frac = (float)axis / (float)(span - 1);
  if (frac < 0.0f)
    frac = 0.0f;
  if (frac > 1.0f)
    frac = 1.0f;
  if (sl->orientation == SLIDER_VERTICAL)
    frac = 1.0f - frac;
  return sl->min_value + frac * (sl->max_value - sl->min_value);
}

bool slider_is_hovered(const char *name) {
  const SliderSpec *sl = name ? slider_find(name) : NULL;
  return sl ? sl->hovered : false;
}

bool slider_is_clicked(const char *name) {
  const SliderSpec *sl = name ? slider_find(name) : NULL;
  return sl ? sl->clicked : false;
}

bool slider_is_dragging(const char *name) {
  const SliderSpec *sl = name ? slider_find(name) : NULL;
  return sl ? sl->dragging : false;
}

float slider_value(const char *name) {
  const SliderSpec *sl = name ? slider_find(name) : NULL;
  return sl ? sl->value : 0.0f;
}

float slider_fraction(const char *name) {
  const SliderSpec *sl = name ? slider_find(name) : NULL;
  return sl ? slider_fraction_of(sl) : 0.0f;
}

int slider_percent(const char *name) {
  return (int)lroundf(slider_fraction(name) * 100.0f);
}

bool slider_set_value(const char *name, float value) {
  SliderSpec *sl = name ? slider_find(name) : NULL;
  if (!sl)
    return false;
  sl->value = slider_snap(sl, value);
  return true;
}

void slider_reset_all(void) {
  for (int i = 0; i < slider_count; i++) {
    sliders[i].hovered = false;
    sliders[i].dragging = false;
    sliders[i].clicked = false;
    sliders[i].was_down = false;
    sliders[i].on_input = NULL;
    sliders[i].on_change = NULL;
    sliders[i].on_hover = NULL;
    sliders[i].render = NULL;
  }
}

static void slider_track_geometry(const SliderSpec *sl, int *tx, int *ty,
                                  int *tw, int *th, int *rx, int *ry,
                                  int *rsize) {
  int t = sl->thickness > 0 ? sl->thickness : 4;
  int ts;
  if (sl->orientation == SLIDER_VERTICAL) {
    ts = sl->w > 0 ? sl->w : 16;
    int track_x = sl->x + (sl->w - t) / 2;
    *tx = track_x;
    *ty = sl->y;
    *tw = t;
    *th = sl->h;
    *rx = sl->x + (sl->w - ts) / 2;
    *ry = sl->y +
          (int)lroundf((float)(sl->h - ts) * (1.0f - slider_fraction_of(sl)));
    *rsize = ts;
    return;
  }
  t = sl->thickness > 0 ? sl->thickness : 4;
  if (t > sl->h)
    t = sl->h > 0 ? sl->h : t;
  ts = sl->h > 0 ? sl->h : 16;
  int track_y = sl->y + (sl->h - t) / 2;
  *tx = sl->x;
  *ty = track_y;
  *tw = sl->w;
  *th = t;
  *rx = sl->x + (int)lroundf((float)(sl->w - ts) * slider_fraction_of(sl));
  *ry = sl->y + (sl->h - ts) / 2;
  *rsize = ts;
}

void slider_draw(const SliderSpec *spec) {
  if (!spec)
    return;
  int tw =
      spec->w > 0 ? spec->w : (spec->orientation == SLIDER_VERTICAL ? 16 : 150);
  int th =
      spec->h > 0 ? spec->h : (spec->orientation == SLIDER_VERTICAL ? 150 : 16);
  if (spec->render) {
    SliderState st;
    memset(&st, 0, sizeof(st));
    st.rect.x = spec->x;
    st.rect.y = spec->y;
    st.rect.w = tw;
    st.rect.h = th;
    st.name = spec->name;
    st.orientation = spec->orientation;
    st.hovered = spec->hovered;
    st.dragging = spec->dragging;
    st.clicked = spec->clicked;
    st.value = spec->value;
    st.fraction = slider_fraction_of(spec);
    spec->render(st);
    return;
  }

  int tx, ty, twk, thk, rx, ry, rsize;
  slider_track_geometry(spec, &tx, &ty, &twk, &thk, &rx, &ry, &rsize);

  unsigned long track_bg = spec->bg_color ? spec->bg_color : COLOR_BG;
  unsigned long fill = spec->fill_color
                           ? spec->fill_color
                           : (spec->hovered ? SLIDER_WHITE : SLIDER_GRAY);
  unsigned long thumb = spec->thumb_color ? spec->thumb_color : SLIDER_THUMB;
  unsigned long track_border = spec->hovered ? SLIDER_WHITE : SLIDER_GRAY;

  if (!spec->no_bg) {
    Platform_DrawRectRaw(
        (DrawRectRawSpec){.x = tx,
                          .y = ty,
                          .w = twk,
                          .h = thk,
                          .color = track_bg,
                          .radius = thk > twk ? twk / 2 : thk / 2,
                          .rounded = true,
                          .opaque = true,
                          .thickness = 1,
                          .filled = true});
  }

  int fill_radius = thk > twk ? twk / 2 : thk / 2;
  if (spec->orientation == SLIDER_VERTICAL) {
    int fill_h = (int)lroundf((float)thk * slider_fraction_of(spec));
    Platform_DrawRectRaw((DrawRectRawSpec){.x = tx,
                                           .y = ty + thk - fill_h,
                                           .w = twk,
                                           .h = fill_h,
                                           .color = fill,
                                           .radius = fill_radius,
                                           .rounded = true,
                                           .opaque = true,
                                           .thickness = 1,
                                           .filled = true});
  } else {
    int fill_w = (int)lroundf((float)twk * slider_fraction_of(spec));
    Platform_DrawRectRaw((DrawRectRawSpec){.x = tx,
                                           .y = ty,
                                           .w = fill_w,
                                           .h = thk,
                                           .color = fill,
                                           .radius = fill_radius,
                                           .rounded = true,
                                           .opaque = true,
                                           .thickness = 1,
                                           .filled = true});
  }

  if (spec->border) {
    Platform_DrawRectRaw((DrawRectRawSpec){.x = tx,
                                           .y = ty,
                                           .w = twk,
                                           .h = thk,
                                           .color = track_border,
                                           .radius = fill_radius,
                                           .rounded = true,
                                           .opaque = true,
                                           .thickness = 1,
                                           .filled = false});
  }

  Platform_DrawRectRaw((DrawRectRawSpec){.x = rx,
                                         .y = ry,
                                         .w = rsize,
                                         .h = rsize,
                                         .color = thumb,
                                         .radius = rsize / 2,
                                         .rounded = true,
                                         .opaque = true,
                                         .thickness = 1,
                                         .filled = true});
  Platform_DrawRectRaw((DrawRectRawSpec){.x = rx,
                                         .y = ry,
                                         .w = rsize,
                                         .h = rsize,
                                         .color = track_border,
                                         .radius = rsize / 2,
                                         .rounded = true,
                                         .opaque = true,
                                         .thickness = 1,
                                         .filled = false});
}

bool slider_place(const SliderSpec *spec) {
  if (!spec || !spec->name || !spec->name[0])
    return false;

  int x = spec->x, y = spec->y;
  int w =
      spec->w > 0 ? spec->w : (spec->orientation == SLIDER_VERTICAL ? 16 : 150);
  int h =
      spec->h > 0 ? spec->h : (spec->orientation == SLIDER_VERTICAL ? 150 : 16);

  float minv = spec->min_value;
  float maxv = spec->max_value;
  if (maxv - minv < SLIDER_MIN_SPAN) {
    log_error("slider '%s' has an empty range (%g..%g)", spec->name,
              (double)minv, (double)maxv);
    return false;
  }

  float seed = spec->value;
  bool fresh = false;
  SliderSpec *sl = slider_intern(spec->name, seed, &fresh);
  if (!sl)
    return false;

  sl->x = x;
  sl->y = y;
  sl->w = w;
  sl->h = h;
  sl->orientation = spec->orientation == SLIDER_VERTICAL ? SLIDER_VERTICAL
                                                         : SLIDER_HORIZONTAL;
  sl->min_value = minv;
  sl->max_value = maxv;
  sl->step = spec->step > 0.0f ? spec->step : 0.0f;
  if (sl->value < sl->min_value)
    sl->value = sl->min_value;
  if (sl->value > sl->max_value)
    sl->value = sl->max_value;
  sl->thickness = spec->thickness > 0 ? spec->thickness : 4;
  sl->fill_color = spec->fill_color;
  sl->thumb_color = spec->thumb_color;
  sl->bg_color = spec->bg_color;
  sl->hovered_color = spec->hovered_color;
  sl->clicked_color = spec->clicked_color;
  sl->border = spec->border;
  sl->no_bg = spec->no_bg;
  sl->on_input = spec->on_input;
  sl->on_change = spec->on_change;
  sl->on_hover = spec->on_hover;
  sl->render = spec->render;

  if (fresh)
    sl->value = slider_snap(sl, seed);

  int mx = 0, my = 0;
  bool down = false;
  slider_mouse(&mx, &my, &down);

  bool hovered = (mx >= x && mx < x + w && my >= y && my < y + h);
  bool hover_changed = (hovered != sl->hovered);
  sl->hovered = hovered;

  int rel_x = mx - x, rel_y = my - y;
  int span = sl->orientation == SLIDER_VERTICAL ? h : w;
  int axis = sl->orientation == SLIDER_VERTICAL ? rel_y : rel_x;

  if (down && !sl->was_down) {
    if (hovered) {
      sl->dragging = true;
      sl->clicked = true;
      float nv = slider_snap(sl, slider_value_of_axis(sl, axis, span));
      bool changed = nv != sl->value;
      sl->value = nv;
      if (changed || sl->step > 0.0f) {
        if (sl->on_input)
          sl->on_input(sl->value);
      }
    }
  }

  if (sl->dragging && down) {
    float nv = slider_snap(sl, slider_value_of_axis(sl, axis, span));
    if (nv != sl->value) {
      sl->value = nv;
      if (sl->on_input)
        sl->on_input(sl->value);
    }
  }

  if (!down && sl->was_down) {
    sl->clicked = false;
    if (sl->dragging) {
      sl->dragging = false;
      if (sl->on_change)
        sl->on_change(sl->value);
    }
  }

  sl->was_down = down;

  slider_draw(sl);

  if (hover_changed && sl->on_hover)
    sl->on_hover();
  return true;
}
