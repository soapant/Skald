#include <stdlib.h>

#include <pthread.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include "platform/x11/display_x11.h"
#include "platform/x11/drawing_x11.h"
#include "platform/x11/window_factory_x11.h"
#include "platform/platform.h"
#include "helpers/helpers_ui.h"
#include "core/timer.h"
#include "platform/wm_platform.h"
#include "platform/widget.h"
#include "platform/draw.h"
#include "platform/render_thread.h"
#include "deps/log.h"
#include "config.h"

#define MSG_MAX_ACTIVE 24
#define MSG_HOVER_DEFAULT_W 900
#define MSG_HOVER_DEFAULT_H 600

typedef struct {
    Window win;
    bool indefinite;
    char key[64];
    int w;
    int h;
} MsgSlot;

static pthread_mutex_t g_msg_lock = PTHREAD_MUTEX_INITIALIZER;
static MsgSlot g_msg_slots[MSG_MAX_ACTIVE];
static int g_msg_slot_count = 0;

static void MsgRemoveSlotLocked(Display *dpy, int idx) {
        PROFILE_START(_pt);
XDestroyWindow(dpy, g_msg_slots[idx].win);
    for (int i = idx; i < g_msg_slot_count - 1; i++) g_msg_slots[i] = g_msg_slots[i + 1];
    g_msg_slot_count--;
}

static void MsgRemoveWindowLocked(Display *dpy, Window win) {
        PROFILE_START(_pt);
for (int i = 0; i < g_msg_slot_count; i++) {
        if (g_msg_slots[i].win == win) { MsgRemoveSlotLocked(dpy, i); return; }
    }
}

static int MsgFindKeyLocked(const char *key) {
        PROFILE_START(_pt);
if (!key || !key[0]) { PROFILE_END_VAR(_pt, "MsgFindKeyLocked"); return -1; }
    for (int i = 0; i < g_msg_slot_count; i++) {
        if (strcmp(g_msg_slots[i].key, key) == 0) return i;
    }
    { PROFILE_END_VAR(_pt, "MsgFindKeyLocked"); return -1; }
}

static void MsgRemoveIndefiniteLocked(Display *dpy) {
        PROFILE_START(_pt);
for (int i = 0; i < g_msg_slot_count; ) {
        if (g_msg_slots[i].indefinite) MsgRemoveSlotLocked(dpy, i);
        else i++;
    }
}

typedef struct {
    Display *dpy;
    Window win;
    double timeout;
} MsgDismissPayload;

static void *MsgDismissThread(void *arg) {
        PROFILE_START(_pt);
MsgDismissPayload *p = (MsgDismissPayload *)arg;
    if (!p) { PROFILE_END_VAR(_pt, "MsgDismissThread"); return NULL; }
    if (p->timeout > 0) {
        struct timespec ts;
        ts.tv_sec = (time_t)p->timeout;
        ts.tv_nsec = (long)((p->timeout - (double)ts.tv_sec) * 1000000000.0);
        if (ts.tv_nsec < 0) ts.tv_nsec = 0;
        if (ts.tv_nsec > 999999999L) ts.tv_nsec = 999999999L;
        nanosleep(&ts, NULL);
    }
    Display *dpy = p->dpy;
    Window win = p->win;
    free(p);
    if (dpy && win != None) {
        pthread_mutex_lock(&g_msg_lock);
        MsgRemoveWindowLocked(dpy, win);
        pthread_mutex_unlock(&g_msg_lock);
        XFlush(dpy);
    }
    { PROFILE_END_VAR(_pt, "MsgDismissThread"); return NULL; }
}

#define MSG_MAX_LINES 64

#define MSG_AUTO_WRAP_MAX_W 900
#define MSG_AUTO_WRAP_MAX_H 600

typedef struct {
    const char *s;
    int n;
    int w;
} MsgLine;

static int MsgUtf8CharLen(unsigned char c) {
        PROFILE_START(_pt);
if ((c & 0x80) == 0) { PROFILE_END_VAR(_pt, "MsgUtf8CharLen"); return 1; }
    if ((c & 0xE0) == 0xC0) { PROFILE_END_VAR(_pt, "MsgUtf8CharLen"); return 2; }
    if ((c & 0xF0) == 0xE0) { PROFILE_END_VAR(_pt, "MsgUtf8CharLen"); return 3; }
    if ((c & 0xF8) == 0xF0) { PROFILE_END_VAR(_pt, "MsgUtf8CharLen"); return 4; }
    { PROFILE_END_VAR(_pt, "MsgUtf8CharLen"); return 1; }
}

static int MsgTextWidth(Display *dpy, XftFont *xf, const char *s, int n) {
        PROFILE_START(_pt);
if (!s || n <= 0) { PROFILE_END_VAR(_pt, "MsgTextWidth"); return 0; }
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, xf, (FcChar8 *)s, n, &ext);
    { PROFILE_END_VAR(_pt, "MsgTextWidth"); return ext.xOff; }
}

static int MsgWrapText(Display *dpy, XftFont *xf, const char *text, int maxW, MsgLine *lines, int max_lines) {
        PROFILE_START(_pt);
int count = 0;
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
    { PROFILE_END_VAR(_pt, "MsgWrapText"); return count; }
}

static void x11_display_message_draw(Display *dpy, Window win, const char *message, unsigned long color, int w, int h, bool no_bg,void (*render)(void)) {
        PROFILE_START(_pt);
XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, win, &attrs)) { PROFILE_END_VAR(_pt, "x11_display_message_draw"); return ; }

    if (!no_bg) {
        Pixmap bg = x11_get_cached_background(dpy, win, w, h);
        if (bg) {
            GC gc = XCreateGC(dpy, win, 0, NULL);
            XCopyArea(dpy, bg, win, gc, 0, 0, w, h, 0, 0);
            XFreeGC(dpy, gc);
        } else {
            x11_draw_background_to_drawable(dpy, win, w, h);
        }
    } else {
        XClearArea(dpy, win, 0, 0, w, h, False);
    }

    XftDraw *xd = XftDrawCreate(dpy, win, attrs.visual, attrs.colormap);
    if (!xd) { PROFILE_END_VAR(_pt, "x11_display_message_draw"); return ; }


    if (render != NULL) {
        Platform_SetDrawContext(dpy, xd, attrs.visual, attrs.colormap);
        render(); 
    }
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    XftFont *xf = FH_TO_XFT(font);
    if (xf && message && message[0] != '\0') {
        XftColor col;
        x11_xft_alloc_color(dpy, attrs.visual, attrs.colormap, rgb_to_hex(color), 0xFFFF, &col);
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
    Platform_DestroyFont(font);

    Platform_SetDrawContext(NULL, NULL, NULL, 0);
    XftDrawDestroy(xd);
    XFlush(dpy);
}

static void MsgPresent(Display *dpy, const char *message, int x, int y, int winW, int winH,double timeout, ColorRef color,bool no_bg,void (*render)(void)) {
        PROFILE_START(_pt);
int ax,ay,aw,ah;
    WindowManager_GetMainMonitorGeometry(&ax,&ay,&aw,&ah);
    Platform_SetTransparentWindow(no_bg);
    Window win = x11_create_display_message_window(dpy, x + ax, y + ay, winW, winH, false);
    Platform_SetTransparentWindow(false);
    if (!win) { PROFILE_END_VAR(_pt, "MsgPresent"); return ; }
    XMapWindow(dpy, win);
    XFlush(dpy);
    x11_display_message_draw(dpy, win, message, (unsigned long)color, winW, winH,no_bg,render);
    bool indefinite = !(timeout != 6969.0 && timeout > 0.0);
    pthread_mutex_lock(&g_msg_lock);
    if (g_msg_slot_count >= MSG_MAX_ACTIVE) MsgRemoveSlotLocked(dpy, 0);
    g_msg_slots[g_msg_slot_count].win = win;
    g_msg_slots[g_msg_slot_count].indefinite = indefinite;
    g_msg_slots[g_msg_slot_count].key[0] = '\0';
    g_msg_slots[g_msg_slot_count].w = winW;
    g_msg_slots[g_msg_slot_count].h = winH;
    g_msg_slot_count++;
    pthread_mutex_unlock(&g_msg_lock);
    if (!indefinite) {
        MsgDismissPayload *p = malloc(sizeof(*p));
        if (p) {
            p->dpy = dpy;
            p->win = win;
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


#define HOVER_WIDGET_MAX 8

typedef struct {
    char key[64];
    PlatformWidget *w;
    void (*render)(void *ud);
    void *ud;
    void (*release)(void *ud);
    void *release_ud;
    int fps;
    bool no_bg;
    unsigned long bg_hex;
    int x, y, w_px, h_px;
    bool dead;
} HoverWidgetSlot;

static HoverWidgetSlot g_hover_widgets[HOVER_WIDGET_MAX];
static pthread_mutex_t g_hover_widget_lock = PTHREAD_MUTEX_INITIALIZER;

static int HoverWidgetFindLocked(const char *key) {
        PROFILE_START(_pt);
for (int i = 0; i < HOVER_WIDGET_MAX; i++) {
        if (g_hover_widgets[i].w && !g_hover_widgets[i].dead && strcmp(g_hover_widgets[i].key, key) == 0) return i;
    }
    { PROFILE_END_VAR(_pt, "HoverWidgetFindLocked"); return -1; }
}

static int HoverWidgetAllocLocked(const char *key) {
        PROFILE_START(_pt);
for (int i = 0; i < HOVER_WIDGET_MAX; i++) {
        if (!g_hover_widgets[i].w) return i;
    }
    { PROFILE_END_VAR(_pt, "HoverWidgetAllocLocked"); return -1; }
}

static void HoverWidgetFrame(void *ud) {
        PROFILE_START(_pt);
HoverWidgetSlot *s = (HoverWidgetSlot *)ud;
    if (!s || !s->w || s->dead) { PROFILE_END_VAR(_pt, "HoverWidgetFrame"); return ; }
    if (s->render) s->render(s->ud);
}

void Platform_HoverWidgetHide(const char *key) {
        PROFILE_START(_pt);
if (!key || !key[0]) { PROFILE_END_VAR(_pt, "Platform_HoverWidgetHide"); return ; }
    pthread_mutex_lock(&g_hover_widget_lock);
    int idx = HoverWidgetFindLocked(key);
    if (idx < 0) { pthread_mutex_unlock(&g_hover_widget_lock); return; }
    PlatformWidget *w = g_hover_widgets[idx].w;
    void (*release)(void *) = g_hover_widgets[idx].release;
    void *release_ud = g_hover_widgets[idx].release_ud;
    memset(&g_hover_widgets[idx], 0, sizeof(g_hover_widgets[idx]));
    pthread_mutex_unlock(&g_hover_widget_lock);
    if (w) {
        Platform_Widget_SetFrameCallback(w, NULL, NULL);
        Platform_Widget_Destroy(w);
    }
    if (release) release(release_ud);
    if (!Platform_RenderThread_IsCurrentThread()) Platform_RenderThread_Sync();
}

void Platform_HoverWidgetShow(const char *key, void (*render)(void* ud), void* ud, int x, int y,
                              int w, int h, int fps, bool no_bg, unsigned long bg_hex,
                              void (*release)(void *ud), void *release_ud) {        PROFILE_START(_pt);
if (!key || !key[0] || w <= 0 || h <= 0) { PROFILE_END_VAR(_pt, "Platform_HoverWidgetShow"); return ; }

    pthread_mutex_lock(&g_hover_widget_lock);
    int idx = HoverWidgetFindLocked(key);
    if (idx >= 0) {
        HoverWidgetSlot *s = &g_hover_widgets[idx];
        void (*old_release)(void *) = s->release;
        void *old_release_ud = s->release_ud;
        s->render = render;
        s->ud = ud;
        s->release = release;
        s->release_ud = release_ud;
        bool geom_changed = (s->x != x || s->y != y || s->w_px != w || s->h_px != h);
        s->x = x; s->y = y; s->w_px = w; s->h_px = h;
        int ax = x - w / 2;
        int ay = y - h / 2;
        PlatformWidget *pw = s->w;
        void (*prev_release)(void *) = old_release;
        void *prev_release_ud = old_release_ud;
        pthread_mutex_unlock(&g_hover_widget_lock);
        if (geom_changed) {
            Platform_Widget_Resize(pw, w, h);
            Platform_Widget_Move(pw, ax, ay);
        }
        Platform_Widget_Invalidate(pw);
        if (prev_release && prev_release != release) prev_release(prev_release_ud);
        return;
    }
    idx = HoverWidgetAllocLocked(key);
    if (idx < 0) { pthread_mutex_unlock(&g_hover_widget_lock); return; }
    HoverWidgetSlot *s = &g_hover_widgets[idx];
    memset(s, 0, sizeof(*s));
    snprintf(s->key, sizeof(s->key), "%s", key);
    s->render = render;
    s->ud = ud;
    s->release = release;
    s->release_ud = release_ud;
    s->fps = fps;
    s->no_bg = no_bg;
    s->bg_hex = bg_hex;
    s->x = x; s->y = y; s->w_px = w; s->h_px = h;
    pthread_mutex_unlock(&g_hover_widget_lock);

    Platform_SetDrawBgColor(no_bg ? 0UL : bg_hex);
    PlatformWidget *pw = Platform_Widget_Create(x - w / 2, y - h / 2, w, h, fps, no_bg, NULL, key);
    Platform_SetDrawBgColor(0);
    if (!pw) {
        if (release) release(release_ud);
        return;
    }
    pthread_mutex_lock(&g_hover_widget_lock);
    if (HoverWidgetFindLocked(key) < 0) {
        s->w = pw;
        pthread_mutex_unlock(&g_hover_widget_lock);
        Platform_Widget_SetBgColor(pw, no_bg ? 0 : bg_hex);
        Platform_Widget_SetNoBg(pw, no_bg);
        Platform_Widget_SetFrameCallback(pw, HoverWidgetFrame, s);
    } else {
        pthread_mutex_unlock(&g_hover_widget_lock);
        Platform_Widget_Destroy(pw);
        if (release) release(release_ud);
    }
}

static void MsgHoverPresent(Display *dpy, const char *key, const char *message,
                            int centerX, int centerY, int winW, int winH, ColorRef color,bool no_bg) {
        PROFILE_START(_pt);
if (!dpy || !key || !key[0] || !message || !message[0]) { PROFILE_END_VAR(_pt, "MsgHoverPresent"); return ; }
    if (winW <= 0 || winH <= 0) { PROFILE_END_VAR(_pt, "MsgHoverPresent"); return ; }
    pthread_mutex_lock(&g_msg_lock);
    int idx = MsgFindKeyLocked(key);
    if (idx >= 0) {
        Window w = g_msg_slots[idx].win;
        if (g_msg_slots[idx].w != winW || g_msg_slots[idx].h != winH) {
            XMoveResizeWindow(dpy, w, centerX - winW / 2, centerY - winH / 2, winW, winH);
            g_msg_slots[idx].w = winW;
            g_msg_slots[idx].h = winH;
        }
        pthread_mutex_unlock(&g_msg_lock);
        x11_display_message_draw(dpy, w, message, (unsigned long)color, winW, winH,no_bg,NULL);
        return;
    }
    pthread_mutex_unlock(&g_msg_lock);

    Window win = x11_create_display_message_window(dpy, centerX - winW / 2, centerY - winH / 2,
                                                    winW, winH, false);
    if (win == None) { PROFILE_END_VAR(_pt, "MsgHoverPresent"); return ; }
    XMapRaised(dpy, win);
    XFlush(dpy);
    x11_display_message_draw(dpy, win, message, (unsigned long)color, winW, winH,no_bg,NULL);

    pthread_mutex_lock(&g_msg_lock);
    if (g_msg_slot_count >= MSG_MAX_ACTIVE) MsgRemoveSlotLocked(dpy, 0);
    int at = MsgFindKeyLocked(key);
    if (at < 0) at = g_msg_slot_count < MSG_MAX_ACTIVE ? g_msg_slot_count : MSG_MAX_ACTIVE - 1;
    g_msg_slots[at].win = win;
    g_msg_slots[at].indefinite = true;
    snprintf(g_msg_slots[at].key, sizeof(g_msg_slots[at].key), "%s", key);
    g_msg_slots[at].w = winW;
    g_msg_slots[at].h = winH;
    if (at >= g_msg_slot_count) g_msg_slot_count = at + 1;
    pthread_mutex_unlock(&g_msg_lock);
    XFlush(dpy);
}

void Platform_HoverMessageRelease(const char *key) {
        PROFILE_START(_pt);
Display *dpy = get_display();
    if (!dpy || !key || !key[0]) { PROFILE_END_VAR(_pt, "Platform_HoverMessageRelease"); return ; }
    pthread_mutex_lock(&g_msg_lock);
    int idx = MsgFindKeyLocked(key);
    if (idx >= 0) MsgRemoveSlotLocked(dpy, idx);
    pthread_mutex_unlock(&g_msg_lock);
    XFlush(dpy);
}

void Platform_DisplayMessage(const char* message, int centerX, int centerY, bool no_bg, int w, int h, int maxWidth, int maxHeight, double timeout, ColorRef color, void (*render)(void)) {
        PROFILE_START(_pt);
Display *dpy = get_display();
    if (!dpy) { PROFILE_END_VAR(_pt, "Platform_DisplayMessage"); return ; }

    int winW = 0;
    int winH = 0;

    if (w > 0 && h > 0) {
        winW = w;
        winH = h;
    } else {
        if (maxWidth <= 0) maxWidth = MSG_AUTO_WRAP_MAX_W;
        if (maxHeight <= 0) maxHeight = MSG_AUTO_WRAP_MAX_H;
        
        FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
        XftFont *xf = FH_TO_XFT(font);
        int textW = 0;
        int textH = 0;
        MsgLine lines[MSG_MAX_LINES];
        int nlines = 0;
        
        if (xf) {
            int wrapW = maxWidth - 30;
            if (wrapW > MSG_AUTO_WRAP_MAX_W) wrapW = MSG_AUTO_WRAP_MAX_W;
            nlines = MsgWrapText(dpy, xf, message, wrapW, lines, MSG_MAX_LINES);
            for (int i = 0; i < nlines; i++) {
                if (lines[i].w > textW) textW = lines[i].w;
            }
            textH = nlines * (xf->ascent + xf->descent);
        }
        Platform_DestroyFont(font);
        
        winW = textW + 30;
        winH = textH + 30;
        if (winW > maxWidth) winW = maxWidth;
        if (winH > maxHeight) winH = maxHeight;
    }

    int x = abs(centerX - winW / 2);
    int y = abs(centerY - winH / 2);
    MsgPresent(dpy, message, x, y, winW, winH,timeout,color,no_bg,render);
}

static void MsgMeasure(Display *dpy, const char *message, int maxWidth, int maxHeight,
                       int *outW, int *outH) {
        PROFILE_START(_pt);
*outW = 0;
    *outH = 0;
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    XftFont *xf = FH_TO_XFT(font);
    int textW = 0, textH = 0;
    MsgLine lines[MSG_MAX_LINES];
    int nlines = 0;
    if (xf) {
        int wrapW = maxWidth - 30;
        if (wrapW > MSG_AUTO_WRAP_MAX_W) wrapW = MSG_AUTO_WRAP_MAX_W;
        nlines = MsgWrapText(dpy, xf, message, wrapW, lines, MSG_MAX_LINES);
        for (int i = 0; i < nlines; i++) if (lines[i].w > textW) textW = lines[i].w;
        textH = nlines * (xf->ascent + xf->descent);
    }
    Platform_DestroyFont(font);
    int winW = textW + 30;
    int winH = textH + 30;
    if (winW > maxWidth) winW = maxWidth;
    if (winH > maxHeight) winH = maxHeight;
    *outW = winW;
    *outH = winH;
}

void Platform_HoverMessage(const char* key, const char* message, int centerX, int centerY,
                           int w, int h, int maxWidth, int maxHeight, ColorRef color,bool no_bg) {
        PROFILE_START(_pt);
Display *dpy = get_display();
    if (!dpy) { PROFILE_END_VAR(_pt, "Platform_HoverMessage"); return ; }
    if (maxWidth <= 0) maxWidth = MSG_HOVER_DEFAULT_W;
    if (maxHeight <= 0) maxHeight = MSG_HOVER_DEFAULT_H;
    int winW = 0, winH = 0;
    if (w > 0 && h > 0) {
        winW = w;
        winH = h;
    } else {
        MsgMeasure(dpy, message, maxWidth, maxHeight, &winW, &winH);
    }
    MsgHoverPresent(dpy, key, message, centerX, centerY, winW, winH, color,no_bg);
}

void Platform_DisplayMessageAnchored(const char* message, int rect_x, int rect_y, int rect_w, int rect_h, int h_slot, int v_slot, int gap, int outward, int maxWidth, int maxHeight, double timeout, ColorRef color,bool no_bg) {
        PROFILE_START(_pt);
if (!message || !message[0]) { PROFILE_END_VAR(_pt, "Platform_DisplayMessageAnchored"); return ; }
    Display *dpy = get_display();
    if (!dpy) { PROFILE_END_VAR(_pt, "Platform_DisplayMessageAnchored"); return ; }
    if (maxWidth <= 0) maxWidth = MSG_AUTO_WRAP_MAX_W;
    if (maxHeight <= 0) maxHeight = MSG_AUTO_WRAP_MAX_H;
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    XftFont *xf = FH_TO_XFT(font);
    int textW = 0;
    int textH = 0;
    MsgLine lines[MSG_MAX_LINES];
    int nlines = 0;
    if (xf) {
        int wrapW = maxWidth - 30;
        if (wrapW > MSG_AUTO_WRAP_MAX_W) wrapW = MSG_AUTO_WRAP_MAX_W;
        nlines = MsgWrapText(dpy, xf, message, wrapW, lines, MSG_MAX_LINES);
        for (int i = 0; i < nlines; i++) {
            if (lines[i].w > textW) textW = lines[i].w;
        }
        textH = nlines * (xf->ascent + xf->descent);
    }
    Platform_DestroyFont(font);
    int winW = textW + 30;
    int winH = textH + 30;
    if (winW > maxWidth) winW = maxWidth;
    if (winH > maxHeight) winH = maxHeight;
    int x;
    if (h_slot == 0) x = outward ? rect_x - gap - winW : rect_x + gap;
    else if (h_slot == 2) x = outward ? rect_x + rect_w + gap : rect_x + rect_w - gap - winW;
    else x = rect_x + (rect_w - winW) / 2;
    int y;
    if (v_slot == 0) y = outward ? rect_y - gap - winH : rect_y + gap;
    else if (v_slot == 2) y = outward ? rect_y + rect_h + gap : rect_y + rect_h - gap - winH;
    else y = rect_y + (rect_h - winH) / 2;
    int ax,ay,aw,ah;
    WindowManager_GetMainMonitorGeometry(&ax,&ay,&aw,&ah);
    MsgPresent(dpy, message, x - ax, y - ay, winW, winH, timeout, color,no_bg,NULL);
}

void Platform_DismissMessage(void) {
        PROFILE_START(_pt);
Display *dpy = get_display();
    if (!dpy) { PROFILE_END_VAR(_pt, "Platform_DismissMessage"); return ; }
    pthread_mutex_lock(&g_msg_lock);
    while (g_msg_slot_count > 0) MsgRemoveSlotLocked(dpy, 0);
    pthread_mutex_unlock(&g_msg_lock);
    XFlush(dpy);
}

void Platform_DismissIndefiniteMessage(void) {
        PROFILE_START(_pt);
Display *dpy = get_display();
    if (!dpy) { PROFILE_END_VAR(_pt, "Platform_DismissIndefiniteMessage"); return ; }
    pthread_mutex_lock(&g_msg_lock);
    MsgRemoveIndefiniteLocked(dpy);
    pthread_mutex_unlock(&g_msg_lock);
    XFlush(dpy);
}

bool Platform_MessagePopupShowWindow(Popup* self, int x, int y, int w, int h) {
        PROFILE_START(_pt);
(void)self;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "window_manager:Platform_MessagePopupShowWindow");
    { PROFILE_END_VAR(_pt, "Platform_MessagePopupShowWindow"); return true; }
}

void Platform_MessagePopupHideWindow(Popup* self) {
        PROFILE_START(_pt);
(void)self;
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "window_manager:Platform_MessagePopupHideWindow");
}

void Platform_MessagePopupRender(Popup* self) {
        PROFILE_START(_pt);
(void)self;
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "window_manager:Platform_MessagePopupRender");
}

void Platform_MessagePopupDestroyWindow(Popup* self) {
        PROFILE_START(_pt);
(void)self;
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "window_manager:Platform_MessagePopupDestroyWindow");
}

bool Platform_MessagePopupOnKey(Popup* self, const PopupKeyEvent* key) {
        PROFILE_START(_pt);
(void)self;
    (void)key;
    { PROFILE_END_VAR(_pt, "Platform_MessagePopupOnKey"); return false; }
}
