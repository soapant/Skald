#include "wm/x11_wm.h"
#include "deps/log.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xrandr.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include "platform/platform.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "platform/x11/display_x11.h"

static int _x11_err = 0;
static Window _x11_last_min = None;
static int _x11_err_handler(Display *dpy, XErrorEvent *ev) {
  (void)dpy; (void)ev; _x11_err = 1; return 0; 
}

static char* x11_get_text_prop(Display* dpy, Window w) {
    if (!dpy || w == None) return NULL;
    Atom utf8 = XInternAtom(dpy, "UTF8_STRING", False);
    Atom net_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom wm_name = XInternAtom(dpy, "WM_NAME", False);
    char* out = NULL;
    for (int pass = 0; pass < 2 && !out; pass++) {
        Atom prop = (pass == 0) ? net_name : wm_name;
        Atom want = (pass == 0) ? utf8 : AnyPropertyType;
        Atom type = None; int fmt = 0;
        unsigned long nitems = 0, after = 0; unsigned char* data = NULL;
        if (XGetWindowProperty(dpy, w, prop, 0, 65536, False, want,
                               &type, &fmt, &nitems, &after, &data) != Success) continue;
        if (data) {
            if (type != None && nitems > 0) {
                out = (char*)malloc(nitems + 1);
                if (out) { memcpy(out, data, nitems); out[nitems] = '\0'; }
            }
            XFree(data);
        }
    }
    return out;
}

static char* x11_get_res_class(Display* dpy, Window w) {
    if (!dpy || w == None) return NULL;
    XClassHint* ch = XAllocClassHint();
    if (!ch) return NULL;
    char* out = NULL;
    _x11_err = 0;
    XErrorHandler old = XSetErrorHandler(_x11_err_handler);
    int ok = XGetClassHint(dpy, w, ch);
    XSync(dpy, False);
    XSetErrorHandler(old);
    if (!_x11_err && ok && ch->res_class && ch->res_class[0]) out = strdup(ch->res_class);
    if (ch->res_name) XFree(ch->res_name);
    if (ch->res_class) XFree(ch->res_class);
    XFree(ch);
    return out;
}

static Window* x11_client_list(Display* dpy, int* out_n) {
    if (out_n) *out_n = 0;
    if (!dpy) return NULL;
    Atom a = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    Atom type = None; int fmt = 0;
    unsigned long nitems = 0, after = 0; unsigned char* data = NULL;
    if (XGetWindowProperty(dpy, DefaultRootWindow(dpy), a, 0, 65536, False, XA_WINDOW,
                           &type, &fmt, &nitems, &after, &data) != Success) return NULL;
    Window* out = NULL;
    if (data) {
        if (type == XA_WINDOW && nitems > 0) {
            out = (Window*)malloc(nitems * sizeof(Window));
            if (out) { memcpy(out, data, nitems * sizeof(Window)); if (out_n) *out_n = (int)nitems; }
        }
        XFree(data);
    }
    return out;
}

static bool x11_window_geom(Display* dpy, Window w, int* out_x, int* out_y, int* out_w, int* out_h) {
    if (!dpy || w == None) return false;
    int gx = 0, gy = 0, abs_x = 0, abs_y = 0;
    unsigned int gw = 0, gh = 0, border = 0, depth = 0;
    Window child = None;
    _x11_err = 0;
    XErrorHandler old = XSetErrorHandler(_x11_err_handler);
    int ok = XGetGeometry(dpy, w, &child, &gx, &gy, &gw, &gh, &border, &depth);
    if (ok) ok = XTranslateCoordinates(dpy, w, DefaultRootWindow(dpy), 0, 0, &abs_x, &abs_y, &child);
    XSync(dpy, False);
    XSetErrorHandler(old);
    if (_x11_err || !ok || gw == 0 || gh == 0) return false;
    if (out_x) *out_x = abs_x;
    if (out_y) *out_y = abs_y;
    if (out_w) *out_w = (int)gw;
    if (out_h) *out_h = (int)gh;
    return true;
}

static bool x11_moveresize_window(Display* dpy, Window w, int x, int y, int cw, int ch) {
    if (!dpy || w == None) return false;
    _x11_err = 0;
    XErrorHandler old = XSetErrorHandler(_x11_err_handler);
    Atom mrs = XInternAtom(dpy, "_NET_MOVERESIZE_WINDOW", False);
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = mrs;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = (2L << 12) | (1L << 8) | (3L << 4) | (3L << 2) | 10L;
    ev.xclient.data.l[1] = x;
    ev.xclient.data.l[2] = y;
    ev.xclient.data.l[3] = cw;
    ev.xclient.data.l[4] = ch;
    XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XSync(dpy, False);
    XSetErrorHandler(old);
    if (!_x11_err) return true;
    XMoveResizeWindow(dpy, w, x, y, cw > 0 ? (unsigned)cw : 1, ch > 0 ? (unsigned)ch : 1);
    XSync(dpy, False);
    return _x11_err == 0;
}

static bool x11_activate_window(Display* dpy, Window w) {
    if (!dpy || w == None) return false;
    _x11_err = 0;
    XErrorHandler old = XSetErrorHandler(_x11_err_handler);
    Atom active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = active;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 2L;
    ev.xclient.data.l[1] = (long)CurrentTime;
    XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XSync(dpy, False);
    XSetErrorHandler(old);
    return _x11_err == 0;
}

static int x11_keycode_for(Display* dpy, KeySym ks, int* need_shift) {
    if (need_shift) *need_shift = 0;
    for (int kc = 8; kc < 256; kc++) {
        if (XkbKeycodeToKeysym(dpy, (KeyCode)kc, 0, 0) == ks) return kc;
    }
    for (int kc = 8; kc < 256; kc++) {
        if (XkbKeycodeToKeysym(dpy, (KeyCode)kc, 0, 1) == ks) {
            if (need_shift) *need_shift = 1;
            return kc;
        }
    }
    return 0;
}

bool get_title_x11(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) { log_error("buffer='%s' buffer_size=%d", buffer ? buffer : "null", buffer_size); return false; }
    buffer[0] = '\0';
    Display *display = get_display();
    if (!display) { log_error("display is NULL buffer='%s' buffer_size=%d", buffer ? buffer : "null", buffer_size); return false; }
    Window focus = None;
    int revert_to = 0;
    _x11_err = 0;
    XErrorHandler old = XSetErrorHandler(_x11_err_handler);
    XGetInputFocus(display, &focus, &revert_to);
    XSync(display, False);
    if (_x11_err || focus == None || focus == PointerRoot) {
        log_error("focus is NULL or PointerRoot buffer='%s' buffer_size=%d", buffer ? buffer : "null", buffer_size);
        XSetErrorHandler(old);
        return false;
    }
    char *name = NULL;
    _x11_err = 0;
    int ok = XFetchName(display, focus, &name);
    XSync(display, False);
    XSetErrorHandler(old);
    if (_x11_err || !ok || !name) {
        log_error("XFetchName failed buffer='%s' buffer_size=%d", buffer ? buffer : "null", buffer_size);
        if (name) XFree(name);
        return false;
    }
    strncpy(buffer, name, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    XFree(name);
    return buffer[0] != '\0';
}

bool get_app_id_x11(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    Display *display = get_display();
    if (!display) return false;
    Window focus = None;
    int revert_to = 0;
    _x11_err = 0;
    XErrorHandler old = XSetErrorHandler(_x11_err_handler);
    XGetInputFocus(display, &focus, &revert_to);
    XSync(display, False);
    if (_x11_err || focus == None || focus == PointerRoot) {
        XSetErrorHandler(old);
        return false;
    }
    XClassHint* ch = XAllocClassHint();
    _x11_err = 0;
    int ok = ch ? XGetClassHint(display, focus, ch) : 0;
    XSync(display, False);
    XSetErrorHandler(old);
    if (_x11_err || !ok || !ch || !ch->res_class || !ch->res_class[0]) {
        if (ch) XFree(ch);
        return false;
    }
    strncpy(buffer, ch->res_class, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    if (ch->res_name) XFree(ch->res_name);
    if (ch->res_class) XFree(ch->res_class);
    XFree(ch);
    return buffer[0] != '\0';
}

char* paste_x11(char* content) {
  if (!content) { log_error("content is NULL"); return NULL; }
  Display* dpy = get_display();
  if (!dpy) { log_error("no display"); return NULL; }
  int ev_q = 0, er_q = 0, maj = 0, min = 0;
  if (XTestQueryExtension(dpy, &ev_q, &er_q, &maj, &min) != True) {
    log_error("XTest extension unavailable, cannot synthesize keystrokes");
    return NULL;
  }
  int shift_unused = 0;
  int shift_kc = x11_keycode_for(dpy, XK_Shift_L, &shift_unused);
  int typed = 0;
  for (const unsigned char* p = (const unsigned char*)content; *p; p++) {
    KeySym ks;
    if (*p == '\n') ks = XK_Return;
    else if (*p == '\t') ks = XK_Tab;
    else if (*p >= 0x20 && *p <= 0x7E) ks = (KeySym)*p;
    else { log_warn("paste: skipping unsupported byte 0x%02x", (unsigned)*p); continue; }
    int need_shift = 0;
    int kc = x11_keycode_for(dpy, ks, &need_shift);
    if (!kc) { log_warn("paste: no keycode for keysym 0x%lx", (unsigned long)ks); continue; }
    if (need_shift && shift_kc) XTestFakeKeyEvent(dpy, (KeyCode)shift_kc, True, CurrentTime);
    XTestFakeKeyEvent(dpy, (KeyCode)kc, True, CurrentTime);
    XTestFakeKeyEvent(dpy, (KeyCode)kc, False, CurrentTime);
    if (need_shift && shift_kc) XTestFakeKeyEvent(dpy, (KeyCode)shift_kc, False, CurrentTime);
    XFlush(dpy);
    typed++;
  }
  XSync(dpy, False);
  log_info("paste_x11 typed %d char(s)", typed);
  return strdup("");
}

bool focus_window_title_x11(char* title) {
    if (!title || !title[0]) { log_error("title='%s'", title ? title : "null"); return false; }
    Display* display = get_display();
    if (!display) return false;
    int nclients = 0;
    Window* clients = x11_client_list(display, &nclients);
    if (!clients || nclients <= 0) {
        if (clients) free(clients);
        log_error("focus_window_title_x11: empty client list");
        return false;
    }
    bool found = false;
    for (int i = 0; i < nclients && !found; i++) {
        char* wt = x11_get_text_prop(display, clients[i]);
        if (!wt) continue;
        if (strcasestr(wt, title)) {
            if (x11_activate_window(display, clients[i])) found = true;
        }
        free(wt);
    }
    free(clients);
    if (!found) log_error("focus_window_title_x11: no match for title='%s'", title);
    return found;
}

FlWindowList get_window_names_x11() {
    FlWindowList list = { .windows = NULL, .count = 0 };
    Display* display = get_display();
    if (!display) { log_error("no display"); return list; }
    int nclients = 0;
    Window* clients = x11_client_list(display, &nclients);
    if (!clients || nclients <= 0) {
        if (clients) free(clients);
        log_error("get_window_names_x11: empty client list");
        return list;
    }
    for (int i = 0; i < nclients; i++) {
        char* cls = x11_get_res_class(display, clients[i]);
        if (!cls || !cls[0]) { if (cls) free(cls); continue; }
        char* dot = strchr(cls, '.');
        if (dot) *dot = '\0';
        if (cls[0] == '\0' || !strcasecmp(cls, "N/A") ||
            !strcasecmp(cls, "window") || !strcasecmp(cls, "colored")) {
            free(cls);
            continue;
        }
        cls[0] = (char)toupper((unsigned char)cls[0]);
        char** grown = (char**)realloc(list.windows, (size_t)(list.count + 1) * sizeof(char*));
        if (!grown) { free(cls); break; }
        list.windows = grown;
        list.windows[list.count++] = cls;
    }
    free(clients);
    return list;
}

FlWindowList get_window_titles_x11() {
    FlWindowList list = { .windows = NULL, .count = 0 };
    Display* display = get_display();
    if (!display) { log_error("no display"); return list; }
    int nclients = 0;
    Window* clients = x11_client_list(display, &nclients);
    if (!clients || nclients <= 0) {
        if (clients) free(clients);
        log_error("get_window_titles_x11: empty client list");
        return list;
    }
    for (int i = 0; i < nclients; i++) {
        char* wt = x11_get_text_prop(display, clients[i]);
        if (!wt) continue;
        while (*wt == ' ' || *wt == '\t') wt++;
        if (wt[0] == '\0' || !strcasecmp(wt, "window") || !strcasecmp(wt, "colored")) {
            free(wt);
            continue;
        }
        char** grown = (char**)realloc(list.windows, (size_t)(list.count + 1) * sizeof(char*));
        if (!grown) { free(wt); break; }
        list.windows = grown;
        list.windows[list.count++] = wt;
    }
    free(clients);
    return list;
}

static Display *_x11_cursor_dpy = NULL;

static Display *_x11_cursor_display(void) {
    if (_x11_cursor_dpy == NULL) {
        _x11_cursor_dpy = XOpenDisplay(NULL);
    }
    return _x11_cursor_dpy;
}

void x11_wm_cleanup_cursor_display(void) {
    if (_x11_cursor_dpy != NULL) {
        XCloseDisplay(_x11_cursor_dpy);
        _x11_cursor_dpy = NULL;
    }
}

FlPos get_cursor_position_x11(void) { 
    Display *display;
    Window root_window;
    Window window_returned;
    int root_x = 0, root_y = 0;
    int win_x, win_y;
    unsigned int mask_return;

    display = _x11_cursor_display();
    if (display == NULL) {
        return (FlPos){0, 0};
    }

    root_window = DefaultRootWindow(display);

    XQueryPointer(display, root_window, &window_returned, &window_returned,
                  &root_x, &root_y, &win_x, &win_y, &mask_return);

    return (FlPos){root_x, root_y};
}

static Window x11_focused_window(Display* display) {
    Window focus = None;
    int revert_to = 0;
    _x11_err = 0;
    XErrorHandler old = XSetErrorHandler(_x11_err_handler);
    XGetInputFocus(display, &focus, &revert_to);
    XSync(display, False);
    XSetErrorHandler(old);
    if (_x11_err || focus == None || focus == PointerRoot) return None;
    return focus;
}

bool close_active_window_x11(void) {
    Display* display = get_display();
    if (!display) return false;
    Window focus = x11_focused_window(display);
    if (focus == None) return false;
    Atom close_atom = XInternAtom(display, "_NET_CLOSE_WINDOW", False);
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = focus;
    ev.xclient.message_type = close_atom;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = CurrentTime;
    ev.xclient.data.l[1] = 2;
    _x11_err = 0;
    XErrorHandler old = XSetErrorHandler(_x11_err_handler);
    XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XSync(display, False);
    XSetErrorHandler(old);
    if (_x11_err) {
        XKillClient(display, focus);
        XSync(display, False);
    }
    return true;
}

bool toggle_fullscreen_active_x11(void) {
    Display* display = get_display();
    if (!display) return false;
    Window focus = x11_focused_window(display);
    if (focus == None) return false;
    Atom state_atom = XInternAtom(display, "_NET_WM_STATE", False);
    Atom fs_atom = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = focus;
    ev.xclient.message_type = state_atom;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 2;
    ev.xclient.data.l[1] = (long)fs_atom;
    ev.xclient.data.l[2] = 0;
    ev.xclient.data.l[3] = 1;
    _x11_err = 0;
    XErrorHandler old = XSetErrorHandler(_x11_err_handler);
    XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XSync(display, False);
    XSetErrorHandler(old);
    return _x11_err == 0;
}

bool move_active_window_to_monitor_x11(bool right) {
    Display* display = get_display();
    if (!display) return false;
    Window focus = x11_focused_window(display);
    if (focus == None) return false;
    Window root = DefaultRootWindow(display);
    XRRScreenResources* res = XRRGetScreenResourcesCurrent(display, root);
    if (!res) return false;
    int n = 0;
    int ox[64], oy[64], ow[64], oh[64];
    for (int c = 0; c < res->ncrtc && n < 64; c++) {
        XRRCrtcInfo* ci = XRRGetCrtcInfo(display, res, res->crtcs[c]);
        if (!ci) continue;
        if (ci->noutput == 0 || ci->width == 0 || ci->height == 0) { XRRFreeCrtcInfo(ci); continue; }
        int cx = ci->x, cy = ci->y, cw = (int)ci->width, chh = (int)ci->height;
        XRRFreeCrtcInfo(ci);
        bool dup = false;
        for (int i = 0; i < n; i++) {
            if (ox[i] == cx && oy[i] == cy && ow[i] == cw && oh[i] == chh) { dup = true; break; }
        }
        if (dup) continue;
        ox[n] = cx; oy[n] = cy; ow[n] = cw; oh[n] = chh; n++;
    }
    XRRFreeScreenResources(res);
    if (n < 2) return false;
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (ox[j] < ox[i]) {
                int t = ox[i]; ox[i] = ox[j]; ox[j] = t;
                t = oy[i]; oy[i] = oy[j]; oy[j] = t;
                t = ow[i]; ow[i] = ow[j]; ow[j] = t;
                t = oh[i]; oh[i] = oh[j]; oh[j] = t;
            }
        }
    }
    int wx = 0, wy = 0, ww = 0, wh = 0;
    if (!x11_window_geom(display, focus, &wx, &wy, &ww, &wh)) return false;
    int cx = wx + ww / 2;
    int cur = 0;
    for (int i = 0; i < n; i++) {
        if (cx >= ox[i] && cx < ox[i] + ow[i]) { cur = i; break; }
        if (ox[i] <= cx) cur = i;
    }
    int dst = right ? cur + 1 : cur - 1;
    if (dst < 0 || dst >= n) return false;
    int nx = ox[dst] + (wx - ox[cur]);
    int ny = oy[dst] + (wy - oy[cur]);
    if (nx < ox[dst]) nx = ox[dst];
    if (ny < oy[dst]) ny = oy[dst];
    if (nx + ww > ox[dst] + ow[dst]) nx = ox[dst] + ow[dst] - ww;
    if (ny + wh > oy[dst] + oh[dst]) ny = oy[dst] + oh[dst] - wh;
    if (nx < ox[dst]) nx = ox[dst];
    if (ny < oy[dst]) ny = oy[dst];
    return x11_moveresize_window(display, focus, nx, ny, ww, wh);
}

bool move_active_window_left_x11(void) {
    return move_active_window_to_monitor_x11(false);
}

bool move_active_window_right_x11(void) {
    return move_active_window_to_monitor_x11(true);
}

static bool x11_focus_direction(bool right) {
    Display* display = get_display();
    if (!display) return false;
    Window focus = x11_focused_window(display);
    if (focus == None) return false;
    int fcx = 0, fcy = 0, fw = 0, fh = 0;
    if (!x11_window_geom(display, focus, &fcx, &fcy, &fw, &fh)) return false;
    fcx += fw / 2;
    fcy += fh / 2;
    int nclients = 0;
    Window* clients = x11_client_list(display, &nclients);
    if (!clients || nclients <= 0) {
        if (clients) free(clients);
        return false;
    }
    Window best = None;
    int best_gap = 1 << 30;
    int best_vdist = 1 << 30;
    for (int i = 0; i < nclients; i++) {
        Window w = clients[i];
        if (w == focus || w == None) continue;
        int x = 0, y = 0, ww = 0, wh = 0;
        if (!x11_window_geom(display, w, &x, &y, &ww, &wh)) continue;
        if (ww <= 1 || wh <= 1) continue;
        int cx = x + ww / 2;
        int cy = y + wh / 2;
        if (right) { if (cx <= fcx) continue; }
        else       { if (cx >= fcx) continue; }
        int gap = right ? (cx - fcx) : (fcx - cx);
        int vdist = cy >= fcy ? cy - fcy : fcy - cy;
        if (gap < best_gap || (gap == best_gap && vdist < best_vdist)) {
            best_gap = gap;
            best_vdist = vdist;
            best = w;
        }
    }
    free(clients);
    if (best == None) return false;
    return x11_activate_window(display, best);
}

bool focus_window_left_x11(void) {
    return x11_focus_direction(false);
}

bool focus_window_right_x11(void) {
    return x11_focus_direction(true);
}

bool minimize_active_window_x11(void) {
    Display* display = get_display();
    if (!display) return false;
    if (_x11_last_min != None) {
        Window target = _x11_last_min;
        _x11_last_min = None;
        _x11_err = 0;
        XErrorHandler old = XSetErrorHandler(_x11_err_handler);
        XMapWindow(display, target);
        XRaiseWindow(display, target);
        XSetInputFocus(display, target, RevertToParent, CurrentTime);
        XSync(display, False);
        XSetErrorHandler(old);
        if (_x11_err == 0) return true;
    }
    Window focus = x11_focused_window(display);
    if (focus == None) return false;
    _x11_err = 0;
    XErrorHandler old = XSetErrorHandler(_x11_err_handler);
    XIconifyWindow(display, focus, DefaultScreen(display));
    XSync(display, False);
    XSetErrorHandler(old);
    if (_x11_err == 0) _x11_last_min = focus;
    return _x11_err == 0;
}
