#include <X11/Xlib.h>
#include <time.h>
#include <X11/X.h>
#include <stdbool.h>

#include "core/window_manager.h"
#include "core/context.h"
#include "core/draw_state.h"
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

bool Platform_PromptPopupShowWindow(PlatformPromptPopupShowWindow *argspec) {
  WindowManager *wm = (WindowManager *)argspec->self->manager;
  Display *dpy = wm ? wm->display : NULL;
  if (!dpy)
    return false;
  struct timespec _t0, _t1, _t2, _t3;
  clock_gettime(CLOCK_MONOTONIC, &_t0);
  listbox_invalidate_mouse_pos();
  argspec->self->handle.xid = x11_create_prompt_window(&(X11CreatePromptWindow){.dpy = dpy, .x = argspec->x, .y = argspec->y, .w = argspec->w, .h = argspec->h});
  SkaldContext_Get()->window.hPopupWnd = WH_FROM_XID(argspec->self->handle.xid);
  XSelectInput(dpy, argspec->self->handle.xid,
               KeyPressMask | KeyReleaseMask | ExposureMask | FocusChangeMask |
                   ButtonPressMask | PointerMotionMask);
  clock_gettime(CLOCK_MONOTONIC, &_t1);
  Popup *popup = argspec->self;
  (void)popup;
  clock_gettime(CLOCK_MONOTONIC, &_t2);
  if (argspec->self->vtable && argspec->self->vtable->on_render)
    argspec->self->vtable->on_render(argspec->self);
  clock_gettime(CLOCK_MONOTONIC, &_t3);
  log_info(
      "prompt show: create=%.2fms bg=%.2fms first_render=%.2fms rounded=%d "
      "radius=%d opacity=%d",
      (_t1.tv_sec - _t0.tv_sec) * 1000.0 + (_t1.tv_nsec - _t0.tv_nsec) / 1e6,
      (_t2.tv_sec - _t1.tv_sec) * 1000.0 + (_t2.tv_nsec - _t1.tv_nsec) / 1e6,
      (_t3.tv_sec - _t2.tv_sec) * 1000.0 + (_t3.tv_nsec - _t2.tv_nsec) / 1e6,
      (int)RoundedCorners, CornerRadius, (int)WindowOpacity);
  XMapWindow(dpy, argspec->self->handle.xid);
  XSync(dpy, False);
  {
    XWindowAttributes _a;
    if (XGetWindowAttributes(dpy, argspec->self->handle.xid, &_a)) {
      log_info("prompt mapped req=(%d,%d %dx%d) actual=(%d,%d %dx%d) state=%d",
               argspec->x, argspec->y, argspec->w, argspec->h, _a.x, _a.y, _a.width, _a.height, (int)_a.map_state);
    } else {
      log_info("prompt mapped req=(%d,%d %dx%d) actual=unknown", argspec->x, argspec->y, argspec->w, argspec->h);
    }
  }
  {
    int (*old)(Display *, XErrorEvent *) = XSetErrorHandler(x11_error_handler);
    XSetInputFocus(dpy, argspec->self->handle.xid, RevertToParent, CurrentTime);
    XSync(dpy, False);
    XSetErrorHandler(old);
    Window _fw = None;
    int _rev = 0;
    XGetInputFocus(dpy, &_fw, &_rev);
    if (_fw != argspec->self->handle.xid) {
      old = XSetErrorHandler(x11_error_handler);
      XSetInputFocus(dpy, argspec->self->handle.xid, RevertToParent, CurrentTime);
      XSync(dpy, False);
      XSetErrorHandler(old);
      XGetInputFocus(dpy, &_fw, &_rev);
    }
    log_info("prompt focus win=%lu want=%lu ok=%d", (unsigned long)_fw,
             (unsigned long)argspec->self->handle.xid, _fw == argspec->self->handle.xid);
    char *_nm = NULL;
    if (XFetchName(dpy, argspec->self->handle.xid, &_nm) && _nm) {
      wm_focus_by_title(_nm);
      XFree(_nm);
    }
  }
  XFlush(dpy);
  return true;
}

bool Platform_PromptPopupResize(PlatformPromptPopupResize *argspec) {
  if (argspec->w <= 0 || argspec->h <= 0 || !argspec->self)
    return false;
  WindowManager *wm = (WindowManager *)argspec->self->manager;
  Display *dpy = wm ? wm->display : x11_get_display();
  Window win = argspec->self->handle.xid;
  if (!dpy || win == None)
    return false;
  x11_set_fixed_size_hints(dpy, win, argspec->w, argspec->h);
  XMoveResizeWindow(dpy, win, argspec->x, argspec->y, (unsigned)argspec->w, (unsigned)argspec->h);
  x11_apply_shape(&(X11ApplyShape){.dpy = dpy, .win = win, .w = argspec->w, .h = argspec->h, .rounded = RoundedCorners, .radius = RoundedCorners ? CornerRadius : 0});
  x11_release_prompt_buffer(dpy, win);
  XSync(dpy, False);
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
  if (!draw_state_begin_render())
    return;
  bool suppress_content = prompt_popup_resize_animating(self);
  WindowManager *wm = (WindowManager *)self->manager;
  Display *dpy = wm ? wm->display : NULL;
  if (dpy) {
    const PromptState *psr = (PromptState *)self->user_data;
    if (psr && psr->no_bg) {
      set_transparent_window(true);
      x11_render_prompt_user(dpy, &self->handle.xid,
                             (PromptState *)self->user_data, suppress_content);
      set_transparent_window(false);
    } else {
      x11_render_prompt_user(dpy, &self->handle.xid,
                             (PromptState *)self->user_data, suppress_content);
    }
  }
  if (draw_state_end_render())
    Platform_PromptPopupRender(self);
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
  log_info("x11 popup on_key: state=%p vk=%d", (void *)state,
           key ? key->vk : -1);
  if (!state)
    return true;
  bool handled = prompt_handle_key(state, key);
  log_info("x11 popup on_key: handled=%d vk=%d", handled ? 1 : 0,
           key ? key->vk : -1);
  if (handled)
    return true;

  if (key->text_len > 0) {
    bool swallow = hotkey_text_should_swallow(key);
    if (!swallow && hotkey_text_is_printable(key)) {
      prompt_insert_text(state, key->text, key->text_len);
      return true;
    }
  }
  return false;
}

