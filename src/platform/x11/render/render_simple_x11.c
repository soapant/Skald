#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include "platform/x11/drawing_x11.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_file.h"
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

void DrawSimplePromptUserEditControlOnDraw(Display *dpy, XftDraw* xd, Visual* vis, Colormap cmap, SimplePromptState *state) {
    PROFILE_START(timer);
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    XftFont* xf = FH_TO_XFT(font);

    XGlyphInfo ext;
    if (xf && state->display_message)
        XftTextExtentsUtf8(dpy, xf, (FcChar8*)state->display_message, strlen(state->display_message), &ext);
    else ext.xOff = 0;
    int text_width = ext.xOff;

    Platform_SetDrawContext(dpy, xd, vis, cmap);
    Platform_DrawTextRaw(5, state->height - 40, state->window_title, rgb_to_hex(COLOR_TEXT), font, "LEFT", 5,FontSize);
    if (VimModeText) Platform_DrawTextRaw(5, state->height - 40, g_vim_mode, VimModeTextColor(), font, "RIGHT", 5,FontSize);
    int centered_x = (state->width - text_width) / 2;
    Platform_DrawTextRaw(centered_x, 70, state->display_message, rgb_to_hex(COLOR_TEXT), font, "LEFT", centered_x,FontSize);

    int cursor_x = 0;
    int text_len = strlen(state->user_input);
    int scroll = Linux_UpdateEditScrollOffset(dpy, xf, state->user_input, text_len, state->cursor, state->width, state->scroll_offset, &cursor_x);
    state->scroll_offset = scroll;
    int draw_x = 14 - scroll;
    if (text_len > 0) {
        Platform_DrawTextRaw(draw_x, 20, state->user_input, rgb_to_hex(COLOR_TEXT), font, "", 0, 0);
    }
    {
        int cursor_draw_x = 14 + cursor_x - scroll;
        if (cursor_draw_x < 2) cursor_draw_x = 2;
        if (cursor_draw_x > state->width - 4) cursor_draw_x = state->width - 4;
        if (IsInsertMode()) {
            Platform_DrawRectRaw(cursor_draw_x, 27, 2, FontSize, rgb_to_hex(COLOR_TEXT));
        } else {
            Platform_DrawRectRaw(cursor_draw_x, 27, 12, FontSize, rgb_to_hex(COLOR_TEXT));
        }
    }
    Platform_DestroyFont(font);
    PROFILE_END_VAR(timer, "subclasses_x11:DrawSimplePromptUserEditControlOnDraw");
}

void DrawSimplePromptUserEditControl(Display *dpy, Window *win, SimplePromptState *state) {
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
    if (!xd) { PROFILE_END_VAR(timer, "subclasses_x11:DrawSimplePromptUserEditControl"); return; }
    DrawSimplePromptUserEditControlOnDraw(dpy, xd, vis, cmap, state);
    XftDrawDestroy(xd);
    PROFILE_END_VAR(timer, "subclasses_x11:DrawSimplePromptUserEditControl");
}

bool Linux_RenderSimplePromptUser(Display *dpy, Window *win, SimplePromptState *state) {
    int w = state->width;
    int h = state->height;
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, *win, &attrs)) {
        Linux_DrawBackground(dpy, *win, w, h);
        DrawSimplePromptUserEditControl(dpy, win,state);
        XFlush(dpy);
        return true;
    }
    Visual* vis = attrs.visual;
    int depth = attrs.depth;
    Colormap cmap = attrs.colormap;
    Pixmap dbl = XCreatePixmap(dpy, *win, w, h, depth);
    if (!dbl) {
        Linux_DrawBackground(dpy, *win, w, h);
        DrawSimplePromptUserEditControl(dpy, win,state);
        XFlush(dpy);
        return true;
    }
    XftDraw* xdDbl = XftDrawCreate(dpy, dbl, vis, cmap);
    if (!xdDbl) {
        XFreePixmap(dpy, dbl);
        Linux_DrawBackground(dpy, *win, w, h);
        DrawSimplePromptUserEditControl(dpy, win,state);
        XFlush(dpy);
        return true;
    }
    Pixmap bg = Linux_GetCachedBackground(dpy, *win, w, h);
    if (bg) {
        GC gc = XCreateGC(dpy, dbl, 0, NULL);
        XCopyArea(dpy, bg, dbl, gc, 0, 0, w, h, 0, 0);
        XFreeGC(dpy, gc);
    } else {
        Linux_DrawBackgroundToDrawable(dpy, dbl, w, h);
    }
    DrawSimplePromptUserEditControlOnDraw(dpy, xdDbl, vis, cmap, state);
    GC gc = XCreateGC(dpy, *win, 0, NULL);
    XCopyArea(dpy, dbl, *win, gc, 0, 0, w, h, 0, 0);
    XFreeGC(dpy, gc);
    XftDrawDestroy(xdDbl);
    XFreePixmap(dpy, dbl);
    XFlush(dpy);
    return true;
}

