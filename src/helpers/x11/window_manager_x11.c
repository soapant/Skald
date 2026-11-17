#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/X.h>
#include <X11/extensions/XKB.h>
#include <linux/input-event-codes.h>
#include "core/hotkey_manager.h"
#include "core/feature_loader.h"

#include "core/window_manager.h"
#include "deps/log.h"
#include "core/listbox_hover.h"
#include "core/prompt_keys.h"
#include "helpers/helpers_input_state.h"
#include "platform/platform.h"
#include "platform/draw.h"
#include "core/context.h"
#include "core/types.h"
#include "core/types/keys.h"
#if __has_include(<X11/extensions/XTest.h>)
#include <X11/extensions/XTest.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/input.h>
#include "helpers/x11/clipboard_x11.h"
#include "helpers/x11/input_x11.h"
#include "helpers/helpers_render_thread.h"
#include "platform/widget.h"
#include "core/remap_manager.h"
#include "config_values.h"

int pending_hotkey_vk = 0;
int pending_hotkey_mod = 0;
long pending_hotkey_timeMs = 0;

static bool x11_autorepeat_saved = false;
static unsigned int x11_saved_delay = 0;
static unsigned int x11_saved_interval = 0;

static bool should_use_grab(void) { return x11_should_use_grab(); }

static int init_uinput(void) { return x11_init_uinput(); }
static void destroy_uinput(void) {

  x11_mouse_grab_end();
  x11_destroy_uinput();
}
static void x11_scan_and_hook_keyboards(void) { x11_scan_keyboards(); }

static void *input_thread_func(void *arg) {
  WindowManager *wm = (WindowManager *)arg;
  int kb_count = x11_keyboard_count_get(), mouse_count = x11_mouse_count_get();
  if (kb_count == 0 && mouse_count == 0)
    return NULL;
  int nfds = kb_count + mouse_count;
  struct pollfd *fds = calloc((size_t)nfds, sizeof(struct pollfd));
  if (!fds) {
    return NULL;
  }
  for (int i = 0; i < kb_count; i++) {
    fds[i].fd = x11_keyboard_fd(i);
    fds[i].events = POLLIN;
  }
  for (int i = 0; i < mouse_count; i++) {
    fds[kb_count + i].fd = x11_mouse_fd(i);
    fds[kb_count + i].events = POLLIN;
  }
  struct input_event ev;
  while (wm->running) {
    int r = poll(fds, nfds, 100);
    if (r > 0) {
      for (int i = 0; i < kb_count; i++)
        if (fds[i].revents & POLLIN) {
          bool grabbed = x11_device_grabbed_get(i);
          while (read(fds[i].fd, &ev, sizeof(ev)) > 0) {
            bool has_popup = WindowManager_GetTopPopup(wm) != NULL;
            x11_handle_evdev(&ev, has_popup, grabbed);
          }
        }
      for (int i = 0; i < mouse_count; i++)
        if (fds[kb_count + i].revents & POLLIN) {
          bool mgrab = x11_mouse_grabbed_get(i);
          while (read(x11_mouse_fd(i), &ev, sizeof(ev)) > 0) {
            x11_handle_evdev_mouse(&ev);
            x11_mouse_forward(&ev, mgrab ? 1 : 0);
          }
        }
    }
  }
  free(fds);
  return NULL;
}

static void x11_drain_selection_complete(Display *dpy, WindowManager *wm) {
  WindowManager_DrainSelectionComplete(wm);
  XFlush(dpy);
}

static void x11_handle_key_press(Display *dpy, XKeyEvent *ev,
                                 WindowManager *wm) {

  int mods = 0;
  if (ev->state & ControlMask)
    mods |= HOTKEY_MOD_CTRL;
  if (ev->state & Mod1Mask)
    mods |= HOTKEY_MOD_ALT;
  if (ev->state & ShiftMask)
    mods |= HOTKEY_MOD_SHIFT;
  KeySym ks;
  char buf[32];
  int len = XLookupString(ev, buf, sizeof(buf), &ks, NULL);
  if (ks == XK_Control_L || ks == XK_Control_R)
    mods |= HOTKEY_MOD_CTRL;
  else if (ks == XK_Alt_L || ks == XK_Alt_R)
    mods |= HOTKEY_MOD_ALT;
  else if (ks == XK_Shift_L || ks == XK_Shift_R)
    mods |= HOTKEY_MOD_SHIFT;

  int vk_code = x11_keysym_to_vk(ks);
  int char_hint =
      (len == 1 && (unsigned char)buf[0] >= 32 && (unsigned char)buf[0] <= 126)
          ? (unsigned char)buf[0]
          : 0;
  int modifiers = mods;
  if (vk_code)
    input_state_update(vk_code, true, modifiers, char_hint);
  log_info("xkey ks=0x%lx len=%d buf0=%d vk=%d mods=%d state=0x%x",
           (unsigned long)ks, len, len > 0 ? (int)(unsigned char)buf[0] : -1,
           vk_code, modifiers, ev->state);

  Popup *top = WindowManager_GetTopPopup(wm);
  if (!top) {
    return;
  }

  PopupKeyEvent key;
  memset(&key, 0, sizeof(key));
  key.vk = x11_keysym_to_vk(ks);
  key.ctrl = (mods & HOTKEY_MOD_CTRL) != 0;
  key.shift = (mods & HOTKEY_MOD_SHIFT) != 0;
  key.alt = (mods & HOTKEY_MOD_ALT) != 0;
  if (len > 0) {
    key.text_len = len < (int)sizeof(key.text) ? len : (int)sizeof(key.text);
    memcpy(key.text, buf, key.text_len);
  }
  if (top->vtable && top->vtable->on_key)
    top->vtable->on_key(top, &key);

  x11_drain_selection_complete(dpy, wm);
  Popup *cur_top = WindowManager_GetTopPopup(wm);
  if (cur_top && cur_top->vtable && cur_top->vtable->on_render)
    cur_top->vtable->on_render(cur_top);
  XFlush(dpy);
}

static void x11_handle_key_release(XKeyEvent *ev) {

  KeySym ks;
  char buf[32];
  int len = XLookupString(ev, buf, sizeof(buf), &ks, NULL);
  int vk = x11_keysym_to_vk(ks);
  int mods = remap_current_mods();
  if (vk)
    input_state_update(vk, false, mods, 0);
}

static void x11_process_xevent(Display *dpy, XEvent *ev, WindowManager *wm) {
  if (x11_clipboard_handle_event(dpy, ev)) {
    return;
  }
  if (ev->type == ButtonPress && ev->xbutton.button == Button1) {
    if (Platform_Widget_DispatchClick((long)ev->xbutton.window))
      return;
  }
  Popup *top = WindowManager_GetTopPopup(wm);
  if (!top) {
    return;
  }
  switch (ev->type) {
  case KeyPress:
    x11_handle_key_press(dpy, &ev->xkey, wm);
    break;
  case KeyRelease:
    x11_handle_key_release(&ev->xkey);
    break;
  case ButtonPress:
    if (ev->xbutton.button == Button4 || ev->xbutton.button == Button5) {
      int wx = 0, wy = 0;
      if (Platform_GetMousePos(&wx, &wy))
        listbox_publish_mouse_pos(wx, wy);
      listbox_queue_scroll(ev->xbutton.button == Button4 ? -LISTBOX_WHEEL_ROWS
                                                         : LISTBOX_WHEEL_ROWS);
      if (top->vtable && top->vtable->on_render) {
        top->vtable->on_render(top);
      }
    } else if (ev->xbutton.button == Button1) {
      if (wm->popup_count > 1 &&
          !WindowManager_IsPointInTopPopup(wm, ev->xbutton.x_root,
                                           ev->xbutton.y_root)) {
        WindowManager_PopPopup(wm);
        Popup *new_top = WindowManager_GetTopPopup(wm);
        if (new_top && new_top->vtable && new_top->vtable->on_render)
          new_top->vtable->on_render(new_top);
        XFlush(dpy);
        return;
      }
      int wx = 0, wy = 0;
      if (Platform_WindowToRoot(top->handle, &wx, &wy)) {
        listbox_publish_mouse_pos(wx + ev->xbutton.x, wy + ev->xbutton.y);
      }
      if (top->type == POPUP_PROMPT && top->user_data) {
        PromptState *st = (PromptState *)top->user_data;
        bool ctrl = (ev->xbutton.state & ControlMask) != 0;
        bool shift = (ev->xbutton.state & ShiftMask) != 0;
        bool alt = (ev->xbutton.state & Mod1Mask) != 0;
        prompt_handle_list_click(st, ctrl, shift, alt);
      }
      x11_drain_selection_complete(dpy, wm);
      Popup *current_top = WindowManager_GetTopPopup(wm);
      if (current_top && current_top->vtable &&
          current_top->vtable->on_render) {
        current_top->vtable->on_render(current_top);
      }
      XFlush(dpy);
    }
    break;
  case Expose:
    if (top->vtable && top->vtable->on_render) {
      top->vtable->on_render(top);
    }
    break;
  case FocusIn:
    log_info("FocusIn win=%lu mode=%d detail=%d top=%lu count=%d sel=%d",
             (unsigned long)ev->xfocus.window, ev->xfocus.mode,
             ev->xfocus.detail, top ? (unsigned long)top->handle.xid : 0UL,
             wm ? wm->popup_count : 0,
             SkaldContext_Get()->window.selection_complete ? 1 : 0);
    break;
  case FocusOut: {
    Window lost_win = ev->xfocus.window;
    bool lost_is_top = top && WH_EQUALS(top->handle, WH_FROM_XID(lost_win));
    long long since_pop = Platform_GetWallClockMs() -
                          SkaldContext_Get()->window.last_popup_pop_ms;
    log_info(
        "FocusOut win=%lu mode=%d detail=%d lost_is_top=%d top=%lu count=%d "
        "since_pop=%lld sel=%d",
        (unsigned long)lost_win, ev->xfocus.mode, ev->xfocus.detail,
        lost_is_top ? 1 : 0, top ? (unsigned long)top->handle.xid : 0UL,
        wm ? wm->popup_count : 0, since_pop,
        SkaldContext_Get()->window.selection_complete ? 1 : 0);
    if (since_pop >= 0 && since_pop < 250) {
      break;
    }
    if (!lost_is_top) {
      break;
    }
    if (top->vtable && top->vtable->on_focus_lost) {
      top->vtable->on_focus_lost(top);
      break;
    }
    if (CloseOnLossOfFocus) {
      SkaldContext_Get()->window.selection_complete = true;
    }
    break;
  }
  }
  x11_drain_selection_complete(dpy, wm);
}

int WindowManager_Run(WindowManager *wm) {
  XEvent xev;

  feature_loader_service_builds();
  feature_loader_drain_retired_unloads();
  if (!wm)
    return -1;
  wm->running = true;
  x11_set_window_manager(wm);

  Display *dpy = wm->display;
  if (!dpy)
    return -1;

  bool use_grab = should_use_grab();
  if (!use_grab) {
    x11_uinput_fd_set(-1);
  } else {
    x11_uinput_fd_set(init_uinput());
    if (x11_uinput_fd_get() < 0) {
      log_warn("continuing without uinput (no swallow)");
    }
  }

  x11_scan_and_hook_keyboards();

  if (hotkey_has_mouse_binding()) {
    x11_mouse_grab_begin();
  }

  bool grab_keyboards = remap_has_bindings() || hotkey_has_keyboard_binding();
  if (grab_keyboards) {
    if (x11_uinput_fd_get() < 0 && !x11_grab_blocked()) {
      log_warn("input grab forcing uinput init despite use_grab=%d", use_grab);
      x11_uinput_fd_set(init_uinput());
      if (x11_uinput_fd_get() >= 0)
        log_warn("input grab uinput forced fd=%d", x11_uinput_fd_get());
      else
        log_warn("input grab uinput force failed, fallback XTest will be used");
    }
    if (x11_uinput_fd_get() >= 0) {
      x11_clear_suppress();
      remap_reset();
      input_state_clear();
      for (int i = 0; i < x11_keyboard_count_get(); i++) {
        if (!x11_device_grabbed_get(i)) {
          struct input_event drain_ev;
          while (read(x11_keyboard_fd(i), &drain_ev, sizeof(drain_ev)) > 0) {
          }
          if (ioctl(x11_keyboard_fd(i), EVIOCGRAB, 1) == 0) {
            x11_device_grabbed_set(i, true);
            unsigned long held[(KEY_MAX + 8) / 8] = {0};
            bool have_held =
                ioctl(x11_keyboard_fd(i), EVIOCGKEY(sizeof(held)), held) >= 0;
            if (have_held) {
              for (int k = 0; k <= KEY_MAX; k++) {
                if (held[k / (8 * (int)sizeof(unsigned long))] &
                    (1UL << (k % (8 * (int)sizeof(unsigned long))))) {
                  struct input_event rel_ev;
                  memset(&rel_ev, 0, sizeof(rel_ev));
                  gettimeofday(&rel_ev.time, NULL);
                  rel_ev.type = EV_KEY;
                  rel_ev.code = k;
                  rel_ev.value = 0;
                  x11_uinput_emit(&rel_ev);
                }
              }
#if __has_include(<X11/extensions/XTest.h>)
              if (dpy) {
                for (int k = 0; k <= KEY_MAX; k++) {
                  if (held[k / (8 * (int)sizeof(unsigned long))] &
                      (1UL << (k % (8 * (int)sizeof(unsigned long))))) {
                    XTestFakeKeyEvent(dpy, (unsigned int)(k + 8), False,
                                      CurrentTime);
                  }
                }
                XFlush(dpy);
              }
#endif
            }
          }
        }
      }
    } else {
      log_warn("keyboard grab skipped: no uinput, not grabbing to avoid bricking");
    }
  }
  if (dpy) {
    usleep(300000);
    if (XkbGetAutoRepeatRate(dpy, XkbUseCoreKbd, &x11_saved_delay,
                             &x11_saved_interval)) {
      x11_autorepeat_saved = true;
      XkbSetAutoRepeatRate(dpy, XkbUseCoreKbd, 300, 33);
      XSync(dpy, False);
    }
  }
  pthread_t input_thr = 0;
  bool input_started = false;
  if (x11_keyboard_count_get() > 0) {
    if (pthread_create(&input_thr, NULL, input_thread_func, wm) == 0)
      input_started = true;
    else
      log_error("failed to start input thread");
  } else {
    log_warn("no structural keyboard nodes under /dev/input/, hotkeys will use "
             "X11 events only");
  }

  while (wm->running) {
    Display *rdpy = (Display *)render_thread_display();
    struct pollfd xfds[3];
    int nfds = 0;
    int main_idx = -1, render_idx = -1, wake_idx = -1;
    int hk_fd = hotkey_wake_fd();
    if (hk_fd >= 0) {
      wake_idx = nfds;
      xfds[nfds].fd = hk_fd;
      xfds[nfds].events = POLLIN;
      xfds[nfds].revents = 0;
      nfds++;
    }
    if (rdpy && rdpy != dpy) {
      main_idx = nfds;
      xfds[nfds].fd = ConnectionNumber(dpy);
      xfds[nfds].events = POLLIN;
      xfds[nfds].revents = 0;
      nfds++;
      render_idx = nfds;
      xfds[nfds].fd = ConnectionNumber(rdpy);
      xfds[nfds].events = POLLIN;
      xfds[nfds].revents = 0;
      nfds++;
    } else {
      main_idx = 0;
      xfds[0].fd = ConnectionNumber(dpy);
      xfds[0].events = POLLIN;
      xfds[0].revents = 0;
      nfds = 1;
    }
    int poll_res = poll(xfds, (nfds_t)nfds, 50);
    if (wake_idx >= 0 && (xfds[wake_idx].revents & POLLIN)) {
      unsigned long long v;
      read(hk_fd, &v, sizeof(v));
    }
    if (poll_res < 0)
      break;
    if (main_idx >= 0 && (xfds[main_idx].revents & POLLIN)) {
      while (XPending(dpy) > 0) {
        XNextEvent(dpy, &xev);
        x11_process_xevent(dpy, &xev, wm);
      }
    }
    if (render_idx >= 0 && (xfds[render_idx].revents & POLLIN)) {
      while (XPending(rdpy) > 0) {
        XNextEvent(rdpy, &xev);
        if (xev.type == ButtonPress && xev.xbutton.button == Button1) {
          Platform_Widget_DispatchClick((long)xev.xbutton.window);
        }
      }
    }
    feature_loader_service_builds();
    hotkey_drain_pending();
  }

  if (input_started)
    pthread_join(input_thr, NULL);

  for (int i = 0; i < x11_keyboard_count_get(); i++) {
    if (x11_keyboard_fd(i) >= 0) {
      ioctl(x11_keyboard_fd(i), EVIOCGRAB, 0);
      close(x11_keyboard_fd(i));
      x11_keyboard_fd_set(i, -1);
    }
  }
  if (dpy && x11_autorepeat_saved) {
    XkbSetAutoRepeatRate(dpy, XkbUseCoreKbd, x11_saved_delay,
                         x11_saved_interval);
    XSync(dpy, False);
    x11_autorepeat_saved = false;
  }
  destroy_uinput();
  render_thread_stop();
  x11_set_window_manager(NULL);
  return 0;
}

void WindowManager_ProcessEvents(WindowManager *wm) {
  if (!wm || !wm->display) {
    return;
  }

  Display *dpy = wm->display;

  struct pollfd xfd = {.fd = ConnectionNumber(dpy), .events = POLLIN};
  poll(&xfd, 1, 16);

  XEvent ev;

  while (XPending(dpy) > 0) {
    XNextEvent(dpy, &ev);
    x11_process_xevent(dpy, &ev, wm);
  }

  Display *rdpy = (Display *)render_thread_display();
  if (rdpy && rdpy != dpy) {
    struct pollfd rfd = {.fd = ConnectionNumber(rdpy), .events = POLLIN};
    poll(&rfd, 1, 0);
    while (XPending(rdpy) > 0) {
      XNextEvent(rdpy, &ev);
      if (ev.type == ButtonPress && ev.xbutton.button == Button1) {
        Platform_Widget_DispatchClick((long)ev.xbutton.window);
      }
    }
  }
  feature_loader_service_builds();
  hotkey_drain_pending();
}

void WindowManager_Quit(WindowManager *wm) {
  if (wm)
    wm->running = false;
}
