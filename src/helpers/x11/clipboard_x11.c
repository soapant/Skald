#include "helpers/x11/clipboard_x11.h"
#include "helpers/helpers_text.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <errno.h>
#include <X11/X.h>
#include <stdarg.h>

#include "helpers/x11/display_x11.h"
#include "core/clipboard_history.h"
#include "config_values.h"
#include "core/types/window.h"

#define CLIP_INCR_THRESHOLD (256u * 1024u)
#define CLIP_INCR_CHUNK (64u * 1024u)
#define CLIP_READ_TRIES 40

static volatile long cl_seq = 0;
static long cl_reads = 0;
static long cl_read_fail = 0;
static long cl_selclear = 0;

static long cl_selreq = 0;

static long cl_ms_since(void) {

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static long cl_t0 = 0;

static void cl_trace(const char *what, const char *fmt, ...) {

  if (!DebugMode) {
    return;
  }
  va_list ap;
  long n = ++cl_seq;
  char buf[512];
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  fprintf(stderr, "[clip %+ldms #%ld] %s %s\n", cl_ms_since() - cl_t0, n, what,
          buf);
}

static Window clip_win = None;
static Display *clip_rd_dpy = NULL;
static Window clip_rd_win = None;
static Atom clip_rd_prop = None;
static char *clip_text = NULL;
static unsigned char *clip_png = NULL;
static size_t clip_png_len = 0;
static Atom clip_clipboard = None;
static Atom clip_primary = None;
static Atom clip_targets = None;
static Atom clip_utf8 = None;
static Atom clip_timestamp = None;
static Atom clip_jpeg_atom = None;
static Atom clip_gif_atom = None;
static Atom clip_bmp_atom = None;
static Atom clip_img_atom = None;
static Atom clip_urilist = None;
static Atom clip_gnome_files = None;
static char *clip_file_uri = NULL;
static char *clip_gnome_payload = NULL;
static Atom clip_incr = None;
static Atom clip_rd_incr = None;

static bool incr_active = false;
static Window incr_req = None;
static Atom incr_prop = None;
static size_t incr_off = 0;
static long incr_old_mask = 0;

void x11_clipboard_ensure_started(void) {
  if (cl_t0 == 0)
    cl_t0 = cl_ms_since();
  Display *dpy = x11_get_display();
  if (!dpy) {
    return;
  }
  if (clip_win != None) {
    return;
  }
  Window root = DefaultRootWindow(dpy);
  XSetWindowAttributes attrs = {0};
  attrs.override_redirect = True;
  Window win = XCreateWindow(dpy, root, 0, 0, 1, 1, 0, 0, InputOutput,
                             CopyFromParent, CWOverrideRedirect, &attrs);
  if (!win) {
    return;
  }
  clip_clipboard = XInternAtom(dpy, "CLIPBOARD", False);
  clip_primary = XInternAtom(dpy, "PRIMARY", False);
  clip_targets = XInternAtom(dpy, "TARGETS", False);
  clip_utf8 = XInternAtom(dpy, "UTF8_STRING", False);
  clip_timestamp = XInternAtom(dpy, "TIMESTAMP", False);
  clip_jpeg_atom = XInternAtom(dpy, "image/jpeg", False);
  clip_gif_atom = XInternAtom(dpy, "image/gif", False);
  clip_bmp_atom = XInternAtom(dpy, "image/bmp", False);
  clip_urilist = XInternAtom(dpy, "text/uri-list", False);
  clip_gnome_files = XInternAtom(dpy, "x-special/gnome-copied-files", False);
  clip_incr = XInternAtom(dpy, "INCR", False);
  XMapWindow(dpy, win);
  XFlush(dpy);
  clip_win = win;
}

static void x11_clipboard_abort_incr(void) {

  if (incr_active && incr_req != None && incr_old_mask) {
    Display *adpy = x11_get_display();
    if (adpy)
      XSelectInput(adpy, incr_req, incr_old_mask);
  }
  incr_active = false;
  incr_req = None;
  incr_prop = None;
  incr_off = 0;
  incr_old_mask = 0;
}

static void x11_clipboard_serve_request(Display *dpy,
                                        XSelectionRequestEvent *req) {

  XEvent resp = {0};
  resp.xselection.type = SelectionNotify;
  resp.xselection.display = req->display;
  resp.xselection.requestor = req->requestor;
  resp.xselection.selection = req->selection;
  resp.xselection.target = req->target;
  resp.xselection.property = None;
  resp.xselection.time = req->time;
  Atom prop = req->property == None ? req->target : req->property;
  if (req->selection != clip_clipboard && req->selection != clip_primary) {
    XSendEvent(dpy, req->requestor, False, 0, &resp);
    XFlush(dpy);
    return;
  }
  char *tname = XGetAtomName(dpy, req->target);
  cl_trace("SERVE",
           "requestor=%lu sel=%s target=%s(%lu) prop_req=%lu time=%lu text=%s "
           "png=%d",
           (unsigned long)req->requestor,
           req->selection == clip_clipboard
               ? "CLIPBOARD"
               : (req->selection == clip_primary ? "PRIMARY" : "other"),
           tname ? tname : "?", (unsigned long)req->target,
           (unsigned long)req->property, (unsigned long)req->time,
           clip_text ? "yes" : "no", (int)clip_png_len);
  if (tname)
    XFree(tname);
  if (req->target == clip_targets) {
    Atom list[10];
    int n = 0;
    list[n++] = clip_targets;
    list[n++] = clip_timestamp;
    if (clip_text) {
      list[n++] = clip_utf8;
      list[n++] = XA_STRING;
    }
    if (clip_png)
      list[n++] = clip_img_atom;
    if (clip_file_uri) {
      list[n++] = clip_urilist;
      list[n++] = clip_gnome_files;
    }
    XChangeProperty(dpy, req->requestor, prop, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)list, n);
    resp.xselection.property = prop;
  } else if ((req->target == clip_utf8 || req->target == XA_STRING) &&
             clip_text) {
    Atom type = (req->target == clip_utf8) ? clip_utf8 : XA_STRING;
    XChangeProperty(dpy, req->requestor, prop, type, 8, PropModeReplace,
                    (unsigned char *)clip_text, strlen(clip_text));
    resp.xselection.property = prop;
  } else if (req->target == clip_img_atom && clip_png) {
    if (clip_png_len > CLIP_INCR_THRESHOLD) {
      x11_clipboard_abort_incr();
      XWindowAttributes wa;
      memset(&wa, 0, sizeof(wa));
      incr_old_mask = 0;
      if (XGetWindowAttributes(dpy, req->requestor, &wa)) {
        incr_old_mask = (long)wa.your_event_mask;
        XSelectInput(dpy, req->requestor, incr_old_mask | PropertyChangeMask);
      } else {
        XSelectInput(dpy, req->requestor, PropertyChangeMask);
      }
      long total = (long)clip_png_len;
      XChangeProperty(dpy, req->requestor, prop, clip_incr, 32, PropModeReplace,
                      (unsigned char *)&total, 1);
      XFlush(dpy);
      incr_active = true;
      incr_req = req->requestor;
      incr_prop = prop;
      incr_off = 0;

      resp.xselection.property = prop;
    } else {
      XChangeProperty(dpy, req->requestor, prop, clip_img_atom, 8,
                      PropModeReplace, clip_png, (int)clip_png_len);
      resp.xselection.property = prop;
    }
  } else if (req->target == clip_urilist && clip_file_uri) {
    size_t n = strlen(clip_file_uri);
    XChangeProperty(dpy, req->requestor, prop, clip_urilist, 8, PropModeReplace,
                    (unsigned char *)clip_file_uri, (int)n);
    resp.xselection.property = prop;
  } else if (req->target == clip_gnome_files && clip_gnome_payload) {
    XChangeProperty(dpy, req->requestor, prop, clip_gnome_files, 8,
                    PropModeReplace, (unsigned char *)clip_gnome_payload,
                    (int)strlen(clip_gnome_payload));
    resp.xselection.property = prop;
  } else if (req->target == clip_timestamp) {
    unsigned long tm = (unsigned long)req->time;
    XChangeProperty(dpy, req->requestor, prop, XA_INTEGER, 32, PropModeReplace,
                    (unsigned char *)&tm, 1);
    resp.xselection.property = prop;
  } else {
  }
  XSendEvent(dpy, req->requestor, False, 0, &resp);
  XFlush(dpy);
  char *tname2 = XGetAtomName(dpy, req->target);
  cl_trace("SERVE", "replied requestor=%lu target=%s prop_given=%s refused=%d",
           (unsigned long)req->requestor, tname2 ? tname2 : "?",
           resp.xselection.property != None ? "yes" : "NONE",
           resp.xselection.property == None ? 1 : 0);
  if (tname2)
    XFree(tname2);
}

static void x11_clipboard_serve_incr(Display *dpy, XPropertyEvent *pev) {

  if (!incr_active || !clip_png) {
    x11_clipboard_abort_incr();
    return;
  }
  if (pev->window != incr_req || pev->atom != incr_prop ||
      pev->state != PropertyDelete) {
    return;
  }
  size_t remaining = clip_png_len - incr_off;
  if (remaining > 0) {
    size_t n = remaining > CLIP_INCR_CHUNK ? CLIP_INCR_CHUNK : remaining;
    XChangeProperty(dpy, incr_req, incr_prop, clip_img_atom, 8, PropModeReplace,
                    clip_png + incr_off, (int)n);
    incr_off += n;
    cl_trace("INCR", "sent chunk n=%zu off=%zu remaining=%zu", n, incr_off,
             remaining);
  } else {
    XChangeProperty(dpy, incr_req, incr_prop, clip_img_atom, 8, PropModeReplace,
                    NULL, 0);
    cl_trace("INCR", "END off=%zu len=%zu terminator sent", incr_off,
             clip_png_len);
    x11_clipboard_abort_incr();
  }
  XFlush(dpy);
}

bool x11_clipboard_handle_event(Display *dpy, XEvent *ev) {
  if (!dpy || !ev)
    return false;
  if (ev->type == SelectionRequest) {
    cl_selreq++;
    cl_trace("EVENT",
             "SelectionRequest n=%ld requestor=%lu sel=%lu target=%lu prop=%lu "
             "time=%lu",
             cl_selreq, (unsigned long)ev->xselectionrequest.requestor,
             (unsigned long)ev->xselectionrequest.selection,
             (unsigned long)ev->xselectionrequest.target,
             (unsigned long)ev->xselectionrequest.property,
             (unsigned long)ev->xselectionrequest.time);
    x11_clipboard_serve_request(dpy, &ev->xselectionrequest);
    return true;
  }
  if (ev->type == PropertyNotify) {
    if (incr_active) {
      x11_clipboard_serve_incr(dpy, &ev->xproperty);
      return true;
    }
    return false;
  }
  if (ev->type == SelectionClear) {
    cl_selclear++;
    Window new_owner = XGetSelectionOwner(dpy, ev->xselectionclear.selection);
    cl_trace(
        "EVENT",
        "SelectionClear n=%ld sel=%lu new_owner=%lu had_text=%d had_png=%d",
        cl_selclear, (unsigned long)ev->xselectionclear.selection,
        (unsigned long)new_owner, clip_text ? 1 : 0, (int)clip_png_len);
    if (ev->xselectionclear.selection == clip_clipboard ||
        ev->xselectionclear.selection == clip_primary) {
      char *sname = XGetAtomName(dpy, ev->xselectionclear.selection);

      if (sname)
        XFree(sname);
      x11_clipboard_abort_incr();
      free(clip_text);
      clip_text = NULL;
      free(clip_png);
      clip_png = NULL;
      clip_png_len = 0;
      free(clip_file_uri);
      clip_file_uri = NULL;
      free(clip_gnome_payload);
      clip_gnome_payload = NULL;
      return true;
    }
  }
  return false;
}

static int x11_clipboard_take_ownership(Display *dpy) {

  Window prev = XGetSelectionOwner(dpy, clip_clipboard);
  cl_trace("OWN", "before prev_clipboard_owner=%lu our_win=%lu",
           (unsigned long)prev, (unsigned long)clip_win);
  XSetSelectionOwner(dpy, clip_clipboard, clip_win, CurrentTime);
  XSetSelectionOwner(dpy, clip_primary, clip_win, CurrentTime);
  XFlush(dpy);
  Window owner = XGetSelectionOwner(dpy, clip_clipboard);
  cl_trace("OWN", "after owner=%lu we_own=%d", (unsigned long)owner,
           owner == clip_win ? 1 : 0);
  return owner == clip_win;
}

int x11_clipboard_set_text(const char *text) {
  if (!text)
    return 0;
  cl_trace("SETTEXT", "begin len=%d text='%s'", (int)strlen(text), text);
  x11_clipboard_ensure_started();
  Display *dpy = x11_get_display();
  if (!dpy || clip_win == None) {
    cl_trace("SETTEXT", "no display/win");
    return 0;
  }
  char *nt = strdup(text);
  if (!nt)
    return 0;
  free(clip_text);
  clip_text = nt;
  free(clip_png);
  clip_png = NULL;
  clip_png_len = 0;
  free(clip_file_uri);
  clip_file_uri = NULL;
  free(clip_gnome_payload);
  clip_gnome_payload = NULL;
  XStoreBytes(dpy, text, (int)strlen(text));
  XFlush(dpy);
  int r = x11_clipboard_take_ownership(dpy);
  cl_trace("SETTEXT", "end ret=%d", r);
  return r;
}

int x11_clipboard_set_file(const char *path) {
  if (!path || !path[0])
    return 0;
  x11_clipboard_ensure_started();
  Display *dpy = x11_get_display();
  if (!dpy || clip_win == None)
    return 0;
  x11_clipboard_abort_incr();
  char *uri = clipboard_path_to_uri_list(path);
  if (!uri)
    return 0;
  size_t n = strlen(uri) + 8;
  char *gnome = (char *)malloc(n);
  if (!gnome) {
    free(uri);
    return 0;
  }
  snprintf(gnome, n, "copy\n%s", uri);
  free(clip_file_uri);
  clip_file_uri = uri;
  free(clip_gnome_payload);
  clip_gnome_payload = gnome;
  free(clip_text);
  clip_text = NULL;
  free(clip_png);
  clip_png = NULL;
  clip_png_len = 0;
  return x11_clipboard_take_ownership(dpy);
}

int x11_clipboard_set_image(const unsigned char *png, size_t len) {
  if (!png || len == 0)
    return 0;

  x11_clipboard_ensure_started();
  Display *dpy = x11_get_display();
  if (!dpy || clip_win == None)
    return 0;
  x11_clipboard_abort_incr();
  const char *mime = clipboard_image_mime(clipboard_image_format(png, len));
  clip_img_atom = XInternAtom(dpy, mime ? mime : "image/png", False);
  unsigned char *np = malloc(len);
  if (!np)
    return 0;
  memcpy(np, png, len);
  free(clip_png);
  clip_png = np;
  clip_png_len = len;
  free(clip_text);
  clip_text = NULL;
  free(clip_file_uri);
  clip_file_uri = NULL;
  free(clip_gnome_payload);
  clip_gnome_payload = NULL;
  return x11_clipboard_take_ownership(dpy);
}

void x11_clipboard_shutdown(void) {
  Display *dpy = x11_get_display();
  if (dpy && clip_win != None)
    XDestroyWindow(dpy, clip_win);
  clip_win = None;
  if (clip_rd_dpy) {
    XCloseDisplay(clip_rd_dpy);
    clip_rd_dpy = NULL;
    clip_rd_win = None;
    clip_rd_prop = None;
  }
  x11_clipboard_abort_incr();
  free(clip_text);
  clip_text = NULL;
  free(clip_png);
  clip_png = NULL;
  clip_png_len = 0;
  free(clip_file_uri);
  clip_file_uri = NULL;
  free(clip_gnome_payload);
  clip_gnome_payload = NULL;
}

typedef struct {
  Window win;
  Atom selection;
  Atom property;
} ClipReadWait;

static Bool x11_clip_read_notify_pred(Display *dpy, XEvent *ev, XPointer arg) {

  (void)dpy;
  ClipReadWait *w = (ClipReadWait *)arg;
  if (ev->type == SelectionNotify)
    return ev->xselection.requestor == w->win &&
           ev->xselection.selection == w->selection;
  return False;
}

static Bool x11_clip_read_prop_pred(Display *dpy, XEvent *ev, XPointer arg) {

  (void)dpy;
  ClipReadWait *w = (ClipReadWait *)arg;
  if (ev->type == PropertyNotify)
    return ev->xproperty.window == w->win &&
           ev->xproperty.atom == w->property &&
           ev->xproperty.state == PropertyNewValue;
  return False;
}

static long x11_clip_now_ms(void) {

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long)ts.tv_sec * 1000 + (long)ts.tv_nsec / 1000000;
}

static int x11_clip_read_wait_event(Display *dpy, XEvent *ev,
                                    Bool (*pred)(Display *, XEvent *, XPointer),
                                    ClipReadWait *w, int tries) {

  if (tries <= 0)
    tries = 1;
  int fd = ConnectionNumber(dpy);
  long deadline = x11_clip_now_ms() + (long)tries * 5;
  struct pollfd pfd = {fd, POLLIN, 0};
  for (;;) {
    if (XCheckIfEvent(dpy, ev, pred, (XPointer)w))
      return 1;
    long remain = deadline - x11_clip_now_ms();
    if (remain <= 0)
      return 0;
    int r = poll(&pfd, 1, (int)remain);
    if (r < 0 && errno != EINTR)
      return 0;
  }
}

static unsigned char *x11_clip_get_prop(Display *dpy, Window win, Atom prop,
                                        long length, bool delete_flag,
                                        Atom *out_actual, int *out_format,
                                        unsigned long *out_nitems,
                                        unsigned long *out_after) {
  Atom actual = None;
  int format = 0;
  unsigned long nitems = 0;
  unsigned long after = 0;
  unsigned char *data = NULL;
  if (XGetWindowProperty(dpy, win, prop, 0, length, delete_flag ? True : False,
                         AnyPropertyType, &actual, &format, &nitems, &after,
                         &data) != Success)
    return NULL;
  if (out_actual)
    *out_actual = actual;
  if (out_format)
    *out_format = format;
  if (out_nitems)
    *out_nitems = nitems;
  if (out_after)
    *out_after = after;
  return data;
}

static unsigned char *x11_clip_fetch(Display *dpy, Window win, Atom prop,
                                     Atom *actual, int *format,
                                     unsigned long *nitems,
                                     unsigned long *after) {
  return x11_clip_get_prop(dpy, win, prop, (~0L), true, actual, format, nitems,
                           after);
}

typedef struct {
  unsigned char *data;
  Atom actual;
  unsigned long nitems;
} X11ClipFetchResult;

static X11ClipFetchResult x11_clip_fetch_full(Display *dpy, Window win,
                                              Atom prop) {
  X11ClipFetchResult r = {0};
  int format = 0;
  unsigned long after = 0;
  r.data =
      x11_clip_fetch(dpy, win, prop, &r.actual, &format, &r.nitems, &after);
  return r;
}

static char *x11_clip_read_property(Display *dpy, Window win, Atom prop,
                                    size_t *out_len) {

  X11ClipFetchResult r = x11_clip_fetch_full(dpy, win, prop);
  if (!r.data)
    return NULL;
  if (r.actual == clip_rd_incr) {
    XFree(r.data);
    return NULL;
  }
  unsigned char *data = r.data;
  unsigned long nitems = r.nitems;
  char *out = malloc(nitems + 1);
  if (!out) {
    XFree(data);
    return NULL;
  }
  memcpy(out, data, nitems);
  out[nitems] = '\0';
  if (out_len)
    *out_len = (size_t)nitems;
  XFree(data);
  return out;
}

static char *x11_clip_read_incr(Display *dpy, Window win, Atom prop,
                                ClipReadWait *w) {

  size_t cap = 65536;
  size_t len = 0;
  char *out = malloc(cap);
  if (!out)
    return NULL;
  XEvent ev;
  for (;;) {
    if (!x11_clip_read_wait_event(dpy, &ev, x11_clip_read_prop_pred, w, 400))
      break;
    X11ClipFetchResult r = x11_clip_fetch_full(dpy, win, prop);
    unsigned char *data = r.data;
    unsigned long nitems = r.nitems;
    if (!data || nitems == 0) {
      if (data)
        XFree(data);
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

  if (clip_rd_dpy)
    return clip_rd_dpy;
  Display *d = XOpenDisplay(NULL);
  if (!d) {
    Display *main_dpy = x11_get_display();
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
  clip_rd_win = XCreateWindow(d, root, 0, 0, 1, 1, 0, 0, InputOutput,
                              CopyFromParent, CWOverrideRedirect, &attrs);
  clip_rd_prop = XInternAtom(d, "CLIP_READ", False);
  clip_rd_incr = XInternAtom(d, "INCR", False);
  XFlush(d);
  clip_rd_dpy = d;
  return clip_rd_dpy;
}

static char *x11_clip_read_target(Atom target) {
  Display *dpy = x11_clipboard_read_display();
  if (!dpy || clip_rd_win == None) {
    cl_trace("READ", "no read display/win");
    return NULL;
  }
  Window win = clip_rd_win;
  Atom prop = clip_rd_prop;
  char *tname = XGetAtomName(dpy, target);
  char tbuf[64] = {0};
  if (tname) {
    SafeCopy(tbuf, sizeof(tbuf), tname);
    XFree(tname);
    tname = NULL;
  }
  XSelectInput(dpy, win, PropertyChangeMask);
  XConvertSelection(dpy, clip_clipboard, target, prop, win, CurrentTime);
  XFlush(dpy);
  ClipReadWait w = {win, clip_clipboard, prop};
  XEvent ev;
  if (!x11_clip_read_wait_event(dpy, &ev, x11_clip_read_notify_pred, &w,
                                CLIP_READ_TRIES)) {
    return NULL;
  }
  if (ev.xselection.property == None) {
    return NULL;
  }
  Atom actual = None;
  int format = 0;
  unsigned long nitems = 0;
  unsigned long after = 0;
  unsigned char *data = NULL;
  if (XGetWindowProperty(dpy, win, prop, 0, 1, False, AnyPropertyType, &actual,
                         &format, &nitems, &after, &data) != Success) {
    return NULL;
  }
  if (data)
    XFree(data);
  if (actual == clip_rd_incr) {
    XDeleteProperty(dpy, win, prop);
    XFlush(dpy);
    return x11_clip_read_incr(dpy, win, prop, &w);
  }
  return x11_clip_read_property(dpy, win, prop, NULL);
}

unsigned long x11_clipboard_owner(void) {

  x11_clipboard_ensure_started();
  Display *dpy = x11_get_display();
  if (!dpy || clip_clipboard == None)
    return 0;
  return (unsigned long)XGetSelectionOwner(dpy, clip_clipboard);
}

char *x11_clipboard_get_text(void) {
  cl_reads++;
  x11_clipboard_ensure_started();
  Display *dpy = x11_get_display();
  if (!dpy || clip_win == None) {
    cl_trace("GETTEXT", "no display, read#%ld", cl_reads);
    return NULL;
  }
  if (XGetSelectionOwner(dpy, clip_clipboard) == clip_win && clip_text) {
    return strdup(clip_text);
  }
  char *out = x11_clip_read_target(clip_utf8);
  if (!out) {
    out = x11_clip_read_target(XA_STRING);
  }
  if (!out)
    cl_read_fail++;
  return out;
}

#if !defined(SKALD_WAYLAND_SESSION)
#include "platform/x11/clipboard_backend_x11.h"

bool x11_clipboard_available(void) { return true; }
#endif
