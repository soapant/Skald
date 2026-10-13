#include "core/context.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <sys/time.h>
#include "core/timer.h"
#include "platform/draw.h"
#include "platform/flapi_platform.h"
#include "platform/x11/drawing_x11.h"

static Display* g_pbuf_dpy = NULL;
static Window g_pbuf_win = None;
static int g_pbuf_w = 0;
static int g_pbuf_h = 0;
static int g_pbuf_depth = 0;
static Visual* g_pbuf_vis = NULL;
static Colormap g_pbuf_cmap = None;
static Pixmap g_pbuf_pm = None;
static XftDraw* g_pbuf_xd = NULL;

static void x11_pbuf_drop(void) {
        PROFILE_START(_pt);
if (g_pbuf_xd) { XftDrawDestroy(g_pbuf_xd); g_pbuf_xd = NULL; }
    if (g_pbuf_pm) {
        if (g_pbuf_dpy) XFreePixmap(g_pbuf_dpy, g_pbuf_pm);
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

void x11_release_prompt_buffer(Display* dpy, Window win) {
        PROFILE_START(_pt);
if (g_pbuf_dpy == dpy && g_pbuf_win == win) x11_pbuf_drop();
}

static bool x11_pbuf_acquire(Display* dpy, Window win, int w, int h, int depth, Visual* vis, Colormap cmap) {
        PROFILE_START(_pt);
if (g_pbuf_xd && g_pbuf_pm && g_pbuf_dpy == dpy && g_pbuf_win == win &&
        g_pbuf_w == w && g_pbuf_h == h && g_pbuf_depth == depth &&
        g_pbuf_vis == vis && g_pbuf_cmap == cmap) {
        return true;
    }
    x11_pbuf_drop();
    Pixmap pm = XCreatePixmap(dpy, win, w, h, depth);
    if (!pm) { PROFILE_END_VAR(_pt, "x11_pbuf_acquire"); return false; }
    XftDraw* xd = XftDrawCreate(dpy, pm, vis, cmap);
    if (!xd) { XFreePixmap(dpy, pm); return false; }
    g_pbuf_dpy = dpy;
    g_pbuf_win = win;
    g_pbuf_w = w;
    g_pbuf_h = h;
    g_pbuf_depth = depth;
    g_pbuf_vis = vis;
    g_pbuf_cmap = cmap;
    g_pbuf_pm = pm;
    g_pbuf_xd = xd;
    { PROFILE_END_VAR(_pt, "x11_pbuf_acquire"); return true; }
}

void DrawPromptUserEditControlOnDraw(Display *dpy, XftDraw* xd, Visual* vis, Colormap cmap, PromptState *state) {
        PROFILE_START(_pt);
Platform_DrawEditBox(state, dpy, xd, vis, cmap);
}

void DrawPromptUserListBoxOnDraw(Display *dpy, XftDraw* xd, Visual* vis, Colormap cmap, PromptState *state) {
        PROFILE_START(_pt);
Platform_DrawListBox(state, dpy, xd, vis, cmap);
}

void DrawPromptUserEditControl(Display *dpy,Window* win,PromptState *state) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    XWindowAttributes attrs;
    Visual* vis = NULL;
    Colormap cmap = 0;
    if (XGetWindowAttributes(dpy, *win, &attrs)) {
        vis = attrs.visual; cmap = attrs.colormap;
    } else {
        int screen = DefaultScreen(dpy);
        vis = DefaultVisual(dpy, screen);
        cmap = DefaultColormap(dpy, screen);
    }
    XftDraw* xd = XftDrawCreate(dpy, *win, vis, cmap);
    if (!xd) { PROFILE_END_VAR(timer, "subclasses_x11:DrawPromptUserEditControl"); return; }
    DrawPromptUserEditControlOnDraw(dpy, xd, vis, cmap, state);
    XftDrawDestroy(xd);
    PROFILE_END_VAR(timer, "subclasses_x11:DrawPromptUserEditControl");
}

void DrawPromptUserListBox(Display *dpy,Window* win,PromptState *state) {
            PROFILE_START(_pt);
PROFILE_START(timer);
      XWindowAttributes attrs;
      Visual* vis = NULL;
      Colormap cmap = 0;
      if (XGetWindowAttributes(dpy, *win, &attrs)) {
          vis = attrs.visual; cmap = attrs.colormap;
      } else {
          int screen = DefaultScreen(dpy);
          vis = DefaultVisual(dpy, screen);
          cmap = DefaultColormap(dpy, screen);
      }
      XftDraw* xd = XftDrawCreate(dpy, *win, vis, cmap);
      if (!xd) { PROFILE_END_VAR(timer, "subclasses_x11:DrawPromptUserListBox"); return; }
      DrawPromptUserListBoxOnDraw(dpy, xd, vis, cmap, state);
      XftDrawDestroy(xd);
      PROFILE_END_VAR(timer, "subclasses_x11:DrawPromptUserListBox");
}

bool x11_render_prompt_user(Display *dpy, Window *win, PromptState *state) {
        PROFILE_START(_pt);
Platform_SetPreRenderState(state);
    if (!fl_prerender()) { PROFILE_END_VAR(_pt, "x11_render_prompt_user"); return true; }
    int w = state->width;
    int h = state->height;
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, *win, &attrs)) {
        x11_draw_background(dpy, *win, w, h);
        Platform_SetDrawContext(dpy, NULL, NULL, 0);
        if (FlintContext_Get()->prompt_callbacks.render != NULL) {
          fl_editbox_reset_placed();
          fl_checkbox_frame_begin();
          Platform_SetDrawContext(dpy, NULL, NULL, 0);
          FlintContext_Get()->prompt_callbacks.render(state);
        } else {
          if (state->has_listbox) DrawPromptUserListBox(dpy,win,state);
          DrawPromptUserEditControl(dpy, win,state);
        }
        XFlush(dpy);
        return true;
    }
    Visual* vis = attrs.visual;
    int depth = attrs.depth;
    Colormap cmap = attrs.colormap;
    if (!x11_pbuf_acquire(dpy, *win, w, h, depth, vis, cmap)) {
        x11_draw_background(dpy, *win, w, h);
        Platform_SetDrawContext(dpy, NULL, NULL, 0);
        if (FlintContext_Get()->prompt_callbacks.render != NULL) {
          fl_editbox_reset_placed();
          fl_checkbox_frame_begin();
          FlintContext_Get()->prompt_callbacks.render(state);
        } else {
          if (state->has_listbox) DrawPromptUserListBox(dpy,win,state);
          DrawPromptUserEditControl(dpy, win,state);
        }
        XFlush(dpy);
        return true;
    }
    Pixmap dbl = g_pbuf_pm;
    XftDraw* xdDbl = g_pbuf_xd;
    x11_set_dbl_visual(dpy, vis, cmap, depth);
    Pixmap bg = x11_get_cached_background(dpy, *win, w, h);
    if (bg) {
        GC gc = XCreateGC(dpy, dbl, 0, NULL);
        XCopyArea(dpy, bg, dbl, gc, 0, 0, w, h, 0, 0);
        XFreeGC(dpy, gc);
    } else {
        x11_draw_background_to_drawable(dpy, dbl, w, h);
    }
    Platform_SetDrawContext(dpy, xdDbl, vis, cmap);
    if (FlintContext_Get()->prompt_callbacks.render != NULL) {
        fl_editbox_reset_placed();
        fl_checkbox_frame_begin();
        FlintContext_Get()->prompt_callbacks.render(state);
    } else {
        if (state->has_listbox) {
            DrawPromptUserListBoxOnDraw(dpy, xdDbl, vis, cmap, state);
        }
        DrawPromptUserEditControlOnDraw(dpy, xdDbl, vis, cmap, state);
    }
    Platform_SetDrawContext(NULL,NULL,NULL,0);
    x11_clear_dbl_visual();
    GC gc = XCreateGC(dpy, *win, 0, NULL);
    XCopyArea(dpy, dbl, *win, gc, 0, 0, w, h, 0, 0);
    XFreeGC(dpy, gc);
    XFlush(dpy);
    { PROFILE_END_VAR(_pt, "x11_render_prompt_user"); return true; }
}

