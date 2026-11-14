#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/X.h>
#include <stdbool.h>
#include <stdio.h>

#include "platform/widget.h"
#include "core/widget_registry.h"
#include "platform/draw.h"
#include "helpers/helpers_ui.h"
#include "helpers/x11/drawing_x11.h"
#include "helpers/x11/window_factory_x11.h"
#include "deps/log.h"
#include "config_values.h"
#include "core/types/color.h"
#include "core/types/window.h"
#include "helpers/helpers_render_thread.h"

struct PlatformWidget {
  PlatformWidgetBase base;
  Display *dpy;
  Window win;
  XftDraw *xd;
  Visual *vis;
  Colormap cmap;
  int last_draw_w;
  int last_draw_h;
};

#define WIDGET_REG_MAX 32
static PlatformWidget *s_widgets[WIDGET_REG_MAX];

static void widget_registry_add(PlatformWidget *w) {

  for (int i = 0; i < WIDGET_REG_MAX; i++) {
    if (!s_widgets[i]) {
      s_widgets[i] = w;
      return;
    }
  }
}
static void widget_registry_remove(const PlatformWidget *w) {

  for (int i = 0; i < WIDGET_REG_MAX; i++) {
    if (s_widgets[i] == w) {
      s_widgets[i] = NULL;
      return;
    }
  }
}

bool Platform_Widget_DispatchClick(long window_id) {
  for (int i = 0; i < WIDGET_REG_MAX; i++) {
    PlatformWidget *w = s_widgets[i];
    if (!w || w->base.dead)
      continue;
    if ((long)w->win != window_id)
      continue;
    if (!w->base.on_click)
      return false;
    w->base.on_click();
    return true;
  }
  return false;
}

static long long widget_now_ms(void) {

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static void x11_widget_frame(void *ud) {

  PlatformWidget *widget = (PlatformWidget *)ud;
  if (!widget || widget->base.dead) {
    return;
  }
  Display *dpy = widget->dpy;
  if (!dpy || widget->win == None) {
    return;
  }
  if (widget->base.w <= 0 || widget->base.h <= 0) {
    return;
  }

  long long now = widget_now_ms();
  long long interval =
      widget->base.fps > 0 ? 1000LL / widget->base.fps : 1000LL;
  if (interval < 16)
    interval = 16;
  if (!widget->base.dirty) {
    if (now - widget->base.last_draw_ms < interval)
      return;
  }
  widget->base.dirty = false;
  widget->base.last_draw_ms = now;

  if (widget->base.no_bg) {
    GC tgc = XCreateGC(dpy, widget->win, 0, NULL);
    XSetForeground(dpy, tgc, 0);
    XFillRectangle(dpy, widget->win, tgc, 0, 0, widget->base.w, widget->base.h);
    XFreeGC(dpy, tgc);
  } else {
    SetDrawBgColor(widget->base.has_bg ? widget->base.bg_color : 0);
    Pixmap bg = x11_get_cached_background(dpy, widget->win, widget->base.w,
                                          widget->base.h);
    if (bg) {
      GC gc = XCreateGC(dpy, widget->win, 0, NULL);
      XCopyArea(dpy, bg, widget->win, gc, 0, 0, widget->base.w, widget->base.h,
                0, 0);
      XFreeGC(dpy, gc);
    } else {
      x11_draw_background_to_drawable(dpy, widget->win, widget->base.w,
                                      widget->base.h);
    }
  }
  SetDrawBgColor(0);

  if (!widget->xd) {
    widget->xd = x11_create_xft_draw(dpy, widget->win);
    if (!widget->xd) {
      log_warn("widget: no XftDraw for window 0x%lx",
               (unsigned long)widget->win);
      return;
    }
  }

  x11_set_draw_context(dpy, widget->xd, widget->vis, widget->cmap);
  if (widget->base.fn)
    widget->base.fn(widget->base.ud);
  Platform_FlushDraw();
  x11_draw_window_border(dpy, widget->win, widget->base.w, widget->base.h,
                         widget->base.no_bg);
  XFlush(dpy);
}

PlatformWidget *Platform_Widget_Create(int x, int y, int w, int h, int fps,
                                       bool no_bg, void (*on_click)(void),
                                       const char *name) {

  (void)name;
  if (w <= 0 || h <= 0)
    return NULL;
  if (!RenderThread_EnsureStarted(fps))
    return NULL;
  Display *dpy = (Display *)RenderThread_Display();
  if (!dpy) {
    log_warn("widget: no render thread display");
    return NULL;
  }

  PlatformWidget *widget = (PlatformWidget *)calloc(1, sizeof(*widget));
  if (!widget)
    return NULL;
  widget->dpy = dpy;
  Widget_InitBase(&widget->base, x, y, w, h, fps, no_bg, on_click, name);
  widget->last_draw_w = 0;
  widget->last_draw_h = 0;

  SetTransparentWindow(no_bg);
  widget->win =
      x11_create_display_message_window(dpy, x, y, w, h, on_click != NULL);
  SetTransparentWindow(false);
  if (!widget->win) {
    free(widget);
    return NULL;
  }
  if (!x11_get_drawable_visual(dpy, widget->win, &widget->vis, &widget->cmap,
                               NULL)) {
    int screen = DefaultScreen(dpy);
    widget->vis = DefaultVisual(dpy, screen);
    widget->cmap = DefaultColormap(dpy, screen);
  }

  if (!RenderThread_AddFrameCallback(x11_widget_frame, widget)) {
    XDestroyWindow(dpy, widget->win);
    XFlush(dpy);
    free(widget);
    return NULL;
  }

  unsigned long amb = GetDrawBgColor();
  if (amb) {
    widget->base.has_bg = true;
    widget->base.bg_color = amb;
  }

  x11_widget_frame(widget);

  XMapRaised(dpy, widget->win);
  XFlush(dpy);
  widget->base.mapped = true;
  widget_registry_add(widget);
  return widget;
}

void Platform_Widget_Destroy(PlatformWidget *widget) {
  if (!widget || widget->base.dead) {
    return;
  }
  widget->base.dead = true;
  widget_registry_remove(widget);
  RenderThread_RemoveFrameCallback(x11_widget_frame, widget);
  Display *dpy = widget->dpy;
  if (dpy && widget->win != None) {
    if (widget->xd) {
      XftDrawDestroy(widget->xd);
      widget->xd = NULL;
    }
    XDestroyWindow(dpy, widget->win);
    XFlush(dpy);
    widget->win = None;
  }
  free(widget);
}

void Platform_Widget_Show(PlatformWidget *widget) {
  if (!Widget_BaseCheck(widget ? &widget->base : NULL) || widget->base.mapped) {
    return;
  }
  if (!widget->dpy || widget->win == None) {
    return;
  }
  XMapRaised(widget->dpy, widget->win);
  XFlush(widget->dpy);
  widget->base.mapped = true;
}

void Platform_Widget_Hide(PlatformWidget *widget) {
  if (!widget || !Widget_BaseCheck(&widget->base) || !widget->base.mapped) {
    return;
  }
  if (!widget->dpy || widget->win == None) {
    return;
  }
  XUnmapWindow(widget->dpy, widget->win);
  XFlush(widget->dpy);
  widget->base.mapped = false;
}

void Platform_Widget_Move(PlatformWidget *widget, int x, int y) {
  if (!widget || !Widget_BaseMove(&widget->base, x, y)) {
    return;
  }
  if (!widget->dpy || widget->win == None) {
    return;
  }
  XMoveWindow(widget->dpy, widget->win, x, y);
  XFlush(widget->dpy);
}

void Platform_Widget_SetNoBg(PlatformWidget *widget, bool no_bg) {
  if (!widget || !Widget_BaseSetNoBg(&widget->base, no_bg)) {
    return;
  }
  widget->base.dirty = true;
  RenderThread_Wake();
}

void Platform_Widget_SetBgColor(PlatformWidget *widget, ColorRef bg_color) {
  if (!widget || !Widget_BaseSetBgColor(&widget->base, bg_color)) {
    return;
  }
  if (!widget->dpy || widget->win == None) {
    return;
  }
  unsigned int a =
      (RoundedCorners || widget->base.no_bg) ? 0 : (unsigned int)WindowOpacity;
  unsigned long bg =
      widget->base.no_bg
          ? 0UL
          : (widget->base.has_bg ? RgbToHex(bg_color) : RgbToHex(COLOR_BG));
  unsigned int r = (unsigned int)((bg >> 16) & 0xFF);
  unsigned int g = (unsigned int)((bg >> 8) & 0xFF);
  unsigned int b = (unsigned int)(bg & 0xFF);
  XSetWindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.background_pixel = (a << 24) | (r << 16) | (g << 8) | b;
  XChangeWindowAttributes(widget->dpy, widget->win, CWBackPixel, &attrs);
  XFlush(widget->dpy);
  widget->base.dirty = true;
  RenderThread_Wake();
}

void Platform_Widget_Resize(PlatformWidget *widget, int w, int h) {
  if (!widget || !Widget_BaseResize(&widget->base, w, h)) {
    return;
  }
  if (!widget->dpy || widget->win == None) {
    return;
  }
  XMoveResizeWindow(widget->dpy, widget->win, widget->base.x, widget->base.y, w,
                    h);
  XFlush(widget->dpy);
  widget->base.dirty = true;
  RenderThread_Wake();
}

void Platform_Widget_FlushDestruction(void) {}
