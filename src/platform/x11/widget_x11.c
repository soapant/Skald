#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include "platform/widget.h"
#include "platform/draw.h"
#include "platform/ui.h"
#include "platform/platform.h"
#include "config.h"
#include "helpers/helpers_ui.h"
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
    unsigned long bg_hex;
    bool has_bg;
    bool no_bg;
    bool dirty;
    long long last_draw_ms;
    int last_draw_w;
    int last_draw_h;
    void (*on_click)(void);
    char name[64];
};

#define WIDGET_REG_MAX 32
static PlatformWidget* s_widgets[WIDGET_REG_MAX];

static void widget_registry_add(PlatformWidget* w) {
    for (int i = 0; i < WIDGET_REG_MAX; i++) {
        if (!s_widgets[i]) { s_widgets[i] = w; return; }
    }
}
static void widget_registry_remove(PlatformWidget* w) {
    for (int i = 0; i < WIDGET_REG_MAX; i++) {
        if (s_widgets[i] == w) { s_widgets[i] = NULL; return; }
    }
}

bool Platform_Widget_DispatchClick(long window_id) {
    for (int i = 0; i < WIDGET_REG_MAX; i++) {
        PlatformWidget* w = s_widgets[i];
        if (!w || w->dead) continue;
        if ((long)w->win != window_id) continue;
        if (!w->on_click) return false;
        w->on_click();
        return true;
    }
    return false;
}

static long long widget_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static void widget_frame(void *ud) {
    PlatformWidget *widget = (PlatformWidget *)ud;
    if (!widget || widget->dead) return;
    Display *dpy = widget->dpy;
    if (!dpy || widget->win == None) return;
    if (widget->w <= 0 || widget->h <= 0) return;

    long long now = widget_now_ms();
    long long interval = widget->fps > 0 ? 1000LL / widget->fps : 1000LL;
    if (interval < 16) interval = 16;
    const char *reason = "fps";
    if (!widget->dirty) {
        if (now - widget->last_draw_ms < interval) return;
    } else {
        reason = "DIRTY";
    }
    widget->dirty = false;
    widget->last_draw_ms = now;

    if (widget->no_bg) {
        GC tgc = XCreateGC(dpy, widget->win, 0, NULL);
        XSetForeground(dpy, tgc, 0);
        XFillRectangle(dpy, widget->win, tgc, 0, 0, widget->w, widget->h);
        XFreeGC(dpy, tgc);
    } else {
        Platform_SetDrawBgColor(widget->has_bg ? widget->bg_hex : 0);
        Pixmap bg = x11_get_cached_background(dpy, widget->win, widget->w, widget->h);
        if (bg) {
            GC gc = XCreateGC(dpy, widget->win, 0, NULL);
            XCopyArea(dpy, bg, widget->win, gc, 0, 0, widget->w, widget->h, 0, 0);
            XFreeGC(dpy, gc);
        } else {
            x11_draw_background_to_drawable(dpy, widget->win, widget->w, widget->h);
        }
    }
    Platform_SetDrawBgColor(0);

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

PlatformWidget *Platform_Widget_Create(int x, int y, int w, int h, int fps, bool no_bg, void (*on_click)(void), const char *name) {
    (void)name;
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
    widget->no_bg = no_bg;
    widget->on_click = on_click;
    snprintf(widget->name, sizeof(widget->name), "%s", name ? name : "?");
    widget->dirty = true;
    widget->last_draw_ms = 0;
    widget->last_draw_w = 0;
    widget->last_draw_h = 0;

    Platform_SetTransparentWindow(no_bg);
    widget->win = x11_create_display_message_window(dpy, x, y, w, h, on_click != NULL);
    Platform_SetTransparentWindow(false);
    if (!widget->win) {
        free(widget);
        return NULL;
    }
    if (!x11_get_drawable_visual(dpy, widget->win, &widget->vis, &widget->cmap, NULL)) {
        int screen = DefaultScreen(dpy);
        widget->vis = DefaultVisual(dpy, screen);
        widget->cmap = DefaultColormap(dpy, screen);
    }

    if (!Platform_RenderThread_AddFrameCallback(widget_frame, widget)) {
        XDestroyWindow(dpy, widget->win);
        XFlush(dpy);
        free(widget);
        return NULL;
    }

    unsigned long amb = Platform_GetDrawBgColor();
    if (amb) {
        widget->has_bg = true;
        widget->bg_hex = amb;
    }

    widget_frame(widget);

    XMapRaised(dpy, widget->win);
    XFlush(dpy);
    widget->mapped = true;
    widget_registry_add(widget);
    return widget;
}

void Platform_Widget_Destroy(PlatformWidget *widget) {
    if (!widget || widget->dead) return;
    widget->dead = true;
    widget_registry_remove(widget);
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

void Platform_Widget_Move(PlatformWidget *widget, int x, int y) {
    if (!widget || widget->dead) return;
    if (!widget->dpy || widget->win == None) return;
    if (widget->x == x && widget->y == y) return;
    widget->x = x;
    widget->y = y;
    XMoveWindow(widget->dpy, widget->win, x, y);
    XFlush(widget->dpy);
}

void Platform_Widget_SetNoBg(PlatformWidget *widget, bool no_bg) {
    if (!widget || widget->dead) return;
    widget->no_bg = no_bg;
    widget->dirty = true;
    Platform_RenderThread_Wake();
}

void Platform_Widget_SetFps(PlatformWidget *widget, int fps) {
    if (!widget || widget->dead) return;
    if (fps <= 0) fps = 1;
    if (widget->fps == fps) return;
    widget->fps = fps;
    widget->last_draw_ms = 0;
    Platform_RenderThread_EnsureStarted(fps);
    widget->dirty = true;
    Platform_RenderThread_Wake();
}

void Platform_Widget_SetBgColor(PlatformWidget *widget, unsigned long hex) {
    if (!widget || widget->dead) return;
    widget->has_bg = hex != 0;
    widget->bg_hex = hex;
    if (!widget->dpy || widget->win == None) return;
    unsigned int a = (RoundedCorners || widget->no_bg) ? 0 : (unsigned int)WindowOpacity;
    unsigned long bg = widget->no_bg ? 0UL : (widget->has_bg ? hex : rgb_to_hex(COLOR_BG));
    unsigned int r = (unsigned int)((bg >> 16) & 0xFF);
    unsigned int g = (unsigned int)((bg >> 8) & 0xFF);
    unsigned int b = (unsigned int)(bg & 0xFF);
    XSetWindowAttributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.background_pixel = (a << 24) | (r << 16) | (g << 8) | b;
    XChangeWindowAttributes(widget->dpy, widget->win, CWBackPixel, &attrs);
    XFlush(widget->dpy);
    widget->dirty = true;
    Platform_RenderThread_Wake();
}

void Platform_Widget_Invalidate(PlatformWidget *widget) {
    if (!widget || widget->dead) return;
    widget->dirty = true;
    Platform_RenderThread_Wake();
}

void Platform_Widget_Resize(PlatformWidget *widget, int w, int h) {
    if (!widget || widget->dead) return;
    if (w <= 0 || h <= 0) return;
    if (!widget->dpy || widget->win == None) return;
    if (widget->w == w && widget->h == h) return;
    widget->w = w;
    widget->h = h;
    XMoveResizeWindow(widget->dpy, widget->win, widget->x, widget->y, w, h);
    XFlush(widget->dpy);
    widget->dirty = true;
    Platform_RenderThread_Wake();
}

int Platform_Widget_Width(const PlatformWidget *widget) {
    return widget ? widget->w : 0;
}

int Platform_Widget_Height(const PlatformWidget *widget) {
    return widget ? widget->h : 0;
}
