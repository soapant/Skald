#include "wm/x11_wm.h"
#include "deps/log.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include "platform/platform.h"
#include "helpers/helpers_text.h"
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
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"xdotool %s",content);
  char* out = Platform_RunTerminalCommand(buffer,false);
  if (!out) log_error("content='%s'", content ? content : "null");
  return out;
}

bool focus_window_title_x11(char* title) {
    if (!title) { log_error("title is NULL"); return false; }
    char command[512];
    snprintf(command, sizeof(command),
             "id=$(xwininfo -root -children | grep -i \"%s\" | awk '{print $1}' | head -n 1); "
             "[ -n \"$id\" ] && xprop -root -f _NET_ACTIVE_WINDOW 32a -set _NET_ACTIVE_WINDOW \"$id\"", 
             title);
    char* out = Platform_RunTerminalCommand(command, false);
    if (!out) { log_error("title='%s'", title ? title : "null"); return false; }
    free(out);
    return true;
}

FlWindowList get_window_names_x11() {
   char* output = Platform_RunTerminalCommand("wmctrl -x -l", false);
   FlWindowList list = { .windows = NULL, .count = 0 };
   char buffer[256]; 
   if (output == NULL || strlen(output) == 0) { log_error("output is NULL or empty"); return list; }
   if (strstr(output, "command not found") || strstr(output, ": not found")) { log_error("output is shell error"); free(output); return list; }
   StringList lines = String_Split(output, "\n");
   for (size_t i = 0; i < lines.count; i++) {
       char hex_id[32];
       char desktop_num[16];
       char class_string[128];
       if (sscanf(lines.items[i], "%s %s %s", hex_id, desktop_num, class_string) == 3) {
           char *dot = strchr(class_string, '.');
           if (dot) *dot = '\0';
           strncpy(buffer, class_string, sizeof(buffer) - 1);
           buffer[sizeof(buffer) - 1] = '\0';
           if (strcmp(buffer, "N/A") == 0 || strlen(buffer) == 0) continue;
           if (!strcasecmp(buffer, "window") || !strcasecmp(buffer, "colored")) continue;
           buffer[0] = toupper((unsigned char)buffer[0]);
           char** temp = realloc(list.windows, (list.count + 1) * sizeof(char*));
           if (temp == NULL) break;
           list.windows = temp;
           list.windows[list.count] = strdup(buffer);
           list.count++;
       }
   }
   StringList_Free(&lines);
   free(output);
   return list;
}
FlWindowList get_window_titles_x11() {
   char* output = Platform_RunTerminalCommand("wmctrl -l", false);
   FlWindowList list = { .windows = NULL, .count = 0 };
   if (output == NULL || strlen(output) == 0) { log_error("output is NULL or empty"); return list; }
   if (strstr(output, "command not found") || strstr(output, ": not found")) { log_error("output is shell error"); free(output); return list; }
   StringList lines = String_Split(output, "\n");
   for (size_t i = 0; i < lines.count; i++) {
       char *line = lines.items[i];
       if (!line || strlen(line)==0) continue;
       char hex_id[32]; char desktop[16]; char host[128];
       int off=0;
       if (sscanf(line, "%31s %15s %127s %n", hex_id, desktop, host, &off) < 3) continue;
       char *title = line + off;
       while (*title==' ' || *title=='\t') title++;
       if (strlen(title)==0) continue;
       if (!strcasecmp(title, "window") || !strcasecmp(title, "colored")) continue;
       char** temp = realloc(list.windows, (list.count + 1) * sizeof(char*));
       if (temp == NULL) break;
       list.windows = temp;
       list.windows[list.count] = strdup(title);
       list.count++;
   }
   StringList_Free(&lines);
   free(output);
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
    int* ox = NULL;
    int* ow = NULL;
    int* oy = NULL;
    int* oh = NULL;
    for (int c = 0; c < res->ncrtc; c++) {
        XRRCrtcInfo* ci = XRRGetCrtcInfo(display, res, res->crtcs[c]);
        if (!ci || ci->noutput == 0 || ci->width == 0 || ci->height == 0) { if (ci) XRRFreeCrtcInfo(ci); continue; }
        int* nox = realloc(ox, (size_t)(n + 1) * sizeof(int));
        int* noy = realloc(oy, (size_t)(n + 1) * sizeof(int));
        int* now = realloc(ow, (size_t)(n + 1) * sizeof(int));
        int* noh = realloc(oh, (size_t)(n + 1) * sizeof(int));
        if (!nox || !noy || !now || !noh) {
            free(nox ? nox : ox); free(noy ? noy : oy); free(now ? now : ow); free(noh ? noh : oh);
            ox = NULL; oy = NULL; ow = NULL; oh = NULL;
            XRRFreeCrtcInfo(ci);
            break;
        }
        ox = nox; oy = noy; ow = now; oh = noh;
        ox[n] = ci->x; oy[n] = ci->y; ow[n] = (int)ci->width; oh[n] = (int)ci->height;
        n++;
        XRRFreeCrtcInfo(ci);
    }
    XRRFreeScreenResources(res);
    if (n < 2) { free(ox); free(oy); free(ow); free(oh); return false; }
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
    XWindowAttributes attr;
    _x11_err = 0;
    XErrorHandler old = XSetErrorHandler(_x11_err_handler);
    int got = XGetWindowAttributes(display, focus, &attr);
    XSync(display, False);
    XSetErrorHandler(old);
    if (_x11_err || !got) { free(ox); free(oy); free(ow); free(oh); return false; }
    int cx = attr.x + attr.width / 2;
    int cur = 0;
    for (int i = 0; i < n; i++) {
        if (cx >= ox[i] && cx < ox[i] + ow[i]) { cur = i; break; }
        if (ox[i] <= cx) cur = i;
    }
    int dst = right ? cur + 1 : cur - 1;
    if (dst < 0 || dst >= n) { free(ox); free(oy); free(ow); free(oh); return false; }
    int rel_x = attr.x - ox[cur];
    int rel_y = attr.y - oy[cur];
    int nx = ox[dst] + rel_x;
    int ny = oy[dst] + rel_y;
    if (nx < ox[dst]) nx = ox[dst];
    if (ny < oy[dst]) ny = oy[dst];
    if (nx + attr.width > ox[dst] + ow[dst]) nx = ox[dst] + ow[dst] - attr.width;
    if (ny + attr.height > oy[dst] + oh[dst]) ny = oy[dst] + oh[dst] - attr.height;
    if (nx < ox[dst]) nx = ox[dst];
    if (ny < oy[dst]) ny = oy[dst];
    _x11_err = 0;
    old = XSetErrorHandler(_x11_err_handler);
    XMoveWindow(display, focus, nx, ny);
    XRaiseWindow(display, focus);
    XSync(display, False);
    XSetErrorHandler(old);
    free(ox); free(oy); free(ow); free(oh);
    return _x11_err == 0;
}

bool move_active_window_left_x11(void) {
    return move_active_window_to_monitor_x11(false);
}

bool move_active_window_right_x11(void) {
    return move_active_window_to_monitor_x11(true);
}

bool focus_window_left_x11(void) {
    Display* display = get_display();
    if (!display) return false;
    Window focus = x11_focused_window(display);
    if (focus == None) return false;
    Window root = DefaultRootWindow(display);
    XWindowAttributes fattr;
    _x11_err = 0;
    XErrorHandler old = XSetErrorHandler(_x11_err_handler);
    int got = XGetWindowAttributes(display, focus, &fattr);
    XSync(display, False);
    XSetErrorHandler(old);
    if (_x11_err || !got) return false;
    int fcx = fattr.x + fattr.width / 2;
    int fcy = fattr.y + fattr.height / 2;
    Window rroot = None;
    Window parent = None;
    Window* children = NULL;
    unsigned int nchildren = 0;
    _x11_err = 0;
    old = XSetErrorHandler(_x11_err_handler);
    int ok = XQueryTree(display, root, &rroot, &parent, &children, &nchildren);
    XSync(display, False);
    XSetErrorHandler(old);
    if (_x11_err || !ok || !children) {
        if (children) XFree(children);
        return false;
    }
    Window best = None;
    int best_gap = 1 << 30;
    int best_vdist = 1 << 30;
    for (unsigned int i = 0; i < nchildren; i++) {
        Window w = children[i];
        if (w == focus) continue;
        XWindowAttributes attr;
        _x11_err = 0;
        old = XSetErrorHandler(_x11_err_handler);
        int g = XGetWindowAttributes(display, w, &attr);
        XSync(display, False);
        XSetErrorHandler(old);
        if (_x11_err || !g) continue;
        if (attr.map_state != IsViewable) continue;
        if (attr.width <= 0 || attr.height <= 0) continue;
        int cx = attr.x + attr.width / 2;
        int cy = attr.y + attr.height / 2;
        if (cx >= fcx) continue;
        int gap = fcx - cx;
        int vdist = cy >= fcy ? cy - fcy : fcy - cy;
        if (gap < best_gap || (gap == best_gap && vdist < best_vdist)) {
            best_gap = gap;
            best_vdist = vdist;
            best = w;
        }
    }
    XFree(children);
    if (best == None) return false;
    _x11_err = 0;
    old = XSetErrorHandler(_x11_err_handler);
    XRaiseWindow(display, best);
    XSetInputFocus(display, best, RevertToParent, CurrentTime);
    XSync(display, False);
    XSetErrorHandler(old);
    return _x11_err == 0;
}

bool focus_window_right_x11(void) {
    Display* display = get_display();
    if (!display) return false;
    Window focus = x11_focused_window(display);
    if (focus == None) return false;
    Window root = DefaultRootWindow(display);
    XWindowAttributes fattr;
    _x11_err = 0;
    XErrorHandler old = XSetErrorHandler(_x11_err_handler);
    int got = XGetWindowAttributes(display, focus, &fattr);
    XSync(display, False);
    XSetErrorHandler(old);
    if (_x11_err || !got) return false;
    int fcx = fattr.x + fattr.width / 2;
    int fcy = fattr.y + fattr.height / 2;
    Window rroot = None;
    Window parent = None;
    Window* children = NULL;
    unsigned int nchildren = 0;
    _x11_err = 0;
    old = XSetErrorHandler(_x11_err_handler);
    int ok = XQueryTree(display, root, &rroot, &parent, &children, &nchildren);
    XSync(display, False);
    XSetErrorHandler(old);
    if (_x11_err || !ok || !children) {
        if (children) XFree(children);
        return false;
    }
    Window best = None;
    int best_gap = 1 << 30;
    int best_vdist = 1 << 30;
    for (unsigned int i = 0; i < nchildren; i++) {
        Window w = children[i];
        if (w == focus) continue;
        XWindowAttributes attr;
        _x11_err = 0;
        old = XSetErrorHandler(_x11_err_handler);
        int g = XGetWindowAttributes(display, w, &attr);
        XSync(display, False);
        XSetErrorHandler(old);
        if (_x11_err || !g) continue;
        if (attr.map_state != IsViewable) continue;
        if (attr.width <= 0 || attr.height <= 0) continue;
        int cx = attr.x + attr.width / 2;
        int cy = attr.y + attr.height / 2;
        if (cx <= fcx) continue;
        int gap = cx - fcx;
        int vdist = cy >= fcy ? cy - fcy : fcy - cy;
        if (gap < best_gap || (gap == best_gap && vdist < best_vdist)) {
            best_gap = gap;
            best_vdist = vdist;
            best = w;
        }
    }
    XFree(children);
    if (best == None) return false;
    _x11_err = 0;
    old = XSetErrorHandler(_x11_err_handler);
    XRaiseWindow(display, best);
    XSetInputFocus(display, best, RevertToParent, CurrentTime);
    XSync(display, False);
    XSetErrorHandler(old);
    return _x11_err == 0;
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
