#include "wm/x11_wm.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xrandr.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glob.h>
#include <sys/stat.h>
#include <X11/X.h>
#include <X11/keysym.h>

#include "helpers/x11/display_x11.h"
#include "helpers/x11/env_helpers_x11.h"
#include "deps/log.h"
#include "platform/platform.h"
#include "helpers/helpers_text.h"

static int _x11_err = 0;
static Window _x11_last_min = None;
static int ewmh_move_works = -1;

static int i3_probe = 0;
static char i3_sock[512];

static bool i3_ensure_sock(void) {

  if (i3_probe != 0)
    return i3_probe == 1;
  i3_probe = -1;
  const char *cur = getenv("I3SOCK");
  if (cur && cur[0]) {
    struct stat st;
    if (stat(cur, &st) == 0) {
      snprintf(i3_sock, sizeof(i3_sock), "%s", cur);
      i3_probe = 1;
      return true;
    }
  }
  const char *rtd = x11_env_xdg_runtime_dir();
  if (!rtd || !rtd[0])
    return false;
  char pat[512];
  snprintf(pat, sizeof(pat), "%s/i3/ipc-socket.*", rtd);
  glob_t g;
  memset(&g, 0, sizeof(g));
  if (glob(pat, 0, NULL, &g) == 0 && g.gl_pathc > 0) {
    snprintf(i3_sock, sizeof(i3_sock), "%s", g.gl_pathv[g.gl_pathc - 1]);
    struct stat st;
    if (stat(i3_sock, &st) == 0) {
      setenv("I3SOCK", i3_sock, 1);
      i3_probe = 1;
    }
  }
  globfree(&g);
  return i3_probe == 1;
}

static bool i3_exec(const char *command) {

  if (!i3_ensure_sock())
    return false;
  char *out = Platform_RunTerminalCommand(command, false);
  if (!out || out[0] == '\0') {
    if (out)
      free(out);
    return false;
  }
  bool ok = !(strstr(out, "\"success\":false") || strstr(out, "\"error\""));
  free(out);
  return ok;
}

static bool i3_move_to_output(bool right) {

  char cmd[128];
  snprintf(cmd, sizeof(cmd), "i3-msg move container to output %s",
           right ? "right" : "left");
  return i3_exec(cmd);
}
static int _x11_err_handler(Display *dpy, XErrorEvent *ev) {

  (void)dpy;
  (void)ev;
  _x11_err = 1;
  {
    return 0;
  }
}

static char *x11_get_text_prop(Display *dpy, Window w) {

  if (!dpy || w == None)
    return NULL;
  Atom utf8 = XInternAtom(dpy, "UTF8_STRING", False);
  Atom net_name = XInternAtom(dpy, "_NET_WM_NAME", False);
  Atom wm_name = XInternAtom(dpy, "WM_NAME", False);
  char *out = NULL;
  for (int pass = 0; pass < 2 && !out; pass++) {
    Atom prop = (pass == 0) ? net_name : wm_name;
    Atom want = (pass == 0) ? utf8 : AnyPropertyType;
    Atom type = None;
    int fmt = 0;
    unsigned long nitems = 0, after = 0;
    unsigned char *data = NULL;
    if (XGetWindowProperty(dpy, w, prop, 0, 65536, False, want, &type, &fmt,
                           &nitems, &after, &data) != Success)
      continue;
    if (data) {
      if (type != None && nitems > 0) {
        out = (char *)malloc(nitems + 1);
        if (out) {
          memcpy(out, data, nitems);
          out[nitems] = '\0';
        }
      }
      XFree(data);
    }
  }
  return out;
}

static char *x11_get_res_class(Display *dpy, Window w) {

  if (!dpy || w == None)
    return NULL;
  XClassHint *ch = XAllocClassHint();
  if (!ch)
    return NULL;
  char *out = NULL;
  _x11_err = 0;
  XErrorHandler old = XSetErrorHandler(_x11_err_handler);
  int ok = XGetClassHint(dpy, w, ch);
  XSync(dpy, False);
  XSetErrorHandler(old);
  if (!_x11_err && ok && ch->res_class && ch->res_class[0])
    out = strdup(ch->res_class);
  if (ch->res_name)
    XFree(ch->res_name);
  if (ch->res_class)
    XFree(ch->res_class);
  XFree(ch);
  return out;
}

static Window *x11_client_list(Display *dpy, int *out_n) {

  if (out_n)
    *out_n = 0;
  if (!dpy)
    return NULL;
  Atom a = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
  Atom type = None;
  int fmt = 0;
  unsigned long nitems = 0, after = 0;
  unsigned char *data = NULL;
  if (XGetWindowProperty(dpy, DefaultRootWindow(dpy), a, 0, 65536, False,
                         XA_WINDOW, &type, &fmt, &nitems, &after,
                         &data) != Success) {
    return NULL;
  }
  Window *out = NULL;
  if (data) {
    if (type == XA_WINDOW && nitems > 0) {
      out = (Window *)malloc(nitems * sizeof(Window));
      if (out) {
        memcpy(out, data, nitems * sizeof(Window));
        if (out_n)
          *out_n = (int)nitems;
      }
    }
    XFree(data);
  }
  return out;
}

static bool x11_window_geom(Display *dpy, Window w, int *out_x, int *out_y,
                            int *out_w, int *out_h) {

  if (!dpy || w == None)
    return false;
  int gx = 0, gy = 0, abs_x = 0, abs_y = 0;
  unsigned int gw = 0, gh = 0, border = 0, depth = 0;
  Window child = None;
  _x11_err = 0;
  XErrorHandler old = XSetErrorHandler(_x11_err_handler);
  int ok = XGetGeometry(dpy, w, &child, &gx, &gy, &gw, &gh, &border, &depth);
  if (ok)
    ok = XTranslateCoordinates(dpy, w, DefaultRootWindow(dpy), 0, 0, &abs_x,
                               &abs_y, &child);
  XSync(dpy, False);
  XSetErrorHandler(old);
  if (_x11_err || !ok || gw == 0 || gh == 0)
    return false;
  if (out_x)
    *out_x = abs_x;
  if (out_y)
    *out_y = abs_y;
  if (out_w)
    *out_w = (int)gw;
  if (out_h)
    *out_h = (int)gh;
  return true;
}

static bool x11_moveresize_window(Display *dpy, Window w, int x, int y, int cw,
                                  int ch) {

  if (!dpy || w == None)
    return false;
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
  XSendEvent(dpy, DefaultRootWindow(dpy), False,
             SubstructureRedirectMask | SubstructureNotifyMask, &ev);
  XSync(dpy, False);
  XSetErrorHandler(old);
  if (!_x11_err)
    return true;
  XMoveResizeWindow(dpy, w, x, y, cw > 0 ? (unsigned)cw : 1,
                    ch > 0 ? (unsigned)ch : 1);
  XSync(dpy, False);
  return _x11_err == 0;
}

static bool x11_activate_window(Display *dpy, Window w) {

  if (!dpy || w == None)
    return false;
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
  XSendEvent(dpy, DefaultRootWindow(dpy), False,
             SubstructureRedirectMask | SubstructureNotifyMask, &ev);
  XSync(dpy, False);
  XSetErrorHandler(old);
  return _x11_err == 0;
}

static int x11_keycode_for(Display *dpy, KeySym ks, int *need_shift) {

  if (need_shift)
    *need_shift = 0;
  for (int kc = 8; kc < 256; kc++) {
    if (XkbKeycodeToKeysym(dpy, (KeyCode)kc, 0, 0) == ks)
      return kc;
  }
  for (int kc = 8; kc < 256; kc++) {
    if (XkbKeycodeToKeysym(dpy, (KeyCode)kc, 0, 1) == ks) {
      if (need_shift)
        *need_shift = 1;
      return kc;
    }
  }
  return 0;
}

static Window x11_focused_window(Display *display);

bool x11_get_title(char *buffer, int buffer_size) {
  if (!buffer || buffer_size <= 0) {
    log_error("buffer='%s' buffer_size=%d", buffer ? buffer : "null",
              buffer_size);
    return false;
  }
  buffer[0] = '\0';
  Display *display = x11_get_display();
  if (!display) {
    log_error("display is NULL buffer='%s' buffer_size=%d",
              buffer ? buffer : "null", buffer_size);
    return false;
  }
  Window focus = x11_focused_window(display);
  if (focus == None)
    return false;
  char *name = x11_get_text_prop(display, focus);
  if (!name || !name[0]) {
    log_error("no window title property (win=0x%lx) buffer_size=%d",
              (unsigned long)focus, buffer_size);
    if (name)
      free(name);
    return false;
  }
  SafeCopy(buffer, (size_t)buffer_size, name);
  free(name);
  return buffer[0] != '\0';
}

bool x11_get_app_id(char *buffer, int buffer_size) {
  if (!buffer || buffer_size <= 0)
    return false;
  buffer[0] = '\0';
  Display *display = x11_get_display();
  if (!display)
    return false;
  Window focus = x11_focused_window(display);
  if (focus == None)
    return false;
  XClassHint *ch = XAllocClassHint();
  _x11_err = 0;
  XErrorHandler old = XSetErrorHandler(_x11_err_handler);
  int ok = ch ? XGetClassHint(display, focus, ch) : 0;
  XSync(display, False);
  XSetErrorHandler(old);
  if (_x11_err || !ok || !ch || !ch->res_class || !ch->res_class[0]) {
    if (ch)
      XFree(ch);
    return false;
  }
  SafeCopy(buffer, (size_t)buffer_size, ch->res_class);
  if (ch->res_name)
    XFree(ch->res_name);
  if (ch->res_class)
    XFree(ch->res_class);
  XFree(ch);
  return buffer[0] != '\0';
}

char *x11_paste(char *content) {
  if (!content) {
    log_error("content is NULL");
    return NULL;
  }
  Display *dpy = x11_get_display();
  if (!dpy) {
    log_error("no display");
    return NULL;
  }
  int ev_q = 0, er_q = 0, maj = 0, min = 0;
  if (XTestQueryExtension(dpy, &ev_q, &er_q, &maj, &min) != True) {
    log_error("XTest extension unavailable, cannot synthesize keystrokes");
    return NULL;
  }
  int shift_unused = 0;
  int shift_kc = x11_keycode_for(dpy, XK_Shift_L, &shift_unused);
  for (const unsigned char *p = (const unsigned char *)content; *p; p++) {
    KeySym ks;
    if (*p == '\n')
      ks = XK_Return;
    else if (*p == '\t')
      ks = XK_Tab;
    else if (*p >= 0x20 && *p <= 0x7E)
      ks = (KeySym)*p;
    else {
      log_warn("paste: skipping unsupported byte 0x%02x", (unsigned)*p);
      continue;
    }
    int need_shift = 0;
    int kc = x11_keycode_for(dpy, ks, &need_shift);
    if (!kc) {
      log_warn("paste: no keycode for keysym 0x%lx", (unsigned long)ks);
      continue;
    }
    if (need_shift && shift_kc)
      XTestFakeKeyEvent(dpy, (KeyCode)shift_kc, True, CurrentTime);
    XTestFakeKeyEvent(dpy, (KeyCode)kc, True, CurrentTime);
    XTestFakeKeyEvent(dpy, (KeyCode)kc, False, CurrentTime);
    if (need_shift && shift_kc)
      XTestFakeKeyEvent(dpy, (KeyCode)shift_kc, False, CurrentTime);
    XFlush(dpy);
  }
  XSync(dpy, False);
  return strdup("");
}

unsigned long x11_find_window_by_title(const char *title) {
  if (!title || !title[0])
    return 0;
  Display *display = x11_get_display();
  if (!display)
    return 0;
  int nclients = 0;
  Window *clients = x11_client_list(display, &nclients);
  if (!clients || nclients <= 0) {
    if (clients)
      free(clients);
    return 0;
  }
  unsigned long found = 0;
  for (int i = 0; i < nclients; i++) {
    char *wt = x11_get_text_prop(display, clients[i]);
    if (!wt)
      continue;
    if (strcasestr(wt, title)) {
      found = (unsigned long)clients[i];
      free(wt);
      break;
    }
    free(wt);
  }
  free(clients);
  return found;
}

bool x11_focus_window_title(const char *title) {
  if (!title || !title[0]) {
    log_error("title='%s'", title ? title : "null");
    return false;
  }
  Display *display = x11_get_display();
  if (!display)
    return false;
  unsigned long target = x11_find_window_by_title(title);
  if (target == 0) {
    log_error("x11_focus_window_title: no match for title='%s'", title);
    return false;
  }
  return x11_activate_window(display, (Window)target);
}

static PlWindowList x11_list_client_fields(bool want_names) {
  PlWindowList list = {.windows = NULL, .count = 0};
  Display *display = x11_get_display();
  if (!display) {
    log_error("no display");
    return list;
  }
  int nclients = 0;
  Window *clients = x11_client_list(display, &nclients);
  if (!clients || nclients <= 0) {
    if (clients)
      free(clients);
    log_error("empty client list");
    return list;
  }
  for (int i = 0; i < nclients; i++) {
    char *s = want_names ? x11_get_res_class(display, clients[i])
                         : x11_get_text_prop(display, clients[i]);
    if (!s)
      continue;
    if (want_names) {
      char *dot = strchr(s, '.');
      if (dot)
        *dot = '\0';
      if (s[0] == '\0' || !strcasecmp(s, "N/A") || !strcasecmp(s, "window") ||
          !strcasecmp(s, "colored")) {
        free(s);
        continue;
      }
      s[0] = (char)toupper((unsigned char)s[0]);
    } else {
      char *trimmed = s;
      while (*trimmed == ' ' || *trimmed == '\t')
        trimmed++;
      if (trimmed[0] == '\0' || !strcasecmp(trimmed, "window") ||
          !strcasecmp(trimmed, "colored")) {
        free(s);
        continue;
      }
      if (trimmed != s) {
        char *dup = strdup(trimmed);
        free(s);
        if (!dup)
          continue;
        s = dup;
      }
    }
    bool appended = List_Append(&list, s);
    free(s);
    if (!appended)
      break;
  }
  free(clients);
  return list;
}

PlWindowList x11_get_window_names(void) { return x11_list_client_fields(true); }

PlWindowList x11_get_window_titles(void) {
  return x11_list_client_fields(false);
}

static Display *_x11_cursor_dpy = NULL;

void x11_wm_cleanup_cursor_display(void) {
  if (_x11_cursor_dpy != NULL) {
    XCloseDisplay(_x11_cursor_dpy);
    _x11_cursor_dpy = NULL;
  }
}

static Window x11_focused_window(Display *display) {

  Window focus = None;
  int revert_to = 0;
  _x11_err = 0;
  XErrorHandler old = XSetErrorHandler(_x11_err_handler);
  XGetInputFocus(display, &focus, &revert_to);
  XSync(display, False);
  XSetErrorHandler(old);
  if (_x11_err || focus == None || focus == PointerRoot)
    return None;
  return focus;
}

bool x11_close_active_window(void) {
  Display *display = x11_get_display();
  if (!display)
    return false;
  Window focus = x11_focused_window(display);
  if (focus == None)
    return false;
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
  XSendEvent(display, DefaultRootWindow(display), False,
             SubstructureRedirectMask | SubstructureNotifyMask, &ev);
  XSync(display, False);
  XSetErrorHandler(old);
  if (_x11_err) {
    XKillClient(display, focus);
    XSync(display, False);
  }
  return true;
}

bool x11_toggle_fullscreen_active(void) {
  Display *display = x11_get_display();
  if (!display)
    return false;
  Window focus = x11_focused_window(display);
  if (focus == None)
    return false;
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
  XSendEvent(display, DefaultRootWindow(display), False,
             SubstructureRedirectMask | SubstructureNotifyMask, &ev);
  XSync(display, False);
  XSetErrorHandler(old);
  return _x11_err == 0;
}

static bool x11_activate_neighbor_dir(bool vertical, bool positive) {
  Display *display = x11_get_display();
  if (!display)
    return false;
  Window focus = x11_focused_window(display);
  int fcx = 0, fcy = 0, fw = 0, fh = 0;
  if (!x11_window_geom(display, focus, &fcx, &fcy, &fw, &fh))
    return false;
  fcx += fw / 2;
  fcy += fh / 2;
  int nclients = 0;
  Window *clients = x11_client_list(display, &nclients);
  if (!clients || nclients <= 0) {
    if (clients)
      free(clients);
    return false;
  }
  Window best = None;
  int best_gap = 1 << 30;
  int best_cross = 1 << 30;
  for (int i = 0; i < nclients; i++) {
    Window w = clients[i];
    if (w == focus || w == None)
      continue;
    int x = 0, y = 0, ww = 0, wh = 0;
    if (!x11_window_geom(display, w, &x, &y, &ww, &wh))
      continue;
    if (ww <= 1 || wh <= 1)
      continue;
    int cx = x + ww / 2;
    int cy = y + wh / 2;
    int axis = vertical ? cy : cx;
    int axis_ref = vertical ? fcy : fcx;
    int cross = vertical ? cx : cy;
    int cross_ref = vertical ? fcx : fcy;
    if (positive) {
      if (axis <= axis_ref)
        continue;
    } else {
      if (axis >= axis_ref)
        continue;
    }
    int gap = positive ? (axis - axis_ref) : (axis_ref - axis);
    int cross_dist = cross >= cross_ref ? cross - cross_ref : cross_ref - cross;
    if (gap < best_gap || (gap == best_gap && cross_dist < best_cross)) {
      best_gap = gap;
      best_cross = cross_dist;
      best = w;
    }
  }
  free(clients);
  if (best == None)
    return false;
  return x11_activate_window(display, best);
}

static bool x11_activate_neighbor(bool right) {
  return x11_activate_neighbor_dir(false, right);
}

static bool x11_activate_neighbor_vertical(bool down) {
  return x11_activate_neighbor_dir(true, down);
}

bool x11_move_active_window_to_monitor(bool right) {
  Display *display = x11_get_display();
  if (!display)
    return false;
  Window focus = x11_focused_window(display);
  if (focus == None)
    return false;
  Window root = DefaultRootWindow(display);
  XRRScreenResources *res = XRRGetScreenResourcesCurrent(display, root);
  if (!res)
    return false;
  int n = 0;
  int ox[64], oy[64], ow[64], oh[64];
  for (int c = 0; c < res->ncrtc && n < 64; c++) {
    XRRCrtcInfo *ci = XRRGetCrtcInfo(display, res, res->crtcs[c]);
    if (!ci)
      continue;
    if (ci->noutput == 0 || ci->width == 0 || ci->height == 0) {
      XRRFreeCrtcInfo(ci);
      continue;
    }
    int cx = ci->x, cy = ci->y, cw = (int)ci->width, chh = (int)ci->height;
    XRRFreeCrtcInfo(ci);
    bool dup = false;
    for (int i = 0; i < n; i++) {
      if (ox[i] == cx && oy[i] == cy && ow[i] == cw && oh[i] == chh) {
        dup = true;
        break;
      }
    }
    if (dup)
      continue;
    ox[n] = cx;
    oy[n] = cy;
    ow[n] = cw;
    oh[n] = chh;
    n++;
  }
  XRRFreeScreenResources(res);
  if (n < 2)
    return false;
  for (int i = 0; i < n - 1; i++) {
    for (int j = i + 1; j < n; j++) {
      if (ox[j] < ox[i]) {
        int t = ox[i];
        ox[i] = ox[j];
        ox[j] = t;
        t = oy[i];
        oy[i] = oy[j];
        oy[j] = t;
        t = ow[i];
        ow[i] = ow[j];
        ow[j] = t;
        t = oh[i];
        oh[i] = oh[j];
        oh[j] = t;
      }
    }
  }
  int wx = 0, wy = 0, ww = 0, wh = 0;
  if (!x11_window_geom(display, focus, &wx, &wy, &ww, &wh))
    return false;
  int cx = wx + ww / 2;
  int cur = 0;
  for (int i = 0; i < n; i++) {
    if (cx >= ox[i] && cx < ox[i] + ow[i]) {
      cur = i;
      break;
    }
    if (ox[i] <= cx)
      cur = i;
  }
  int dst = right ? cur + 1 : cur - 1;
  if (dst < 0 || dst >= n)
    return false;
  int nx = ox[dst] + (wx - ox[cur]);
  int ny = oy[dst] + (wy - oy[cur]);
  if (nx < ox[dst])
    nx = ox[dst];
  if (ny < oy[dst])
    ny = oy[dst];
  if (nx + ww > ox[dst] + ow[dst])
    nx = ox[dst] + ow[dst] - ww;
  if (ny + wh > oy[dst] + oh[dst])
    ny = oy[dst] + oh[dst] - wh;
  if (nx < ox[dst])
    nx = ox[dst];
  if (ny < oy[dst])
    ny = oy[dst];
  bool sent = x11_moveresize_window(display, focus, nx, ny, ww, wh);
  if (ewmh_move_works == 1)
    return sent;
  if (ewmh_move_works == 0)
    return false;
  XSync(display, False);
  int vx = 0, vy = 0, vw = 0, vh = 0;
  bool moved = false;
  for (int tries = 0; tries < 20; tries++) {
    usleep(5000);
    if (x11_window_geom(display, focus, &vx, &vy, &vw, &vh) &&
        (vx != wx || vy != wy)) {
      moved = true;
      break;
    }
  }
  if (moved) {
    ewmh_move_works = 1;
    return true;
  }
  ewmh_move_works = 0;
  return false;
}

static bool x11_move_dir(bool right) {

  Display *display = x11_get_display();
  if (!display)
    return false;
  Window before = x11_focused_window(display);
  bool moved =
      (ewmh_move_works != 0 && x11_move_active_window_to_monitor(right)) ||
      i3_move_to_output(right);
  if (!moved)
    return x11_activate_neighbor(right);
  if (before != None)
    x11_activate_window(display, before);
  return true;
}

bool x11_move_active_window_left(void) { return x11_move_dir(false); }

bool x11_move_active_window_right(void) { return x11_move_dir(true); }

bool x11_move_active_window_up(void) {
  return x11_activate_neighbor_vertical(false);
}

bool x11_move_active_window_down(void) {
  return x11_activate_neighbor_vertical(true);
}

bool x11_focus_window_left(void) { return x11_activate_neighbor(false); }

bool x11_focus_window_right(void) { return x11_activate_neighbor(true); }

bool x11_focus_window_up(void) { return x11_activate_neighbor_vertical(false); }

bool x11_focus_window_down(void) {
  return x11_activate_neighbor_vertical(true);
}

static bool x11_is_hidden(Display *dpy, Window w) {

  XWindowAttributes at;
  _x11_err = 0;
  XErrorHandler old = XSetErrorHandler(_x11_err_handler);
  int ok = XGetWindowAttributes(dpy, w, &at);
  XSync(dpy, False);
  XSetErrorHandler(old);
  if (_x11_err || !ok)
    return false;
  return at.map_state == IsUnmapped;
}

bool x11_minimize_active_window(void) {
  Display *display = x11_get_display();
  if (!display)
    return false;
  if (_x11_last_min != None) {
    Window target = _x11_last_min;
    _x11_last_min = None;
    _x11_err = 0;
    XErrorHandler old = XSetErrorHandler(_x11_err_handler);
    XMapWindow(display, target);
    XSync(display, False);
    XSetErrorHandler(old);
    if (_x11_err == 0) {
      x11_activate_window(display, target);
      return true;
    }
  }
  Window focus = x11_focused_window(display);
  if (focus == None)
    return false;
  _x11_err = 0;
  XErrorHandler old = XSetErrorHandler(_x11_err_handler);
  XIconifyWindow(display, focus, DefaultScreen(display));
  XSync(display, False);
  XSetErrorHandler(old);
  if (_x11_err == 0 && x11_is_hidden(display, focus)) {
    _x11_last_min = focus;
    return true;
  }
  _x11_err = 0;
  old = XSetErrorHandler(_x11_err_handler);
  XUnmapWindow(display, focus);
  XSync(display, False);
  XSetErrorHandler(old);
  if (_x11_err == 0 && x11_is_hidden(display, focus)) {
    _x11_last_min = focus;
    return true;
  }
  return false;
}

bool x11_get_mouse_pos(int *x, int *y) {
  Display *dpy = x11_get_display();
  if (!dpy)
    return false;
  Window root = DefaultRootWindow(dpy);
  Window rw = 0, cw = 0;
  int rx = 0, ry = 0, wx = 0, wy = 0;
  unsigned int mask = 0;
  if (!XQueryPointer(dpy, root, &rw, &cw, &rx, &ry, &wx, &wy, &mask))
    return false;
  if (x)
    *x = rx;
  if (y)
    *y = ry;
  return true;
}
