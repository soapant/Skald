#include <X11/Xlib.h>
#include <time.h>
#include <X11/X.h>
#include <stdbool.h>

#include "core/window_manager.h"
#include "wm/wm.h"
#include "platform/draw.h"
#include "config.h"
#include "platform/wm_platform.h"
#include "helpers/x11/subclasses_x11.h"
#include "helpers/x11/drawing_x11.h"
#include "helpers/x11/display_x11.h"
#include "helpers/helpers_ui.h"
#include "core/listbox_hover.h"
#include "core/prompt_keys.h"
#include "helpers/helpers_text.h"
#include "helpers/x11/window_factory_x11.h"
#include "deps/log.h"
#include "config_values.h"
#include "core/types.h"

bool Platform_PromptPopupShowWindow(Popup *self, int x, int y, int w, int h) {
  WindowManager *wm = (WindowManager *)self->manager;
  Display *dpy = wm ? wm->display : NULL;
  if (!dpy)
    return false;
  struct timespec _t0, _t1, _t2, _t3;
  clock_gettime(CLOCK_MONOTONIC, &_t0);
  ListBox_InvalidateMousePos();
  self->handle.xid = x11_create_prompt_window(dpy, x, y, w, h);
  XSelectInput(dpy, self->handle.xid,
               KeyPressMask | KeyReleaseMask | ExposureMask | FocusChangeMask |
                   ButtonPressMask);
  clock_gettime(CLOCK_MONOTONIC, &_t1);
  Popup *popup = self;
  (void)popup;
  clock_gettime(CLOCK_MONOTONIC, &_t2);
  if (self->vtable && self->vtable->on_render)
    self->vtable->on_render(self);
  clock_gettime(CLOCK_MONOTONIC, &_t3);
  log_info(
      "prompt show: create=%.2fms bg=%.2fms first_render=%.2fms rounded=%d "
      "radius=%d opacity=%d",
      (_t1.tv_sec - _t0.tv_sec) * 1000.0 + (_t1.tv_nsec - _t0.tv_nsec) / 1e6,
      (_t2.tv_sec - _t1.tv_sec) * 1000.0 + (_t2.tv_nsec - _t1.tv_nsec) / 1e6,
      (_t3.tv_sec - _t2.tv_sec) * 1000.0 + (_t3.tv_nsec - _t2.tv_nsec) / 1e6,
      (int)RoundedCorners, CornerRadius, (int)WindowOpacity);
  XMapWindow(dpy, self->handle.xid);
  XSync(dpy, False);
  {
    XWindowAttributes _a;
    if (XGetWindowAttributes(dpy, self->handle.xid, &_a)) {
      log_info("prompt mapped req=(%d,%d %dx%d) actual=(%d,%d %dx%d) state=%d",
               x, y, w, h, _a.x, _a.y, _a.width, _a.height, (int)_a.map_state);
    } else {
      log_info("prompt mapped req=(%d,%d %dx%d) actual=unknown", x, y, w, h);
    }
  }
  {
    int (*old)(Display *, XErrorEvent *) = XSetErrorHandler(x11_error_handler);
    XSetInputFocus(dpy, self->handle.xid, RevertToParent, CurrentTime);
    XSync(dpy, False);
    XSetErrorHandler(old);
    Window _fw = None;
    int _rev = 0;
    XGetInputFocus(dpy, &_fw, &_rev);
    if (_fw != self->handle.xid) {
      old = XSetErrorHandler(x11_error_handler);
      XSetInputFocus(dpy, self->handle.xid, RevertToParent, CurrentTime);
      XSync(dpy, False);
      XSetErrorHandler(old);
      XGetInputFocus(dpy, &_fw, &_rev);
    }
    log_info("prompt focus win=%lu want=%lu ok=%d", (unsigned long)_fw,
             (unsigned long)self->handle.xid, _fw == self->handle.xid);
    char *_nm = NULL;
    if (XFetchName(dpy, self->handle.xid, &_nm) && _nm) {
      Wm_FocusByTitle(_nm);
      XFree(_nm);
    }
  }
  XFlush(dpy);
  return true;
}

void Platform_PromptPopupHideWindow(Popup *self) {
  Display *dpy = x11_get_display();
  if (dpy) {
    XUnmapWindow(dpy, self->handle.xid);
    XSync(dpy, False);
  }
}

void Platform_PromptPopupRender(Popup *self) {
  WindowManager *wm = (WindowManager *)self->manager;
  Display *dpy = wm ? wm->display : NULL;
  if (dpy) {
    const PromptState *psr = (PromptState *)self->user_data;
    if (psr && psr->no_bg) {
      SetTransparentWindow(true);
      x11_render_prompt_user(dpy, &self->handle.xid,
                             (PromptState *)self->user_data);
      SetTransparentWindow(false);
      return;
    }
    x11_render_prompt_user(dpy, &self->handle.xid,
                           (PromptState *)self->user_data);
  }
}

void Platform_PromptPopupDestroyWindow(Popup *self) {
  Display *dpy = x11_get_display();
  if (dpy) {
    x11_release_prompt_buffer(dpy, self->handle.xid);
    XDestroyWindow(dpy, self->handle.xid);
    XSync(dpy, False);
  }
}

bool Platform_PromptPopupOnKey(const Popup *self, const PopupKeyEvent *key) {
  PromptState *state = self ? (PromptState *)self->user_data : NULL;
  if (!state)
    return true;
  if (Prompt_HandleKey(state, key))
    return true;

  if (key->text_len > 0) {
    bool swallow = HotkeyText_ShouldSwallow(key);
    if (!swallow && HotkeyText_IsPrintable(key)) {
      Prompt_InsertText(state, key->text, key->text_len);
      return true;
    }
  }
  return false;
}
