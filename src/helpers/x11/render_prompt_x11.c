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
#include "helpers/x11/drawing_x11.h"
#include "helpers/x11/bg_image_x11.h"
#include "helpers/x11/window_factory_x11.h"
#include "helpers/x11/subclasses_x11.h"
#include "core/types.h"
#include "platform/platform.h"
#include "core/draw_state.h"

static Display *pbuf_dpy = NULL;
static Window pbuf_win = None;
static int pbuf_w = 0;
static int pbuf_h = 0;
static int pbuf_depth = 0;
static Visual *pbuf_vis = NULL;
static Colormap pbuf_cmap = None;
static Pixmap pbuf_pm = None;
static XftDraw *pbuf_xd = NULL;

static void x11_pbuf_drop(void) {

  if (pbuf_xd) {
    XftDrawDestroy(pbuf_xd);
    pbuf_xd = NULL;
  }
  if (pbuf_pm) {
    if (pbuf_dpy)
      XFreePixmap(pbuf_dpy, pbuf_pm);
    pbuf_pm = None;
  }
  pbuf_dpy = NULL;
  pbuf_win = None;
  pbuf_w = 0;
  pbuf_h = 0;
  pbuf_depth = 0;
  pbuf_vis = NULL;
  pbuf_cmap = None;
}

void x11_release_prompt_buffer(Display *dpy, Window win) {
  if (pbuf_dpy == dpy && pbuf_win == win)
    x11_pbuf_drop();
}

static bool x11_pbuf_acquire(Display *dpy, Window win, int w, int h, int depth,
                             Visual *vis, Colormap cmap) {

  if (pbuf_xd && pbuf_pm && pbuf_dpy == dpy && pbuf_win == win && pbuf_w == w &&
      pbuf_h == h && pbuf_depth == depth && pbuf_vis == vis &&
      pbuf_cmap == cmap) {
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
  pbuf_dpy = dpy;
  pbuf_win = win;
  pbuf_w = w;
  pbuf_h = h;
  pbuf_depth = depth;
  pbuf_vis = vis;
  pbuf_cmap = cmap;
  pbuf_pm = pm;
  pbuf_xd = xd;
  return true;
}

void x11_draw_prompt_user_edit_control_on_draw(Display *dpy, XftDraw *xd,
                                               Visual *vis, Colormap cmap,
                                               PromptState *state) {
  x11_draw_editbox(state, dpy, xd, vis, cmap);
}

void x11_draw_prompt_user_listbox_on_draw(Display *dpy, XftDraw *xd,
                                          Visual *vis, Colormap cmap,
                                          PromptState *state) {
  x11_draw_listbox(state, dpy, xd, vis, cmap);
}

static XftDraw *x11_prompt_create_draw(Display *dpy, Window *win,
                                       Visual **vis_out, Colormap *cmap_out) {
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
  *vis_out = vis;
  *cmap_out = cmap;
  return XftDrawCreate(dpy, *win, vis, cmap);
}

void x11_draw_prompt_user_edit_control(Display *dpy, Window *win,
                                       PromptState *state) {
  Visual *vis = NULL;
  Colormap cmap = 0;
  XftDraw *xd = x11_prompt_create_draw(dpy, win, &vis, &cmap);
  if (!xd)
    return;
  x11_draw_prompt_user_edit_control_on_draw(dpy, xd, vis, cmap, state);
  XftDrawDestroy(xd);
}

void x11_draw_prompt_user_listbox(Display *dpy, Window *win,
                                  PromptState *state) {
  Visual *vis = NULL;
  Colormap cmap = 0;
  XftDraw *xd = x11_prompt_create_draw(dpy, win, &vis, &cmap);
  if (!xd)
    return;
  x11_draw_prompt_user_listbox_on_draw(dpy, xd, vis, cmap, state);
  XftDrawDestroy(xd);
}

static bool prompt_render_fallback(Display *dpy, Window *win,
                                   PromptState *state, int w, int h) {
  x11_draw_background(dpy, *win, w, h);
  x11_set_draw_context(dpy, NULL, NULL, 0);
  if (state->callbacks && state->callbacks->render) {
    checkbox_frame_begin();
    x11_set_draw_context(dpy, NULL, NULL, 0);
    bool sep_prev = draw_state_get()->suppress_separators;
    draw_state_get()->suppress_separators = state->no_separators;
    state->callbacks->render(state);
    draw_state_get()->suppress_separators = sep_prev;
  } else {
    bool sep_prev = draw_state_get()->suppress_separators;
    draw_state_get()->suppress_separators = state->no_separators;
    if (state->lb)
      x11_draw_prompt_user_listbox(dpy, win, state);
    x11_draw_prompt_user_edit_control(dpy, win, state);
    draw_state_get()->suppress_separators = sep_prev;
  }
  XFlush(dpy);
  return true;
}

bool x11_render_prompt_user(Display *dpy, Window *win, PromptState *state) {
  if (!state || !win || *win == None || !dpy) {
    return false;
  }
  SkaldContext_Get()->window.editbox_placed = false;
  int w = state->width;
  int h = state->height;
  XWindowAttributes attrs;
  if (!XGetWindowAttributes(dpy, *win, &attrs))
    return prompt_render_fallback(dpy, win, state, w, h);
  Visual *vis = attrs.visual;
  int depth = attrs.depth;
  Colormap cmap = attrs.colormap;
  if (!x11_pbuf_acquire(dpy, *win, w, h, depth, vis, cmap))
    return prompt_render_fallback(dpy, win, state, w, h);
  Pixmap dbl = pbuf_pm;
  XftDraw *xdDbl = pbuf_xd;
  x11_set_dbl_visual(dpy, vis, cmap, depth);
  PromptState *pss = state;
  if (state && state->bg_color != 0) {
    GC gc = XCreateGC(dpy, dbl, 0, NULL);
    XSetForeground(dpy, gc, rgb_to_hex(state->bg_color));
    XFillRectangle(dpy, dbl, gc, 0, 0, w, h);
    XFreeGC(dpy, gc);
  } else if (!(pss && pss->no_bg)) {
    if (!x11_bg_image_blit(dpy, dbl, vis, w, h)) {
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
    checkbox_frame_begin();
    bool sep_prev = draw_state_get()->suppress_separators;
    draw_state_get()->suppress_separators = state->no_separators;
    state->callbacks->render(state);
    draw_state_get()->suppress_separators = sep_prev;
  } else {
    bool sep_prev = draw_state_get()->suppress_separators;
    draw_state_get()->suppress_separators = state->no_separators;
    if (state->lb) {
      x11_draw_prompt_user_listbox_on_draw(dpy, xdDbl, vis, cmap, state);
    }
    x11_draw_prompt_user_edit_control_on_draw(dpy, xdDbl, vis, cmap, state);
    draw_state_get()->suppress_separators = sep_prev;
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
