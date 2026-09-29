#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include "platform/widget.h"
#include "platform/draw.h"
#include "platform/x11/drawing_x11.h"
#include "platform/x11/window_factory_x11.h"
#include "deps/log.h"

struct PlatformWidget {
    Display *dpy;
    Window win;
    XftDraw *xd;
    Visual *vis;
    Colormap cmap;
    int x, y, w, h;
    bool mapped;
    bool dead;
    Platform_RenderFrameFn fn;
    void *ud;
    int fps;
};

static void widget_frame(void *ud) {
    PlatformWidget *widget = (PlatformWidget *)ud;
    if (!widget || widget->dead) return;
    Display *dpy = widget->dpy;
    if (!dpy || widget->win == None) return;
    if (widget->w <= 0 || widget->h <= 0) return;

    Pixmap bg = x11_get_cached_background(dpy, widget->win, widget->w, widget->h);
    if (bg) {
        GC gc = XCreateGC(dpy, widget->win, 0, NULL);
        XCopyArea(dpy, bg, widget->win, gc, 0, 0, widget->w, widget->h, 0, 0);
        XFreeGC(dpy, gc);
    } else {
        x11_draw_background_to_drawable(dpy, widget->win, widget->w, widget->h);
    }

    if (!widget->xd) {
        widget->xd = x11_create_xft_draw(dpy, widget->win);
        if (!widget->xd) {
            log_warn("widget: no XftDraw for window 0x%lx", (unsigned long)widget->win);
            return;
        }
    }

    Platform_SetDrawContext(dpy, widget->xd, widget->vis, widget->cmap);
    Platform_SetPreRenderState(NULL);
    if (widget->fn) widget->fn(widget->ud);
    Platform_FlushDraw();
    XFlush(dpy);
}

PlatformWidget *Platform_Widget_Create(int x, int y, int w, int h, int fps) {
    if (w <= 0 || h <= 0) return NULL;
    if (!Platform_RenderThread_EnsureStarted(fps)) return NULL;
    Display *dpy = (Display *)Platform_RenderThread_Display();
    if (!dpy) {
        log_warn("widget: no render thread display");
        return NULL;
    }

    PlatformWidget *widget = (PlatformWidget *)calloc(1, sizeof(*widget));
    if (!widget) return NULL;
    widget->dpy = dpy;
    widget->x = x;
    widget->y = y;
    widget->w = w;
    widget->h = h;
    widget->fps = fps;

    widget->win = x11_create_display_message_window(dpy, x, y, w, h);
    if (!widget->win) {
        free(widget);
        return NULL;
    }
    if (!x11_get_drawable_visual(dpy, widget->win, &widget->vis, &widget->cmap, NULL)) {
        int screen = DefaultScreen(dpy);
        widget->vis = DefaultVisual(dpy, screen);
        widget->cmap = DefaultColormap(dpy, screen);
    }

    XMapRaised(dpy, widget->win);
    XFlush(dpy);
    widget->mapped = true;

    if (!Platform_RenderThread_AddFrameCallback(widget_frame, widget)) {
        XDestroyWindow(dpy, widget->win);
        XFlush(dpy);
        free(widget);
        return NULL;
    }
    log_info("widget created win=0x%lx at %d,%d %dx%d fps=%d", (unsigned long)widget->win, x, y, w, h, fps);
    return widget;
}

void Platform_Widget_Destroy(PlatformWidget *widget) {
    if (!widget || widget->dead) return;
    widget->dead = true;
    Platform_RenderThread_RemoveFrameCallback(widget_frame, widget);
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
    log_info("widget destroyed");
    free(widget);
}

bool Platform_Widget_IsValid(const PlatformWidget *widget) {
    return widget && !widget->dead;
}

void Platform_Widget_SetFrameCallback(PlatformWidget *widget, Platform_RenderFrameFn fn, void *ud) {
    if (!widget || widget->dead) return;
    widget->fn = fn;
    widget->ud = ud;
}

void Platform_Widget_Show(PlatformWidget *widget) {
    if (!widget || widget->dead || widget->mapped) return;
    if (!widget->dpy || widget->win == None) return;
    XMapRaised(widget->dpy, widget->win);
    XFlush(widget->dpy);
    widget->mapped = true;
}

void Platform_Widget_Hide(PlatformWidget *widget) {
    if (!widget || widget->dead || !widget->mapped) return;
    if (!widget->dpy || widget->win == None) return;
    XUnmapWindow(widget->dpy, widget->win);
    XFlush(widget->dpy);
    widget->mapped = false;
}

int Platform_Widget_Width(const PlatformWidget *widget) {
    return widget ? widget->w : 0;
}

int Platform_Widget_Height(const PlatformWidget *widget) {
    return widget ? widget->h : 0;
}
