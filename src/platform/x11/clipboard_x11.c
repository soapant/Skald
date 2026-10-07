#include "platform/x11/clipboard_x11.h"
#include "platform/x11/display_x11.h"
#include "deps/log.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>

#define CLIP_INCR_THRESHOLD (256u * 1024u)
#define CLIP_INCR_CHUNK (64u * 1024u)
#define CLIP_READ_TRIES 40

static volatile long g_cl_seq = 0;
static long g_cl_reads = 0;
static long g_cl_read_fail = 0;
static long g_cl_selclear = 0;

long g_cl_selreq = 0;

static long cl_ms_since(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static long g_cl_t0 = 0;

static void cl_trace(const char *what, const char *fmt, ...) {
    va_list ap;
    long n = ++g_cl_seq;
    char buf[512];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
}

static Window g_clip_win = None;
static Display *g_clip_rd_dpy = NULL;
static Window g_clip_rd_win = None;
static Atom g_clip_rd_prop = None;
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

void x11_clipboard_ensure_started(void) {
    if (g_cl_t0 == 0) g_cl_t0 = cl_ms_since();
    Display *dpy = get_display();
    if (!dpy) {
        return;
    }
    if (g_clip_win != None) return;
    Window root = DefaultRootWindow(dpy);
    XSetWindowAttributes attrs = {0};
    attrs.override_redirect = True;
    Window win = XCreateWindow(dpy, root, 0, 0, 1, 1, 0, 0, InputOutput, CopyFromParent, CWOverrideRedirect, &attrs);
    if (!win) {
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
}

static void x11_clipboard_abort_incr(void) {
    g_incr_active = false;
    g_incr_req = None;
    g_incr_prop = None;
    g_incr_off = 0;
}

static void x11_clipboard_serve_request(Display *dpy, XSelectionRequestEvent *req) {
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
    cl_trace("SERVE", "requestor=%lu sel=%s target=%s(%lu) prop_req=%lu time=%lu text=%s png=%d",
        (unsigned long)req->requestor,
        req->selection == g_clip_clipboard ? "CLIPBOARD" : (req->selection == g_clip_primary ? "PRIMARY" : "other"),
        tname ? tname : "?", (unsigned long)req->target, (unsigned long)req->property,
        (unsigned long)req->time, g_clip_text ? "yes" : "no", (int)g_clip_png_len);
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
            x11_clipboard_abort_incr();
            XSelectInput(dpy, req->requestor, PropertyChangeMask);
            long total = (long)g_clip_png_len;
            XChangeProperty(dpy, req->requestor, prop, g_clip_incr, 32, PropModeReplace, (unsigned char*)&total, 1);
            XFlush(dpy);
            g_incr_active = true;
            g_incr_req = req->requestor;
            g_incr_prop = prop;
            g_incr_off = 0;
            
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
        
    }
    XSendEvent(dpy, req->requestor, False, 0, &resp);
    XFlush(dpy);
    char *tname2 = XGetAtomName(dpy, req->target);
    cl_trace("SERVE", "replied requestor=%lu target=%s prop_given=%s refused=%d",
        (unsigned long)req->requestor, tname2 ? tname2 : "?", resp.xselection.property != None ? "yes" : "NONE",
        resp.xselection.property == None ? 1 : 0);
    if (tname2) XFree(tname2);
}

static void x11_clipboard_serve_incr(Display *dpy, XPropertyEvent *pev) {
    if (!g_incr_active || !g_clip_png) {
        x11_clipboard_abort_incr();
        return;
    }
    if (pev->window != g_incr_req || pev->atom != g_incr_prop || pev->state != PropertyDelete) return;
    size_t remaining = g_clip_png_len - g_incr_off;
    if (remaining > 0) {
        size_t n = remaining > CLIP_INCR_CHUNK ? CLIP_INCR_CHUNK : remaining;
        XChangeProperty(dpy, g_incr_req, g_incr_prop, g_clip_png_atom, 8, PropModeReplace, g_clip_png + g_incr_off, (int)n);
        g_incr_off += n;
        
    } else {
        if (g_clip_png_len > 0 && (g_clip_png_len % CLIP_INCR_CHUNK) == 0) {
            XChangeProperty(dpy, g_incr_req, g_incr_prop, g_clip_png_atom, 8, PropModeReplace, NULL, 0);
        }
        
        x11_clipboard_abort_incr();
    }
    XFlush(dpy);
}

bool x11_clipboard_handle_event(Display *dpy, XEvent *ev) {
    if (!dpy || !ev) return false;
    if (ev->type == SelectionRequest) {
        g_cl_selreq++;
        cl_trace("EVENT", "SelectionRequest n=%ld requestor=%lu sel=%lu target=%lu prop=%lu time=%lu",
            g_cl_selreq, (unsigned long)ev->xselectionrequest.requestor,
            (unsigned long)ev->xselectionrequest.selection, (unsigned long)ev->xselectionrequest.target,
            (unsigned long)ev->xselectionrequest.property, (unsigned long)ev->xselectionrequest.time);
        x11_clipboard_serve_request(dpy, &ev->xselectionrequest);
        return true;
    }
    if (ev->type == PropertyNotify) {
        if (g_incr_active) {
            x11_clipboard_serve_incr(dpy, &ev->xproperty);
            return true;
        }
        return false;
    }
    if (ev->type == SelectionClear) {
        g_cl_selclear++;
        Window new_owner = XGetSelectionOwner(dpy, ev->xselectionclear.selection);
        cl_trace("EVENT", "SelectionClear n=%ld sel=%lu new_owner=%lu had_text=%d had_png=%d",
            g_cl_selclear, (unsigned long)ev->xselectionclear.selection, (unsigned long)new_owner,
            g_clip_text ? 1 : 0, (int)g_clip_png_len);
        if (ev->xselectionclear.selection == g_clip_clipboard || ev->xselectionclear.selection == g_clip_primary) {
            char *sname = XGetAtomName(dpy, ev->xselectionclear.selection);
            
            if (sname) XFree(sname);
            x11_clipboard_abort_incr();
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

static int x11_clipboard_take_ownership(Display *dpy) {
    Window prev = XGetSelectionOwner(dpy, g_clip_clipboard);
    cl_trace("OWN", "before prev_clipboard_owner=%lu our_win=%lu", (unsigned long)prev, (unsigned long)g_clip_win);
    XSetSelectionOwner(dpy, g_clip_clipboard, g_clip_win, CurrentTime);
    XSetSelectionOwner(dpy, g_clip_primary, g_clip_win, CurrentTime);
    XFlush(dpy);
    Window owner = XGetSelectionOwner(dpy, g_clip_clipboard);
    cl_trace("OWN", "after owner=%lu we_own=%d", (unsigned long)owner, owner == g_clip_win ? 1 : 0);
    
    return owner == g_clip_win;
}

int x11_clipboard_set_text(const char *text) {
    if (!text) return 0;
    cl_trace("SETTEXT", "begin len=%d text='%s'", (int)strlen(text), text);
    x11_clipboard_ensure_started();
    Display *dpy = get_display();
    if (!dpy || g_clip_win == None) { cl_trace("SETTEXT", "no display/win"); return 0; }
    char* nt = strdup(text);
    if (!nt) return 0;
    free(g_clip_text);
    g_clip_text = nt;
    free(g_clip_png);
    g_clip_png = NULL;
    g_clip_png_len = 0;
    XStoreBytes(dpy, text, (int)strlen(text));
    XFlush(dpy);
    int r = x11_clipboard_take_ownership(dpy);
    cl_trace("SETTEXT", "end ret=%d", r);
    return r;
}

int x11_clipboard_set_image(const unsigned char *png, size_t len) {
    if (!png || len == 0) return 0;
    
    x11_clipboard_ensure_started();
    Display *dpy = get_display();
    if (!dpy || g_clip_win == None) return 0;
    x11_clipboard_abort_incr();
    unsigned char* np = malloc(len);
    if (!np) return 0;
    memcpy(np, png, len);
    free(g_clip_png);
    g_clip_png = np;
    g_clip_png_len = len;
    free(g_clip_text);
    g_clip_text = NULL;
    return x11_clipboard_take_ownership(dpy);
}

void x11_clipboard_shutdown(void) {
    Display *dpy = get_display();
    if (dpy && g_clip_win != None) XDestroyWindow(dpy, g_clip_win);
    g_clip_win = None;
    if (g_clip_rd_dpy) {
        XCloseDisplay(g_clip_rd_dpy);
        g_clip_rd_dpy = NULL;
        g_clip_rd_win = None;
        g_clip_rd_prop = None;
    }
    x11_clipboard_abort_incr();
    free(g_clip_text);
    g_clip_text = NULL;
    free(g_clip_png);
    g_clip_png = NULL;
    g_clip_png_len = 0;
    
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

static Display *x11_clipboard_read_display(void) {
    if (g_clip_rd_dpy) return g_clip_rd_dpy;
    Display *d = XOpenDisplay(NULL);
    if (!d) {
        Display *main_dpy = get_display();
        if (main_dpy) {
            char *ds = XDisplayString(main_dpy);
            if (ds) {
                d = XOpenDisplay(ds);
                XFree(ds);
            }
        }
    }
    if (!d) {
        
        return NULL;
    }
    Window root = DefaultRootWindow(d);
    XSetWindowAttributes attrs = {0};
    attrs.override_redirect = True;
    g_clip_rd_win = XCreateWindow(d, root, 0, 0, 1, 1, 0, 0, InputOutput, CopyFromParent, CWOverrideRedirect, &attrs);
    g_clip_rd_prop = XInternAtom(d, "FLINT_CLIP_READ", False);
    g_clip_incr = XInternAtom(d, "INCR", False);
    XFlush(d);
    g_clip_rd_dpy = d;
    
    return g_clip_rd_dpy;
}

static char *ClipReadTarget(Atom target) {
    Display *dpy = x11_clipboard_read_display();
    if (!dpy || g_clip_rd_win == None) { cl_trace("READ", "no read display/win"); return NULL; }
    Window win = g_clip_rd_win;
    Atom prop = g_clip_rd_prop;
    Window sel_owner = XGetSelectionOwner(dpy, g_clip_clipboard);
    char *tname = XGetAtomName(dpy, target);
    char tbuf[64] = {0};
    if (tname) { strncpy(tbuf, tname, sizeof(tbuf) - 1); XFree(tname); tname = NULL; }
    XSelectInput(dpy, win, PropertyChangeMask);
    cl_trace("READ", "ConvertSelection target=%s sel_owner=%lu (our_win=%lu same=%d) win=%lu prop=%lu",
        tbuf, (unsigned long)sel_owner, (unsigned long)g_clip_win,
        sel_owner == g_clip_win ? 1 : 0, (unsigned long)win, (unsigned long)prop);
    XConvertSelection(dpy, g_clip_clipboard, target, prop, win, CurrentTime);
    XFlush(dpy);
    ClipReadWait w = { win, g_clip_clipboard, prop };
    XEvent ev;
    if (!ClipReadWaitEvent(dpy, &ev, ClipReadNotifyPred, &w, CLIP_READ_TRIES)) {
        cl_trace("READ", "TIMEOUT waiting SelectionNotify after %d*5ms target=%s sel_owner=%lu",
            CLIP_READ_TRIES, tbuf, (unsigned long)sel_owner);
        return NULL;
    }
    if (ev.xselection.property == None) {
        cl_trace("READ", "REFUSED property=None target=%lu sel_owner=%lu time=%lu",
            (unsigned long)target, (unsigned long)sel_owner, (unsigned long)ev.xselection.time);
        return NULL;
    }
    cl_trace("READ", "got SelectionNotify property=%lu waiting for data", (unsigned long)ev.xselection.property);
    Atom actual = None;
    int format = 0;
    unsigned long nitems = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;
    if (XGetWindowProperty(dpy, win, prop, 0, 1, False, AnyPropertyType, &actual, &format, &nitems, &after, &data) != Success) {
        cl_trace("READ", "XGetWindowProperty FAILED");
        return NULL;
    }
    if (data) XFree(data);
    if (actual == g_clip_incr) {
        cl_trace("READ", "INCR detected, draining");
        XDeleteProperty(dpy, win, prop);
        XFlush(dpy);
        return ClipReadIncr(dpy, win, prop, &w);
    }
    char *res = ClipReadProperty(dpy, win, prop, NULL);
    cl_trace("READ", "result target=%lu len=%d", (unsigned long)target, res ? (int)strlen(res) : -1);
    return res;
}

char *x11_clipboard_get_text(void) {
    g_cl_reads++;
    x11_clipboard_ensure_started();
    Display *dpy = get_display();
    if (!dpy || g_clip_win == None) { cl_trace("GETTEXT", "no display, read#%ld", g_cl_reads); return NULL; }
    if (XGetSelectionOwner(dpy, g_clip_clipboard) == g_clip_win && g_clip_text) {
        cl_trace("GETTEXT", "read#%ld served from own memory len=%d", g_cl_reads, (int)strlen(g_clip_text));
        return strdup(g_clip_text);
    }
    char *out = ClipReadTarget(g_clip_utf8);
    if (!out) {
        out = ClipReadTarget(XA_STRING);
    }
    if (!out) g_cl_read_fail++;
    cl_trace("GETTEXT", "read#%ld fail_total=%ld len=%d", g_cl_reads, g_cl_read_fail, out ? (int)strlen(out) : -1);
    return out;
}

#include "platform/x11/clipboard_backend_x11.h"

bool Platform_ClipboardAvailable(void) {
    return true;
}

int Platform_ClipboardSetText(const char *text) {
    return x11_clipboard_set_text(text);
}

int Platform_ClipboardSetImage(const unsigned char *png, size_t len) {
    return x11_clipboard_set_image(png, len);
}

char* Platform_ClipboardGetText(void) {
    return x11_clipboard_get_text();
}

void Platform_ClipboardShutdown(void) {
    x11_clipboard_shutdown();
}
