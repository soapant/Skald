#include "platform/x11/clipboard_x11.h"
#include "platform/x11/display_x11.h"
#include "deps/log.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define CLIP_INCR_THRESHOLD (256u * 1024u)
#define CLIP_INCR_CHUNK (64u * 1024u)

static Window g_clip_win = None;
static char *g_clip_text = NULL;
static unsigned char *g_clip_png = NULL;
static size_t g_clip_png_len = 0;
static Atom g_clip_clipboard = None;
static Atom g_clip_primary = None;
static Atom g_clip_targets = None;
static Atom g_clip_utf8 = None;
static Atom g_clip_timestamp = None;
static Atom g_clip_png_atom = None;
static Atom g_clip_incr = None;

static bool g_incr_active = false;
static Window g_incr_req = None;
static Atom g_incr_prop = None;
static size_t g_incr_off = 0;

void Linux_ClipboardEnsureStarted(void) {
    Display *dpy = get_display();
    if (!dpy) {
        log_error("clipboard no main display");
        return;
    }
    if (g_clip_win != None) return;
    Window root = DefaultRootWindow(dpy);
    XSetWindowAttributes attrs = {0};
    attrs.override_redirect = True;
    Window win = XCreateWindow(dpy, root, 0, 0, 1, 1, 0, 0, InputOutput, CopyFromParent, CWOverrideRedirect, &attrs);
    if (!win) {
        log_error("clipboard hidden window create failed");
        return;
    }
    g_clip_clipboard = XInternAtom(dpy, "CLIPBOARD", False);
    g_clip_primary = XInternAtom(dpy, "PRIMARY", False);
    g_clip_targets = XInternAtom(dpy, "TARGETS", False);
    g_clip_utf8 = XInternAtom(dpy, "UTF8_STRING", False);
    g_clip_timestamp = XInternAtom(dpy, "TIMESTAMP", False);
    g_clip_png_atom = XInternAtom(dpy, "image/png", False);
    g_clip_incr = XInternAtom(dpy, "INCR", False);
    XMapWindow(dpy, win);
    XFlush(dpy);
    g_clip_win = win;
    log_info("clipboard owner window started win=%lu", (unsigned long)win);
}

static void Linux_ClipboardAbortIncr(void) {
    g_incr_active = false;
    g_incr_req = None;
    g_incr_prop = None;
    g_incr_off = 0;
}

static void Linux_ClipboardServeRequest(Display *dpy, XSelectionRequestEvent *req) {
    XEvent resp = {0};
    resp.xselection.type = SelectionNotify;
    resp.xselection.display = req->display;
    resp.xselection.requestor = req->requestor;
    resp.xselection.selection = req->selection;
    resp.xselection.target = req->target;
    resp.xselection.property = None;
    resp.xselection.time = req->time;
    Atom prop = req->property == None ? req->target : req->property;
    if (req->selection != g_clip_clipboard && req->selection != g_clip_primary) {
        XSendEvent(dpy, req->requestor, False, 0, &resp);
        XFlush(dpy);
        return;
    }
    char *tname = XGetAtomName(dpy, req->target);
    log_info("clipboard serve target=%s(%lu) prop=%lu text=%s png=%d", tname ? tname : "?",
        (unsigned long)req->target, (unsigned long)prop, g_clip_text ? "yes" : "no", (int)g_clip_png_len);
    if (tname) XFree(tname);
    if (req->target == g_clip_targets) {
        Atom list[8];
        int n = 0;
        list[n++] = g_clip_targets;
        list[n++] = g_clip_timestamp;
        if (g_clip_text) {
            list[n++] = g_clip_utf8;
            list[n++] = XA_STRING;
        }
        if (g_clip_png) list[n++] = g_clip_png_atom;
        XChangeProperty(dpy, req->requestor, prop, XA_ATOM, 32, PropModeReplace, (unsigned char*)list, n);
        resp.xselection.property = prop;
    } else if ((req->target == g_clip_utf8 || req->target == XA_STRING) && g_clip_text) {
        Atom type = (req->target == g_clip_utf8) ? g_clip_utf8 : XA_STRING;
        XChangeProperty(dpy, req->requestor, prop, type, 8, PropModeReplace, (unsigned char*)g_clip_text, strlen(g_clip_text));
        resp.xselection.property = prop;
    } else if (req->target == g_clip_png_atom && g_clip_png) {
        if (g_clip_png_len > CLIP_INCR_THRESHOLD) {
            Linux_ClipboardAbortIncr();
            XSelectInput(dpy, req->requestor, PropertyChangeMask);
            long total = (long)g_clip_png_len;
            XChangeProperty(dpy, req->requestor, prop, g_clip_incr, 32, PropModeReplace, (unsigned char*)&total, 1);
            XFlush(dpy);
            g_incr_active = true;
            g_incr_req = req->requestor;
            g_incr_prop = prop;
            g_incr_off = 0;
            log_info("clipboard INCR start total=%d", (int)g_clip_png_len);
            resp.xselection.property = prop;
        } else {
            XChangeProperty(dpy, req->requestor, prop, g_clip_png_atom, 8, PropModeReplace, g_clip_png, (int)g_clip_png_len);
            resp.xselection.property = prop;
        }
    } else if (req->target == g_clip_timestamp) {
        unsigned long tm = (unsigned long)req->time;
        XChangeProperty(dpy, req->requestor, prop, XA_INTEGER, 32, PropModeReplace, (unsigned char*)&tm, 1);
        resp.xselection.property = prop;
    } else {
        log_info("clipboard unsupported target=%lu", (unsigned long)req->target);
    }
    XSendEvent(dpy, req->requestor, False, 0, &resp);
    XFlush(dpy);
}

static void Linux_ClipboardServeIncr(Display *dpy, XPropertyEvent *pev) {
    if (!g_incr_active || !g_clip_png) {
        Linux_ClipboardAbortIncr();
        return;
    }
    if (pev->window != g_incr_req || pev->atom != g_incr_prop || pev->state != PropertyDelete) return;
    size_t remaining = g_clip_png_len - g_incr_off;
    if (remaining > 0) {
        size_t n = remaining > CLIP_INCR_CHUNK ? CLIP_INCR_CHUNK : remaining;
        XChangeProperty(dpy, g_incr_req, g_incr_prop, g_clip_png_atom, 8, PropModeReplace, g_clip_png + g_incr_off, (int)n);
        g_incr_off += n;
        log_info("clipboard INCR chunk off=%d/%d", (int)g_incr_off, (int)g_clip_png_len);
    } else {
        if (g_clip_png_len > 0 && (g_clip_png_len % CLIP_INCR_CHUNK) == 0) {
            XChangeProperty(dpy, g_incr_req, g_incr_prop, g_clip_png_atom, 8, PropModeReplace, NULL, 0);
        }
        log_info("clipboard INCR done");
        Linux_ClipboardAbortIncr();
    }
    XFlush(dpy);
}

bool Linux_ClipboardHandleEvent(Display *dpy, XEvent *ev) {
    if (!dpy || !ev) return false;
    if (ev->type == SelectionRequest) {
        log_info("clipboard got SelectionRequest");
        Linux_ClipboardServeRequest(dpy, &ev->xselectionrequest);
        return true;
    }
    if (ev->type == PropertyNotify) {
        if (g_incr_active) {
            Linux_ClipboardServeIncr(dpy, &ev->xproperty);
            return true;
        }
        return false;
    }
    if (ev->type == SelectionClear) {
        if (ev->xselectionclear.selection == g_clip_clipboard || ev->xselectionclear.selection == g_clip_primary) {
            char *sname = XGetAtomName(dpy, ev->xselectionclear.selection);
            log_info("clipboard lost ownership sel=%s", sname ? sname : "?");
            if (sname) XFree(sname);
            Linux_ClipboardAbortIncr();
            free(g_clip_text);
            g_clip_text = NULL;
            free(g_clip_png);
            g_clip_png = NULL;
            g_clip_png_len = 0;
            return true;
        }
    }
    return false;
}

static int Linux_ClipboardTakeOwnership(Display *dpy) {
    XSetSelectionOwner(dpy, g_clip_clipboard, g_clip_win, CurrentTime);
    XSetSelectionOwner(dpy, g_clip_primary, g_clip_win, CurrentTime);
    XFlush(dpy);
    Window owner = XGetSelectionOwner(dpy, g_clip_clipboard);
    log_info("clipboard owner=%lu win=%lu", (unsigned long)owner, (unsigned long)g_clip_win);
    return owner == g_clip_win;
}

int Linux_ClipboardSetText(const char *text) {
    if (!text) return 0;
    log_info("clipboard set req text='%s'", text);
    Linux_ClipboardEnsureStarted();
    Display *dpy = get_display();
    if (!dpy || g_clip_win == None) return 0;
    free(g_clip_text);
    g_clip_text = strdup(text);
    if (!g_clip_text) return 0;
    XStoreBytes(dpy, text, (int)strlen(text));
    XFlush(dpy);
    return Linux_ClipboardTakeOwnership(dpy);
}

int Linux_ClipboardSetImage(const unsigned char *png, size_t len) {
    if (!png || len == 0) return 0;
    log_info("clipboard set req png len=%d", (int)len);
    Linux_ClipboardEnsureStarted();
    Display *dpy = get_display();
    if (!dpy || g_clip_win == None) return 0;
    Linux_ClipboardAbortIncr();
    free(g_clip_png);
    g_clip_png = malloc(len);
    if (!g_clip_png) return 0;
    memcpy(g_clip_png, png, len);
    g_clip_png_len = len;
    return Linux_ClipboardTakeOwnership(dpy);
}

void Linux_ClipboardShutdown(void) {
    Display *dpy = get_display();
    if (dpy && g_clip_win != None) XDestroyWindow(dpy, g_clip_win);
    g_clip_win = None;
    Linux_ClipboardAbortIncr();
    free(g_clip_text);
    g_clip_text = NULL;
    free(g_clip_png);
    g_clip_png = NULL;
    g_clip_png_len = 0;
    log_info("clipboard owner stopped");
}

typedef struct {
    Window win;
    Atom selection;
    Atom property;
} ClipReadWait;

static Bool ClipReadNotifyPred(Display *dpy, XEvent *ev, XPointer arg) {
    (void)dpy;
    ClipReadWait *w = (ClipReadWait *)arg;
    if (ev->type == SelectionNotify)
        return ev->xselection.requestor == w->win && ev->xselection.selection == w->selection;
    return False;
}

static Bool ClipReadPropPred(Display *dpy, XEvent *ev, XPointer arg) {
    (void)dpy;
    ClipReadWait *w = (ClipReadWait *)arg;
    if (ev->type == PropertyNotify)
        return ev->xproperty.window == w->win && ev->xproperty.atom == w->property && ev->xproperty.state == PropertyNewValue;
    return False;
}

static int ClipReadWaitEvent(Display *dpy, XEvent *ev, Bool (*pred)(Display *, XEvent *, XPointer), ClipReadWait *w, int tries) {
    for (int i = 0; i < tries; i++) {
        if (XCheckIfEvent(dpy, ev, pred, (XPointer)w)) return 1;
        usleep(5000);
    }
    return 0;
}

static char *ClipReadProperty(Display *dpy, Window win, Atom prop, size_t *out_len) {
    Atom actual = None;
    int format = 0;
    unsigned long nitems = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;
    if (XGetWindowProperty(dpy, win, prop, 0, (~0L), True, AnyPropertyType, &actual, &format, &nitems, &after, &data) != Success)
        return NULL;
    if (actual == g_clip_incr) {
        if (data) XFree(data);
        return NULL;
    }
    if (!data) return NULL;
    char *out = malloc(nitems + 1);
    if (!out) {
        XFree(data);
        return NULL;
    }
    memcpy(out, data, nitems);
    out[nitems] = '\0';
    if (out_len) *out_len = (size_t)nitems;
    XFree(data);
    return out;
}

static char *ClipReadIncr(Display *dpy, Window win, Atom prop, ClipReadWait *w) {
    size_t cap = 65536;
    size_t len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    XEvent ev;
    for (;;) {
        if (!ClipReadWaitEvent(dpy, &ev, ClipReadPropPred, w, 400)) break;
        Atom actual = None;
        int format = 0;
        unsigned long nitems = 0;
        unsigned long after = 0;
        unsigned char *data = NULL;
        if (XGetWindowProperty(dpy, win, prop, 0, (~0L), True, AnyPropertyType, &actual, &format, &nitems, &after, &data) != Success)
            break;
        if (!data || nitems == 0) {
            if (data) XFree(data);
            break;
        }
        if (len + nitems + 1 > cap) {
            size_t ncap = (len + nitems + 1) * 2;
            char *n = realloc(out, ncap);
            if (!n) {
                XFree(data);
                free(out);
                return NULL;
            }
            out = n;
            cap = ncap;
        }
        memcpy(out + len, data, nitems);
        len += nitems;
        XFree(data);
    }
    out[len] = '\0';
    return out;
}

static char *ClipReadTarget(Display *dpy, Window win, Atom target) {
    Atom prop = XInternAtom(dpy, "FLINT_CLIP_READ", False);
    XSelectInput(dpy, win, PropertyChangeMask);
    XConvertSelection(dpy, g_clip_clipboard, target, prop, win, CurrentTime);
    XFlush(dpy);
    ClipReadWait w = { win, g_clip_clipboard, prop };
    XEvent ev;
    if (!ClipReadWaitEvent(dpy, &ev, ClipReadNotifyPred, &w, 200)) return NULL;
    if (ev.xselection.property == None) return NULL;
    Atom actual = None;
    int format = 0;
    unsigned long nitems = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;
    if (XGetWindowProperty(dpy, win, prop, 0, 1, False, AnyPropertyType, &actual, &format, &nitems, &after, &data) != Success)
        return NULL;
    if (data) XFree(data);
    if (actual == g_clip_incr) {
        XDeleteProperty(dpy, win, prop);
        XFlush(dpy);
        return ClipReadIncr(dpy, win, prop, &w);
    }
    return ClipReadProperty(dpy, win, prop, NULL);
}

char *Linux_ClipboardGetText(void) {
    Linux_ClipboardEnsureStarted();
    Display *dpy = get_display();
    if (!dpy || g_clip_win == None) return NULL;
    if (XGetSelectionOwner(dpy, g_clip_clipboard) == g_clip_win && g_clip_text)
        return strdup(g_clip_text);
    char *out = ClipReadTarget(dpy, g_clip_win, g_clip_utf8);
    if (!out) out = ClipReadTarget(dpy, g_clip_win, XA_STRING);
    log_info("clipboard read len=%d", out ? (int)strlen(out) : -1);
    return out;
}
