#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include "platform/x11/drawing_x11.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include "core/vim.h"
#include "platform/x11/input_x11.h"
#include "bindings/bindings.h"
#include "bindings/vim_motions.h"
#include "bindings/vim_helpers.h"
#include "core/history.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <sys/time.h>
#include <string.h>
#include "core/timer.h"
#include "platform/draw.h"
#include "core/flapi_platform.h"

void DrawPromptUserEditControlOnDraw(Display *dpy, XftDraw* xd, Visual* vis, Colormap cmap, PromptState *state) {
    void Core_DrawEditBox(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap);
    Core_DrawEditBox(state, dpy, xd, vis, cmap);
}

void DrawPromptUserListBoxOnDraw(Display *dpy, XftDraw* xd, Visual* vis, Colormap cmap, PromptState *state) {
    void Core_DrawListBox(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap);
    Core_DrawListBox(state, dpy, xd, vis, cmap);
}

void DrawPromptUserEditControl(Display *dpy,Window* win,PromptState *state) {
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

bool Linux_RenderPromptUser(Display *dpy, Window *win, PromptState *state) {
    Platform_SetPreRenderState(state);
    if (!fl_prerender()) return true;
    int w = state->width;
    int h = state->height;
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, *win, &attrs)) {
        Linux_DrawBackground(dpy, *win, w, h);
        Platform_SetDrawContext(dpy, NULL, NULL, 0);
        if (FlintContext_Get()->prompt_callbacks.render != NULL) {
          fl_editbox_reset_placed();
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
    Pixmap dbl = XCreatePixmap(dpy, *win, w, h, depth);
    if (!dbl) {
        Linux_DrawBackground(dpy, *win, w, h);
        Platform_SetDrawContext(dpy, NULL, NULL, 0);
        if (FlintContext_Get()->prompt_callbacks.render != NULL) {
          fl_editbox_reset_placed();
          FlintContext_Get()->prompt_callbacks.render(state);
        } else {
          if (state->has_listbox) DrawPromptUserListBox(dpy,win,state);
          DrawPromptUserEditControl(dpy, win,state);
        }
        XFlush(dpy);
        return true;
    }
    XftDraw* xdDbl = XftDrawCreate(dpy, dbl, vis, cmap);
    if (!xdDbl) {
        XFreePixmap(dpy, dbl);
        Linux_DrawBackground(dpy, *win, w, h);
        Platform_SetDrawContext(dpy, NULL, NULL, 0);
        if (FlintContext_Get()->prompt_callbacks.render != NULL) {
          fl_editbox_reset_placed();
          FlintContext_Get()->prompt_callbacks.render(state);
        } else {
          if (state->has_listbox) DrawPromptUserListBox(dpy,win,state);
          DrawPromptUserEditControl(dpy, win,state);
        }
        XFlush(dpy);
        return true;
    }
    Linux_SetDblVisual(dpy, vis, cmap, depth);
    Pixmap bg = Linux_GetCachedBackground(dpy, *win, w, h);
    if (bg) {
        GC gc = XCreateGC(dpy, dbl, 0, NULL);
        XCopyArea(dpy, bg, dbl, gc, 0, 0, w, h, 0, 0);
        XFreeGC(dpy, gc);
    } else {
        Linux_DrawBackgroundToDrawable(dpy, dbl, w, h);
    }
    Platform_SetDrawContext(dpy, xdDbl, vis, cmap);
    if (FlintContext_Get()->prompt_callbacks.render != NULL) {
        fl_editbox_reset_placed();
        FlintContext_Get()->prompt_callbacks.render(state);
    } else {
        if (state->has_listbox) {
            DrawPromptUserListBoxOnDraw(dpy, xdDbl, vis, cmap, state);
        }
        DrawPromptUserEditControlOnDraw(dpy, xdDbl, vis, cmap, state);
    }
    Platform_SetDrawContext(NULL,NULL,NULL,0);
    Linux_ClearDblVisual();
    GC gc = XCreateGC(dpy, *win, 0, NULL);
    XCopyArea(dpy, dbl, *win, gc, 0, 0, w, h, 0, 0);
    XFreeGC(dpy, gc);
    XftDrawDestroy(xdDbl);
    XFreePixmap(dpy, dbl);
    XFlush(dpy);
    return true;
}

