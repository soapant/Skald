#include "platform/x11/clipboard_x11.h"
#include "platform/x11/display_x11.h"
#include "deps/log.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>
#include <stdlib.h>

static Window g_clip_win = None;
static char *g_clip_text = NULL;
static Atom g_clip_clipboard = None;
static Atom g_clip_primary = None;
static Atom g_clip_targets = None;
static Atom g_clip_utf8 = None;
static Atom g_clip_timestamp = None;

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
    XMapWindow(dpy, win);
    XFlush(dpy);
    g_clip_win = win;
    log_info("clipboard owner window started win=%lu", (unsigned long)win);
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
    char *text = g_clip_text ? strdup(g_clip_text) : NULL;
    char *tname = XGetAtomName(dpy, req->target);
    char *sname = XGetAtomName(dpy, req->selection);
    log_info("clipboard serve sel=%s target=%s(%lu) prop=%lu text=%s", sname ? sname : "?", tname ? tname : "?", (unsigned long)req->target, (unsigned long)prop, text ? text : "(null)");
    if (tname) XFree(tname);
    if (sname) XFree(sname);
    if (!text) {
        XSendEvent(dpy, req->requestor, False, 0, &resp);
        XFlush(dpy);
        return;
    }
    if (req->target == g_clip_targets) {
        Atom list[] = { g_clip_targets, g_clip_utf8, XA_STRING, g_clip_timestamp };
        XChangeProperty(dpy, req->requestor, prop, XA_ATOM, 32, PropModeReplace, (unsigned char*)list, 4);
        resp.xselection.property = prop;
    } else if (req->target == g_clip_utf8 || req->target == XA_STRING) {
        Atom type = (req->target == g_clip_utf8) ? g_clip_utf8 : XA_STRING;
        XChangeProperty(dpy, req->requestor, prop, type, 8, PropModeReplace, (unsigned char*)text, strlen(text));
        resp.xselection.property = prop;
    } else if (req->target == g_clip_timestamp) {
        unsigned long tm = (unsigned long)req->time;
        XChangeProperty(dpy, req->requestor, prop, XA_INTEGER, 32, PropModeReplace, (unsigned char*)&tm, 1);
        resp.xselection.property = prop;
    } else {
        log_info("clipboard unsupported target=%lu", (unsigned long)req->target);
    }
    free(text);
    XSendEvent(dpy, req->requestor, False, 0, &resp);
    XFlush(dpy);
}

bool Linux_ClipboardHandleEvent(Display *dpy, XEvent *ev) {
    if (!dpy || !ev) return false;
    if (ev->type == SelectionRequest) {
        log_info("clipboard got SelectionRequest");
        Linux_ClipboardServeRequest(dpy, &ev->xselectionrequest);
        return true;
    }
    if (ev->type == SelectionClear) {
        if (ev->xselectionclear.selection == g_clip_clipboard || ev->xselectionclear.selection == g_clip_primary) {
            char *sname = XGetAtomName(dpy, ev->xselectionclear.selection);
            log_info("clipboard lost ownership sel=%s", sname ? sname : "?");
            if (sname) XFree(sname);
            if (ev->xselectionclear.selection == g_clip_clipboard) {
                free(g_clip_text);
                g_clip_text = NULL;
            }
            return true;
        }
    }
    return false;
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
    XSetSelectionOwner(dpy, g_clip_clipboard, g_clip_win, CurrentTime);
    XSetSelectionOwner(dpy, g_clip_primary, g_clip_win, CurrentTime);
    XStoreBytes(dpy, text, (int)strlen(text));
    XFlush(dpy);
    Window owner = XGetSelectionOwner(dpy, g_clip_clipboard);
    Window owner_p = XGetSelectionOwner(dpy, g_clip_primary);
    log_info("clipboard set text='%s' owner=%lu win=%lu primary=%lu", text, (unsigned long)owner, (unsigned long)g_clip_win, (unsigned long)owner_p);
    if (owner != g_clip_win) {
        free(g_clip_text);
        g_clip_text = NULL;
        return 0;
    }
    return 1;
}

void Linux_ClipboardShutdown(void) {
    Display *dpy = get_display();
    if (dpy && g_clip_win != None) XDestroyWindow(dpy, g_clip_win);
    g_clip_win = None;
    free(g_clip_text);
    g_clip_text = NULL;
    log_info("clipboard owner stopped");
}
