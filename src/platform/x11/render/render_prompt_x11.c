#include "helpers/helpers_ui.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/context.h"
#include "core/checkbox.h"
#include "platform/draw.h"
#include "deps/log.h"
#include "platform/x11/drawing_x11.h"
#include "platform/x11/bg_image_x11.h"
#include "platform/x11/window_factory_x11.h"
#include "core/types.h"
#include "platform/platform.h"

static Display *g_pbuf_dpy = NULL;
static Window g_pbuf_win = None;
static int g_pbuf_w = 0;
static int g_pbuf_h = 0;
static int g_pbuf_depth = 0;
static Visual *g_pbuf_vis = NULL;
static Colormap g_pbuf_cmap = None;
static Pixmap g_pbuf_pm = None;
static XftDraw *g_pbuf_xd = NULL;

static void x11_pbuf_drop(void) {

  if (g_pbuf_xd) {
    XftDrawDestroy(g_pbuf_xd);
    g_pbuf_xd = NULL;
  }
  if (g_pbuf_pm) {
    if (g_pbuf_dpy)
      XFreePixmap(g_pbuf_dpy, g_pbuf_pm);
    g_pbuf_pm = None;
  }
  g_pbuf_dpy = NULL;
  g_pbuf_win = None;
  g_pbuf_w = 0;
  g_pbuf_h = 0;
  g_pbuf_depth = 0;
  g_pbuf_vis = NULL;
  g_pbuf_cmap = None;
}

void x11_release_prompt_buffer(Display *dpy, Window win) {
  if (g_pbuf_dpy == dpy && g_pbuf_win == win)
    x11_pbuf_drop();
}

static bool x11_pbuf_acquire(Display *dpy, Window win, int w, int h, int depth,
                             Visual *vis, Colormap cmap) {

  if (g_pbuf_xd && g_pbuf_pm && g_pbuf_dpy == dpy && g_pbuf_win == win &&
      g_pbuf_w == w && g_pbuf_h == h && g_pbuf_depth == depth &&
      g_pbuf_vis == vis && g_pbuf_cmap == cmap) {
    return true;
  }
  x11_pbuf_drop();
  Pixmap pm = XCreatePixmap(dpy, win, w, h, depth);
  if (!pm)
    return false;
  XftDraw *xd = XftDrawCreate(dpy, pm, vis, cmap);
  if (!xd) {
    XFreePixmap(dpy, pm);
    return false;
  }
  g_pbuf_dpy = dpy;
  g_pbuf_win = win;
  g_pbuf_w = w;
  g_pbuf_h = h;
  g_pbuf_depth = depth;
  g_pbuf_vis = vis;
  g_pbuf_cmap = cmap;
  g_pbuf_pm = pm;
  g_pbuf_xd = xd;
  return true;
}

void DrawPromptUserEditControlOnDraw(Display *dpy, XftDraw *xd, Visual *vis,
                                     Colormap cmap, PromptState *state) {
  x11_draw_editbox(state, dpy, xd, vis, cmap);
}

void DrawPromptUserListBoxOnDraw(Display *dpy, XftDraw *xd, Visual *vis,
                                 Colormap cmap, PromptState *state) {
  x11_draw_listbox(state, dpy, xd, vis, cmap);
}

void DrawPromptUserEditControl(Display *dpy, Window *win, PromptState *state) {
  XWindowAttributes attrs;
  Visual *vis = NULL;
  Colormap cmap = 0;
  if (XGetWindowAttributes(dpy, *win, &attrs)) {
    vis = attrs.visual;
    cmap = attrs.colormap;
  } else {
    int screen = DefaultScreen(dpy);
    vis = DefaultVisual(dpy, screen);
    cmap = DefaultColormap(dpy, screen);
  }
  XftDraw *xd = XftDrawCreate(dpy, *win, vis, cmap);
  if (!xd) {
    return;
  }
  DrawPromptUserEditControlOnDraw(dpy, xd, vis, cmap, state);
  XftDrawDestroy(xd);
}

void DrawPromptUserListBox(Display *dpy, Window *win, PromptState *state) {
  XWindowAttributes attrs;
  Visual *vis = NULL;
  Colormap cmap = 0;
  if (XGetWindowAttributes(dpy, *win, &attrs)) {
    vis = attrs.visual;
    cmap = attrs.colormap;
  } else {
    int screen = DefaultScreen(dpy);
    vis = DefaultVisual(dpy, screen);
    cmap = DefaultColormap(dpy, screen);
  }
  XftDraw *xd = XftDrawCreate(dpy, *win, vis, cmap);
  if (!xd) {
    return;
  }
  DrawPromptUserListBoxOnDraw(dpy, xd, vis, cmap, state);
  XftDrawDestroy(xd);
}

bool x11_render_prompt_user(Display *dpy, Window *win, PromptState *state) {
  if (!state || !win || *win == None || !dpy) {
    return false;
  }
  SkaldContext_Get()->window.editbox_placed = false;
  int w = state->width;
  int h = state->height;
  XWindowAttributes attrs;
  if (!XGetWindowAttributes(dpy, *win, &attrs)) {
    x11_draw_background(dpy, *win, w, h);
    x11_set_draw_context(dpy, NULL, NULL, 0);
    if (state->callbacks->render != NULL) {
      Checkbox_FrameBegin();
      x11_set_draw_context(dpy, NULL, NULL, 0);
      state->callbacks->render(state);
    } else {
      if (state->lb)
        DrawPromptUserListBox(dpy, win, state);
      DrawPromptUserEditControl(dpy, win, state);
    }
    XFlush(dpy);
    return true;
  }
  Visual *vis = attrs.visual;
  int depth = attrs.depth;
  Colormap cmap = attrs.colormap;
  if (!x11_pbuf_acquire(dpy, *win, w, h, depth, vis, cmap)) {
    x11_draw_background(dpy, *win, w, h);
    x11_set_draw_context(dpy, NULL, NULL, 0);
    if (state->callbacks->render != NULL) {
      Checkbox_FrameBegin();
      state->callbacks->render(state);
    } else {
      if (state->lb)
        DrawPromptUserListBox(dpy, win, state);
      DrawPromptUserEditControl(dpy, win, state);
    }
    XFlush(dpy);
    return true;
  }
  Pixmap dbl = g_pbuf_pm;
  XftDraw *xdDbl = g_pbuf_xd;
  x11_set_dbl_visual(dpy, vis, cmap, depth);
  PromptState *pss = state;
  if (state && state->bg_color != 0) {
    GC gc = XCreateGC(dpy, dbl, 0, NULL);
    XSetForeground(dpy, gc, rgb_to_hex(state->bg_color));
    XFillRectangle(dpy, dbl, gc, 0, 0, w, h);
    XFreeGC(dpy, gc);
  } else if (!(pss && pss->no_bg)) {
    if (!x11_bg_image_blit(dpy, dbl, vis, depth, w, h)) {
      Pixmap bg = x11_get_cached_background(dpy, *win, w, h);
      if (bg) {
        GC gc = XCreateGC(dpy, dbl, 0, NULL);
        XCopyArea(dpy, bg, dbl, gc, 0, 0, w, h, 0, 0);
        XFreeGC(dpy, gc);
      } else {
        x11_draw_background_to_drawable(dpy, dbl, w, h);
      }
    }
  } else {
    GC clear_gc = XCreateGC(dpy, dbl, 0, NULL);
    XSetForeground(dpy, clear_gc, 0);
    XFillRectangle(dpy, dbl, clear_gc, 0, 0, w, h);
    XFreeGC(dpy, clear_gc);
  }
  x11_set_draw_context(dpy, xdDbl, vis, cmap);
  if (state->callbacks->render != NULL) {
    Checkbox_FrameBegin();
    state->callbacks->render(state);
  } else {
    if (state->lb) {
      DrawPromptUserListBoxOnDraw(dpy, xdDbl, vis, cmap, state);
    }
    DrawPromptUserEditControlOnDraw(dpy, xdDbl, vis, cmap, state);
  }
  x11_set_draw_context(NULL, NULL, NULL, 0);
  x11_clear_dbl_visual();
  GC gc = XCreateGC(dpy, *win, 0, NULL);
  XCopyArea(dpy, dbl, *win, gc, 0, 0, w, h, 0, 0);
  XFreeGC(dpy, gc);
  x11_draw_window_border(dpy, *win, w, h, state && state->no_bg);
  XFlush(dpy);
  return true;
}
