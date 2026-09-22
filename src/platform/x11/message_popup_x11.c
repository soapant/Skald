#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include "core/platform.h"
#include "platform/ui.h"
#include "platform/wm_platform.h"
#include "helpers/helpers_ui.h"
#include "core/timer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if !defined(_WIN32)
#include <pthread.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include "platform/x11/display_x11.h"
#include "platform/x11/drawing_x11.h"
#include "platform/x11/window_factory_x11.h"

static Window g_msg_win = None;
static unsigned long g_msg_gen = 0;
static bool g_msg_indefinite = false;

typedef struct {
    Display *dpy;
    Window win;
    unsigned long gen;
    int timeout;
} MsgDismissPayload;

static void *MsgDismissThread(void *arg) {
    MsgDismissPayload *p = (MsgDismissPayload *)arg;
    if (!p) return NULL;
    if (p->timeout > 0) sleep((unsigned int)p->timeout);
    Display *dpy = p->dpy;
    Window win = p->win;
    unsigned long gen = p->gen;
    free(p);
    if (dpy && win != None && gen == g_msg_gen && g_msg_win == win) {
        XDestroyWindow(dpy, win);
        XFlush(dpy);
        if (gen == g_msg_gen) { g_msg_win = None; g_msg_indefinite = false; }
    }
    return NULL;
}

#define MSG_MAX_LINES 64

typedef struct {
    const char *s;
    int n;
    int w;
} MsgLine;

static int MsgUtf8CharLen(unsigned char c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static int MsgTextWidth(Display *dpy, XftFont *xf, const char *s, int n) {
    if (!s || n <= 0) return 0;
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, xf, (FcChar8 *)s, n, &ext);
    return ext.xOff;
}

static int MsgWrapText(Display *dpy, XftFont *xf, const char *text, int maxW, MsgLine *lines, int max_lines) {
    int count = 0;
    if (!text || maxW <= 0 || !lines || max_lines <= 0) return 0;
    int spaceW = MsgTextWidth(dpy, xf, " ", 1);
    const char *p = text;
    while (*p && count < max_lines) {
        const char *nl = strchr(p, '\n');
        const char *pend = nl ? nl : p + strlen(p);
        const char *w = p;
        const char *ls = NULL;
        const char *le = NULL;
        int lineW = 0;
        while (w < pend) {
            while (w < pend && *w == ' ') w++;
            if (w >= pend) break;
            const char *ws = w;
            while (w < pend && *w != ' ') w++;
            int wn = (int)(w - ws);
            int ww = MsgTextWidth(dpy, xf, ws, wn);
            if (ww > maxW) {
                if (ls) {
                    lines[count].s = ls;
                    lines[count].n = (int)(le - ls);
                    lines[count].w = lineW;
                    count++;
                    if (count >= max_lines) return count;
                    ls = NULL;
                    le = NULL;
                    lineW = 0;
                }
                const char *c = ws;
                const char *we = ws + wn;
                while (c < we && count < max_lines) {
                    const char *t = c;
                    int best = 0;
                    int bestW = 0;
                    while (t < we) {
                        int cl = MsgUtf8CharLen((unsigned char)*t);
                        if (t + cl > we) cl = (int)(we - t);
                        int nw = MsgTextWidth(dpy, xf, c, (int)(t + cl - c));
                        if (nw > maxW) break;
                        best = (int)(t + cl - c);
                        bestW = nw;
                        t += cl;
                    }
                    if (best == 0) {
                        best = MsgUtf8CharLen((unsigned char)*c);
                        if (c + best > we) best = (int)(we - c);
                        bestW = MsgTextWidth(dpy, xf, c, best);
                    }
                    lines[count].s = c;
                    lines[count].n = best;
                    lines[count].w = bestW;
                    count++;
                    c += best;
                }
                continue;
            }
            int add = ww + (lineW > 0 ? spaceW : 0);
            if (lineW > 0 && lineW + add > maxW) {
                lines[count].s = ls;
                lines[count].n = (int)(le - ls);
                lines[count].w = lineW;
                count++;
                if (count >= max_lines) return count;
                ls = NULL;
                le = NULL;
                lineW = 0;
            }
            if (!ls) ls = ws;
            le = w;
            lineW += add;
        }
        if (ls) {
            lines[count].s = ls;
            lines[count].n = (int)(le - ls);
            lines[count].w = lineW;
            count++;
        } else if (pend == p && nl && count < max_lines) {
            lines[count].s = p;
            lines[count].n = 0;
            lines[count].w = 0;
            count++;
        }
        p = nl ? nl + 1 : pend;
    }
    return count;
}

static void Linux_DisplayMessageDraw(Display *dpy, Window win, const char *message, unsigned long color, int w, int h) {
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, win, &attrs)) return;
    Pixmap bg = Linux_GetCachedBackground(dpy, win, w, h);
    if (bg) {
        GC gc = XCreateGC(dpy, win, 0, NULL);
        XCopyArea(dpy, bg, win, gc, 0, 0, w, h, 0, 0);
        XFreeGC(dpy, gc);
    } else {
        Linux_DrawBackgroundToDrawable(dpy, win, w, h);
    }
    XftDraw *xd = XftDrawCreate(dpy, win, attrs.visual, attrs.colormap);
    if (!xd) return;
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    XftFont *xf = FH_TO_XFT(font);
    if (xf && message) {
        XftColor col;
        Linux_XftAllocColor(dpy, attrs.visual, attrs.colormap, rgb_to_hex(color), 0xFFFF, &col);
        MsgLine lines[MSG_MAX_LINES];
        int nlines = MsgWrapText(dpy, xf, message, w - 30, lines, MSG_MAX_LINES);
        int lineH = xf->ascent + xf->descent;
        int y = (h - nlines * lineH) / 2 + xf->ascent;
        if (y < xf->ascent) y = xf->ascent;
        for (int i = 0; i < nlines; i++) {
            if (y > h - 5) break;
            if (lines[i].n > 0) {
                int x = (w - lines[i].w) / 2;
                if (x < 0) x = 0;
                XftDrawStringUtf8(xd, &col, xf, x, y, (FcChar8 *)lines[i].s, lines[i].n);
            }
            y += lineH;
        }
        XftColorFree(dpy, attrs.visual, attrs.colormap, &col);
    }
    XftDrawDestroy(xd);
    Platform_DestroyFont(font);
    XFlush(dpy);
}

void Platform_DisplayMessage(const char* message, int centerX, int centerY, int maxWidth, int maxHeight, int timeout, ColorRef color) {
    if (!message || !message[0]) return;
    Display *dpy = get_display();
    if (!dpy) return;
    g_msg_gen++;
    if (g_msg_win != None) {
        XDestroyWindow(dpy, g_msg_win);
        g_msg_win = None;
    }
    if (maxWidth <= 0) maxWidth = 900;
    if (maxHeight <= 0) maxHeight = 140;
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    XftFont *xf = FH_TO_XFT(font);
    int textW = 0;
    int textH = 0;
    MsgLine lines[MSG_MAX_LINES];
    int nlines = 0;
    if (xf) {
        nlines = MsgWrapText(dpy, xf, message, maxWidth - 30, lines, MSG_MAX_LINES);
        for (int i = 0; i < nlines; i++) {
            if (lines[i].w > textW) textW = lines[i].w;
        }
        textH = nlines * (xf->ascent + xf->descent);
    }
    Platform_DestroyFont(font);
    int winW = textW + 30;
    int winH = textH + 30;
    int mox = 0, moy = 0, mow = 1920, moh = 1080;
    Platform_WM_GetMonitorGeometry(&mox, &moy, &mow, &moh);
    int cx = mox + mow / 2 + (centerX - 960);
    int cy = moy + moh / 2 + (centerY - 540);
    int x = cx - winW / 2;
    int y = cy - winH / 2;
    Window win = Linux_CreateDisplayMessageWindow(dpy, x, y, winW, winH);
    if (!win) return;
    g_msg_win = win;
    XMapWindow(dpy, win);
    XFlush(dpy);
    Linux_DisplayMessageDraw(dpy, win, message, (unsigned long)color, winW, winH);
    g_msg_indefinite = !(timeout != 6969 && timeout > 0);
    if (timeout != 6969 && timeout > 0) {
        MsgDismissPayload *p = malloc(sizeof(*p));
        if (p) {
            p->dpy = dpy;
            p->win = win;
            p->gen = g_msg_gen;
            p->timeout = timeout;
            pthread_t thr;
            if (pthread_create(&thr, NULL, MsgDismissThread, p) == 0) {
                pthread_detach(thr);
            } else {
                free(p);
            }
        }
    }
}

void Platform_DismissMessage(void) {
    g_msg_gen++;
    g_msg_indefinite = false;
    Display *dpy = get_display();
    if (!dpy || g_msg_win == None) return;
    XDestroyWindow(dpy, g_msg_win);
    g_msg_win = None;
    XFlush(dpy);
}

void Platform_DismissIndefiniteMessage(void) {
    if (!g_msg_indefinite || g_msg_win == None) return;
    Platform_DismissMessage();
}
#endif
