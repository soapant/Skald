#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include "core/platform.h"
#include "platform/ui.h"
#include "core/vim.h"
#include "bindings/vim_helpers.h"
#include "config.h"
#include "platform/draw.h"
#include "platform/wm_platform.h"
#include "core/timer.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <unistd.h>
#include <stdatomic.h>
#include "platform/x11/display_x11.h"
#include "platform/x11/drawing_x11.h"
#include "platform/x11/window_factory_x11.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

bool Platform_EditPopupCreateWindow(Popup* self, int x, int y, int w, int h, const char* title) {
    (void)title;
    PROFILE_START(timer);
    WindowManager* wm = (WindowManager*)self->manager;
    Display* dpy = wm ? wm->display : NULL;
    ((WMEditPopupData*)self)->base.handle.xid = Linux_CreateEditPopupWindow(dpy, x, y, w, h);
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupCreateWindow");
    return true;
}

void Platform_EditPopupShowWindow(Popup* self) {
    PROFILE_START(timer);
    Display* dpy = ((WindowManager*)self->manager)->display;
    XMapWindow(dpy, self->handle.xid);
    XFlush(dpy);
    XSync(dpy, False);
    {
        int (*old)(Display*, XErrorEvent*) = XSetErrorHandler(x11_error_handler);
        XSetInputFocus(dpy, self->handle.xid, RevertToParent, CurrentTime);
        XSync(dpy, False);
        XSetErrorHandler(old);
    }
    XFlush(dpy);
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupShowWindow");
}

void Platform_EditPopupHideWindow(Popup* self) {
    PROFILE_START(timer);
    Display* dpy = get_display();
    if (dpy) { XUnmapWindow(dpy, self->handle.xid); XFlush(dpy); }
    WindowManager* wm = WindowManager_Get();
    if (wm) {
        Popup* top = WindowManager_GetTopPopup(wm);
        if (top) {
            Display* d2 = get_display();
            if (d2) {
                XSetInputFocus(d2, top->handle.xid, RevertToParent, CurrentTime);
                XFlush(d2);
                if (top->vtable && top->vtable->on_render) top->vtable->on_render(top);
                XFlush(d2);
            }
        }
    }
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupHideWindow");
}

void Platform_EditPopupRender(Popup* self) {
    PROFILE_START(timer);
    WMEditPopupData* data = (WMEditPopupData*)self;
    Display* dpy = get_display();
    if (!dpy) { PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupRender"); return; }
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, self->handle.xid, &attrs)) { PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupRender"); return; }
    Visual* vis = attrs.visual;
    int depth = attrs.depth;
    Colormap cmap = attrs.colormap;
    Pixmap dbl = XCreatePixmap(dpy, self->handle.xid, data->w, data->h, depth);
    XftDraw* xd = dbl ? XftDrawCreate(dpy, dbl, vis, cmap) : NULL;
    bool buffered = (dbl && xd);
    if (buffered) {
        Linux_SetDblVisual(dpy, vis, cmap, depth);
        Pixmap bg = Linux_GetCachedBackground(dpy, self->handle.xid, data->w, data->h);
        if (bg) {
            GC gc = XCreateGC(dpy, dbl, 0, NULL);
            XCopyArea(dpy, bg, dbl, gc, 0, 0, data->w, data->h, 0, 0);
            XFreeGC(dpy, gc);
        } else {
            Linux_DrawBackgroundToDrawable(dpy, dbl, data->w, data->h);
        }
    } else {
        if (dbl) { XFreePixmap(dpy, dbl); dbl = None; }
        Linux_DrawBackground(dpy, self->handle.xid, data->w, data->h);
        xd = Linux_CreateXftDraw(dpy, self->handle.xid);
        if (!xd) { PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupRender"); return; }
    }
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    XftFont* xf = FH_TO_XFT(font);
    Platform_SetDrawContext(dpy, xd, vis, cmap);
    if (xf && VimModeText) {
        Platform_DrawTextRaw(5, 5, g_vim_mode, VimModeTextColor(), font, "LEFT", 5,FontSize);
    }
    if (data->title && xf) {
        XGlyphInfo ext; XftTextExtentsUtf8(dpy, xf, (FcChar8*)data->title, strlen(data->title), &ext);
        int title_x = data->w - ext.xOff - 10;
        if (title_x < 50) title_x = 50;
        Platform_DrawTextRaw(title_x, 5, data->title, rgb_to_hex(COLOR_TEXT), font, "LEFT", title_x,FontSize);
    }
    int cursor_x = 0;
    int text_len = data->text ? strlen(data->text) : 0;
    int scroll = 0;
    int text_w = 0;
    if (xf) {
        scroll = Linux_UpdateEditScrollOffset(dpy, xf, data->text ? data->text : "", text_len, data->cursor, data->w, 0, &cursor_x);
        if (text_len > 0) {
            XGlyphInfo textExt;
            XftTextExtentsUtf8(dpy, xf, (FcChar8*)data->text, text_len, &textExt);
            text_w = textExt.xOff;
        }
    }
    int draw_x = 10 - scroll;
    int cursor_draw_x = 10 + cursor_x - scroll;
    int draw_y = data->h / 2 - 10;
    if (text_w > 0 && text_w <= data->w - 20 && data->align) {
        if (strcmp(data->align, "CENTER") == 0) {
            draw_x = (data->w - text_w) / 2;
            cursor_draw_x = draw_x + cursor_x;
        } else if (strcmp(data->align, "RIGHT") == 0) {
            draw_x = data->w - 10 - text_w;
            cursor_draw_x = draw_x + cursor_x;
        }
    }

    if (text_len > 0) {
        Platform_DrawTextRaw(draw_x, draw_y, data->text, rgb_to_hex(COLOR_TEXT), font, "", 0, 0);
    }
    {
        if (cursor_draw_x < 2) cursor_draw_x = 2;
        if (cursor_draw_x > data->w - 4) cursor_draw_x = data->w - 4;
        int textH = xf ? xf->ascent + xf->descent : FontSize;
        int cursor_y = draw_y + (textH - FontSize) / 2;
        if (IsInsertMode()) {
            Platform_DrawRectRaw(cursor_draw_x, cursor_y, 2, FontSize, rgb_to_hex(COLOR_TEXT));
        } else {
            Platform_DrawRectRaw(cursor_draw_x, cursor_y, 12, FontSize, rgb_to_hex(COLOR_TEXT));
        }
    }
    Platform_SetDrawContext(NULL, NULL, NULL, 0);
    if (buffered) {
        Linux_ClearDblVisual();
        GC gc = XCreateGC(dpy, self->handle.xid, 0, NULL);
        XCopyArea(dpy, dbl, self->handle.xid, gc, 0, 0, data->w, data->h, 0, 0);
        XFreeGC(dpy, gc);
    }
    XftDrawDestroy(xd);
    if (dbl) XFreePixmap(dpy, dbl);
    Platform_DestroyFont(font);
    XFlush(dpy);
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupRender");
}

void Platform_EditPopupDestroyWindow(Popup* self) {
    PROFILE_START(timer);
    Display* dpy = get_display();
    if (dpy) XDestroyWindow(dpy, self->handle.xid);
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupDestroyWindow");
}
