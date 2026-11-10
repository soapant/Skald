#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <malloc.h>
#include <X11/X.h>
#include <stdbool.h>
#include <stddef.h>

#include "platform/wm_platform.h"
#include "helpers/helpers_monitor.h"
#include "core/window_manager.h"
#include "core/context.h"
#include "platform/platform.h"
#include "deps/log.h"
#include "helpers/x11/input_x11.h"
#include "helpers/helpers_input_state.h"
#include "core/remap_manager.h"
#include "helpers/x11/display_x11.h"
#include "core/listbox_hover.h"
#include "core/types.h"

extern int WindowManager_RunLinux(WindowManager *wm);
extern void WindowManager_QuitLinux(WindowManager *wm);
extern void WindowManager_ProcessEventsLinux(WindowManager *wm);

bool Platform_WM_InitDisplay(WindowManager *wm) {
  if (!wm)
    return false;
  wm->display = x11_get_display();
  return wm->display != NULL;
}

int Platform_WM_Run(WindowManager *wm) { return WindowManager_RunLinux(wm); }

void Platform_PromptFlushDisplay(WindowManager *wm) {
  Display *dpy = (wm && wm->display) ? wm->display : x11_get_display();
  if (dpy) {
    XFlush(dpy);
    XSync(dpy, False);
  }
}

void Platform_WM_Quit(WindowManager *wm) { WindowManager_QuitLinux(wm); }

void Platform_WM_ProcessEvents(WindowManager *wm) {
  WindowManager_ProcessEventsLinux(wm);
}

void Platform_WM_OnPopEmpty(WindowManager *wm) {
  (void)wm;
  x11_clear_suppress();
  remap_reset();
  InputState_Clear();
  malloc_trim(0);
}

void Platform_WM_GetMonitorGeometry(int *ox, int *oy, int *ow, int *oh) {
  int x = 0, y = 0, w = 1920, h = 1080;
  Display *dpy = NULL;
  WindowManager *wm = WindowManager_Get();
  if (wm)
    dpy = wm->display;
  if (!dpy)
    dpy = x11_get_display();
  if (dpy) {
    Window root = DefaultRootWindow(dpy);
    w = DisplayWidth(dpy, DefaultScreen(dpy));
    h = DisplayHeight(dpy, DefaultScreen(dpy));
    int px = 0, py = 0;
    Window rw = None, cw = None;
    int rx = 0, ry = 0, wx = 0, wy = 0;
    unsigned int m = 0;
    if (XQueryPointer(dpy, root, &rw, &cw, &rx, &ry, &wx, &wy, &m)) {
      px = rx;
      py = ry;
    }
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
    if (res) {
      int bx = x, by = y, bw = w, bh = h;
      bool found = false;
      for (int c = 0; c < res->ncrtc && !found; c++) {
        XRRCrtcInfo *ci = XRRGetCrtcInfo(dpy, res, res->crtcs[c]);
        if (!ci || ci->noutput == 0 || ci->width == 0 || ci->height == 0) {
          if (ci)
            XRRFreeCrtcInfo(ci);
          continue;
        }
        if (px >= ci->x && px < ci->x + ci->width && py >= ci->y &&
            py < ci->y + ci->height) {
          bx = ci->x;
          by = ci->y;
          bw = ci->width;
          bh = ci->height;
          found = true;
        }
        XRRFreeCrtcInfo(ci);
      }
      if (!found) {
        RROutput primary = XRRGetOutputPrimary(dpy, root);
        for (int c = 0; c < res->ncrtc && !found; c++) {
          XRRCrtcInfo *ci = XRRGetCrtcInfo(dpy, res, res->crtcs[c]);
          if (!ci || ci->noutput == 0 || ci->width == 0 || ci->height == 0) {
            if (ci)
              XRRFreeCrtcInfo(ci);
            continue;
          }
          for (int o = 0; o < ci->noutput; o++) {
            if (ci->outputs[o] == primary) {
              bx = ci->x;
              by = ci->y;
              bw = ci->width;
              bh = ci->height;
              found = true;
              break;
            }
          }
          XRRFreeCrtcInfo(ci);
        }
      }
      if (!found) {
        long best = -1;
        for (int c = 0; c < res->ncrtc; c++) {
          XRRCrtcInfo *ci = XRRGetCrtcInfo(dpy, res, res->crtcs[c]);
          if (!ci || ci->noutput == 0 || ci->width == 0 || ci->height == 0) {
            if (ci)
              XRRFreeCrtcInfo(ci);
            continue;
          }
          long area = (long)ci->width * ci->height;
          if (area > best) {
            best = area;
            bx = ci->x;
            by = ci->y;
            bw = ci->width;
            bh = ci->height;
          }
          XRRFreeCrtcInfo(ci);
        }
        if (best >= 0)
          found = true;
      }
      XRRFreeScreenResources(res);
      if (found) {
        x = bx;
        y = by;
        w = bw;
        h = bh;
      }
      log_info("monitor ptr=(%d,%d) geom=(%d,%d %dx%d)", px, py, x, y, w, h);
    }
  }
  if (ox)
    *ox = x;
  if (oy)
    *oy = y;
  if (ow)
    *ow = w;
  if (oh)
    *oh = h;
}


void Platform_ProcessPendingSubwindow(void) {
  SkaldContext *_c = SkaldContext_Get();
  if (!_c || !_c->window.subwindow.pending)
    return;
  _c->window.subwindow.pending = false;
  _c->window.subwindow.created = true;
  WindowManager *wm = WindowManager_Get();
  if (wm && wm->popup_count > 1)
    WindowManager_PopPopup(wm);
  Popup *p = PromptPopup_Create(wm, &_c->window.subwindow.pending_subwindow);
  if (p && p->vtable && p->vtable->on_render)
    p->vtable->on_render(p);
  if (wm && wm->display) {
    XFlush(wm->display);
    XSync(wm->display, False);
  }
}

int Monitor_Fill(MonitorEntry *out, int max_entries) {
  Display *dpy = NULL;
  WindowManager *wm = WindowManager_Get();
  if (wm)
    dpy = wm->display;
  if (!dpy)
    dpy = x11_get_display();
  if (!dpy)
    return 0;
  Window root = DefaultRootWindow(dpy);
  XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
  if (!res)
    return 0;
  int n = 0;
  int primary_at = -1;
  RROutput primary = XRRGetOutputPrimary(dpy, root);
  for (int c = 0; c < res->ncrtc && n < max_entries; c++) {
    XRRCrtcInfo *ci = XRRGetCrtcInfo(dpy, res, res->crtcs[c]);
    if (!ci || ci->noutput == 0 || ci->width == 0 || ci->height == 0) {
      if (ci)
        XRRFreeCrtcInfo(ci);
      continue;
    }
    for (int o = 0; o < ci->noutput; o++)
      if (ci->outputs[o] == primary) {
        primary_at = n;
        break;
      }
    out[n].x = ci->x;
    out[n].y = ci->y;
    out[n].w = ci->width;
    out[n].h = ci->height;
    out[n].is_primary = false;
    out[n].name[0] = '\0';
    for (int o = 0; o < ci->noutput && o < 1; o++) {
      XRROutputInfo *oi = XRRGetOutputInfo(dpy, res, ci->outputs[o]);
      if (oi) {
        if (oi->name && oi->nameLen > 0)
          snprintf(out[n].name, sizeof(out[n].name), "%s", oi->name);
        XRRFreeOutputInfo(oi);
      }
    }
    n++;
    XRRFreeCrtcInfo(ci);
  }
  XRRFreeScreenResources(res);
  if (primary_at >= 0) {
    out[primary_at].is_primary = true;
    if (primary_at > 0) {
      MonitorEntry t = out[0];
      out[0] = out[primary_at];
      out[primary_at] = t;
    }
  }
  return n;
}

