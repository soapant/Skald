#include "helpers/helpers_ui.h"
#include <pthread.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/context.h"
#include "core/editbox.h"
#include "core/prompt_geometry.h"
#include "core/checkbox.h"
#include "core/focus.h"
#include "core/slider.h"
#include "platform/draw.h"
#include "deps/log.h"
#include "helpers/x11/drawing_x11.h"
#include "helpers/x11/bg_image_x11.h"
#include "helpers/x11/window_factory_x11.h"
#include "helpers/x11/subclasses_x11.h"
#include "core/types.h"
#include "platform/platform.h"
#include "core/draw_state.h"

static void prompt_state_update_delta(PromptState *state) {
  long long now_ticks = Platform_TimerNowTicks();
  long long freq = Platform_TimerTickFrequency();
  if (state->last_frame_ticks && freq > 0)
    state->delta_time =
        (double)(now_ticks - state->last_frame_ticks) * 1000.0 / (double)freq;
  state->last_frame_ticks = now_ticks;
}

static Display *pbuf_dpy = NULL;
static Window pbuf_win = None;
static int pbuf_w = 0;
static int pbuf_h = 0;
static int pbuf_depth = 0;
static Visual *pbuf_vis = NULL;
static Colormap pbuf_cmap = None;
static Pixmap pbuf_pm = None;
static XftDraw *pbuf_xd = NULL;
static bool prompt_attrs_valid = false;
static Display *prompt_attrs_dpy = NULL;
static Window prompt_attrs_win = None;
static XWindowAttributes prompt_attrs;

static pthread_mutex_t g_prompt_render_lock;
static pthread_once_t g_prompt_render_once = PTHREAD_ONCE_INIT;

static void x11_prompt_render_lock_init(void) {
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&g_prompt_render_lock, &attr);
  pthread_mutexattr_destroy(&attr);
}

static void x11_prompt_render_lock(void) {
  pthread_once(&g_prompt_render_once, x11_prompt_render_lock_init);
  pthread_mutex_lock(&g_prompt_render_lock);
}

static void x11_prompt_render_unlock(void) {
  pthread_mutex_unlock(&g_prompt_render_lock);
}

static void x11_pbuf_drop(void) {
  log_debug("prompt buffer drop dpy=%p win=%lu pixmap=%lu", (void *)pbuf_dpy,
            (unsigned long)pbuf_win, (unsigned long)pbuf_pm);

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
  prompt_attrs_valid = false;
}

void x11_release_prompt_buffer(Display *dpy, Window win) {
  x11_prompt_render_lock();
  if (pbuf_dpy == dpy && pbuf_win == win)
    x11_pbuf_drop();
  if (prompt_attrs_dpy == dpy && prompt_attrs_win == win)
    prompt_attrs_valid = false;
  x11_prompt_render_unlock();
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

static void prompt_render_frame(Display *dpy, Window *win, XftDraw *xd,
                                Visual *vis, Colormap cmap, PromptState *state,
                                bool custom_render, bool on_draw,
                                bool clear_ctx, bool suppress_content) {
  if (suppress_content)
    return;
  if (custom_render) {
    prompt_state_update_delta(state);
    checkbox_frame_begin();
    slider_frame_begin();
    focus_frame_begin(state->lb != NULL, 0, prompt_list_top(), state->width,
                      state->height - prompt_list_top() -
                          prompt_list_bottom_margin());
    editbox_registry_register_focus(state);
    if (clear_ctx)
      x11_set_draw_context(dpy, NULL, NULL, 0);
    prompt_band_take_placed();
    bool sep_prev = draw_state_get()->suppress_separators;
    draw_state_get()->suppress_separators = state->no_separators;
    editbox_registry_tick(state);
    editbox_registry_begin_frame(state);
    state->callbacks->render(state);
    prompt_band_commit_placed();
    focus_frame_end();
    draw_state_get()->suppress_separators = sep_prev;
  } else {
    focus_frame_begin(state->lb != NULL, 0, prompt_list_top(), state->width,
                      state->height - prompt_list_top() -
                          prompt_list_bottom_margin());
    editbox_registry_register_focus(state);
    bool sep_prev = draw_state_get()->suppress_separators;
    draw_state_get()->suppress_separators = state->no_separators;
    if (state->lb) {
      if (on_draw)
        x11_draw_prompt_user_listbox_on_draw(dpy, xd, vis, cmap, state);
      else
        x11_draw_prompt_user_listbox(dpy, win, state);
    }
    if (on_draw)
      x11_draw_prompt_user_edit_control_on_draw(dpy, xd, vis, cmap, state);
    else
      x11_draw_prompt_user_edit_control(dpy, win, state);
    draw_state_get()->suppress_separators = sep_prev;
    focus_frame_end();
  }
}

static bool prompt_render_fallback(Display *dpy, Window *win,
                                   PromptState *state, int w, int h,
                                   bool suppress_content) {
  log_debug("prompt render fallback win=%lu %dx%d suppress=%d",
            (unsigned long)*win, w, h, suppress_content ? 1 : 0);
  x11_draw_background(dpy, *win, w, h);
  x11_set_draw_context(dpy, NULL, NULL, 0);
  prompt_render_frame(dpy, win, NULL, NULL, 0, state,
                      state->callbacks && state->callbacks->render, false, true,
                      suppress_content);
  XFlush(dpy);
  return true;
}

static bool x11_render_prompt_user_locked(Display *dpy, Window *win,
                                          PromptState *state,
                                          bool suppress_content) {
  if (!state || !win || *win == None || !dpy) {
    return false;
  }
  int w = state->width;
  int h = state->height;
  if (!prompt_attrs_valid || prompt_attrs_dpy != dpy ||
      prompt_attrs_win != *win) {
    if (!XGetWindowAttributes(dpy, *win, &prompt_attrs))
      return prompt_render_fallback(dpy, win, state, w, h, suppress_content);
    prompt_attrs_dpy = dpy;
    prompt_attrs_win = *win;
    prompt_attrs_valid = true;
  }
  XWindowAttributes attrs = prompt_attrs;
  Visual *vis = attrs.visual;
  int depth = attrs.depth;
  Colormap cmap = attrs.colormap;
  if (!x11_pbuf_acquire(dpy, *win, w, h, depth, vis, cmap))
    return prompt_render_fallback(dpy, win, state, w, h, suppress_content);
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
  prompt_render_frame(dpy, win, xdDbl, vis, cmap, state,
                      state->callbacks->render != NULL, true, false,
                      suppress_content);
  x11_set_draw_context(NULL, NULL, NULL, 0);
  x11_clear_dbl_visual();
  x11_draw_window_border(dpy, dbl, vis, w, h, state && state->no_bg);
  GC gc = XCreateGC(dpy, *win, 0, NULL);
  XCopyArea(dpy, dbl, *win, gc, 0, 0, w, h, 0, 0);
  XFreeGC(dpy, gc);
  log_debug("prompt blit win=%lu pm=%lu %dx%d suppress=%d", (unsigned long)*win,
            (unsigned long)dbl, w, h, suppress_content ? 1 : 0);
  XFlush(dpy);
  return true;
}

bool x11_render_prompt_user(Display *dpy, Window *win, PromptState *state,
                            bool suppress_content) {
  x11_prompt_render_lock();
  bool ok = x11_render_prompt_user_locked(dpy, win, state, suppress_content);
  x11_prompt_render_unlock();
  return ok;
}
