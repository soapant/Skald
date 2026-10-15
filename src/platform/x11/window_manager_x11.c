#include "core/window_manager.h"
#include "deps/log.h"
#include "core/prompt_controller.h"

#include "platform/input_state.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrandr.h>
#if __has_include(<X11/extensions/XTest.h>)
#include <X11/extensions/XTest.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <poll.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include "platform/x11/clipboard_x11.h"
#include "platform/x11/input_x11.h"
#include "platform/render_thread.h"
#include "platform/widget.h"
#include "core/timer.h"
#include "core/remap_manager.h"
#include "core/hotkey_manager.h"


int pending_hotkey_vk = 0;
int pending_hotkey_mod = 0;
long pending_hotkey_timeMs = 0;

static bool should_use_grab(void) {
    PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "should_use_grab"); return x11_should_use_grab(); } 
}

static int init_uinput(void) {
    PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "init_uinput"); return x11_init_uinput(); } 
}
static void destroy_uinput(void) {
    PROFILE_START(_pt);
x11_mouse_grab_end();
x11_destroy_uinput(); 
}
static void ScanAndHookKeyboards(void) {
    PROFILE_START(_pt);
x11_scan_keyboards(); 
}

static int XKeysymToVK(KeySym ks) {
    PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "XKeysymToVK"); return x11_keysym_to_vk(ks); } 
}

static void* input_thread_func(void* arg) {
        PROFILE_START(_pt);
WindowManager* wm = (WindowManager*)arg;
    if (g_x11_keyboard_count == 0 && g_x11_mouse_count == 0) { PROFILE_END_VAR(_pt, "input_thread_func"); return NULL; }
    int nfds = g_x11_keyboard_count + g_x11_mouse_count;
    struct pollfd *fds = calloc((size_t)nfds, sizeof(struct pollfd));
    for (int i=0;i<g_x11_keyboard_count;i++) { fds[i].fd = g_x11_keyboards[i].fd; fds[i].events = POLLIN; }
    for (int i=0;i<g_x11_mouse_count;i++) { fds[g_x11_keyboard_count+i].fd = g_x11_mice[i].fd; fds[g_x11_keyboard_count+i].events = POLLIN; }
    struct input_event ev;
    while (wm->running) {
        int r = poll(fds, nfds, 100);
        if (r > 0) {
            for (int i=0;i<g_x11_keyboard_count;i++) if (fds[i].revents & POLLIN) {
                bool grabbed = g_x11_device_grabbed[i];
                while (read(fds[i].fd, &ev, sizeof(ev)) > 0) {
                    bool has_popup = WindowManager_GetTopPopup(wm) != NULL;
                    x11_handle_evdev(&ev, has_popup, grabbed);
                }
            }
            for (int i=0;i<g_x11_mouse_count;i++) if (fds[g_x11_keyboard_count+i].revents & POLLIN) {
                bool mgrab = g_x11_mouse_grabbed[i];
                while (read(g_x11_mice[i].fd, &ev, sizeof(ev)) > 0) {
                    x11_handle_evdev_mouse(&ev);
                    x11_mouse_forward(&ev, mgrab ? 1 : 0);
                }
            }
        }
    }
    free(fds);
    { PROFILE_END_VAR(_pt, "input_thread_func"); return NULL; }
}

static void x11_handle_key_press(Display* dpy, XKeyEvent* ev, WindowManager* wm) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    int mods = 0;
    if (ev->state & ControlMask) mods |= HOTKEY_MOD_CTRL;
    if (ev->state & Mod1Mask) mods |= HOTKEY_MOD_ALT;
    if (ev->state & ShiftMask) mods |= HOTKEY_MOD_SHIFT;
    KeySym ks;
    char buf[32];
    int len = XLookupString(ev, buf, sizeof(buf), &ks, NULL);
    if (ks == XK_Control_L || ks == XK_Control_R) mods |= HOTKEY_MOD_CTRL;
    else if (ks == XK_Alt_L || ks == XK_Alt_R) mods |= HOTKEY_MOD_ALT;
    else if (ks == XK_Shift_L || ks == XK_Shift_R) mods |= HOTKEY_MOD_SHIFT;

    int vk_code = 0;
    if (len == 1 && (unsigned char)buf[0] >= 32 && (unsigned char)buf[0] <= 126) {
        vk_code = (unsigned char)buf[0];
    } else {
        vk_code = XKeysymToVK(ks);
    }
    int modifiers = mods;
    if(vk_code) Platform_InputState_Update(vk_code, true, modifiers);
    log_info("xkey ks=0x%lx len=%d buf0=%d vk=%d mods=%d state=0x%x", (unsigned long)ks, len, len > 0 ? (int)(unsigned char)buf[0] : -1, vk_code, modifiers, ev->state);


    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) { PROFILE_END_VAR(timer, "window_manager_x11:x11_handle_key_press"); return; }

    PopupKeyEvent key;
    memset(&key, 0, sizeof(key));
    key.vk = x11_keysym_to_windows_vk(ks);
    key.ctrl = (mods & HOTKEY_MOD_CTRL) != 0;
    key.shift = (mods & HOTKEY_MOD_SHIFT) != 0;
    key.alt = (mods & HOTKEY_MOD_ALT) != 0;
    if (len > 0) {
        key.text_len = len < (int)sizeof(key.text) ? len : (int)sizeof(key.text);
        memcpy(key.text, buf, key.text_len);
    }
    if (top->vtable && top->vtable->on_key) top->vtable->on_key(top, &key);

    if (FlintContext_Get()->window.selection_complete) {
        FlintContext_Get()->window.selection_complete = false;
        WindowManager_PopPopup(wm);
    }

    Popup* top_after = WindowManager_GetTopPopup(wm);
    if (top_after && top_after->vtable && top_after->vtable->on_render) {
        top_after->vtable->on_render(top_after);
    }
    XFlush(dpy);
    PROFILE_END_VAR(timer, "window_manager_x11:x11_handle_key_press");
}

static void x11_handle_key_release(Display* dpy, XKeyEvent* ev) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    KeySym ks; char buf[32]; int len=XLookupString(ev, buf, sizeof(buf), &ks, NULL);
    int vk=0; if(len==1 && (unsigned char)buf[0]>=32 && (unsigned char)buf[0]<=126) vk=(unsigned char)buf[0]; else vk=XKeysymToVK(ks);
    int mods=remap_current_mods();
    if(vk) Platform_InputState_Update(vk, false, mods);
    PROFILE_END_VAR(timer, "window_manager_x11:x11_handle_key_release");
}

void Linux_ProcessXEvent(Display* dpy, XEvent* ev, WindowManager* wm) {
        PROFILE_START(_pt);
if (x11_clipboard_handle_event(dpy, ev)) { PROFILE_END_VAR(_pt, "Linux_ProcessXEvent"); return ; } 
    if (ev->type == ButtonPress && ev->xbutton.button == Button1) {
        if (Platform_Widget_DispatchClick((long)ev->xbutton.window)) return;
    }
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) { PROFILE_END_VAR(_pt, "Linux_ProcessXEvent"); return ; }
    switch (ev->type) {
        case KeyPress:
            x11_handle_key_press(dpy, &ev->xkey, wm);
            break;
        case KeyRelease:
            x11_handle_key_release(dpy, &ev->xkey);
            break;
        case Expose:
            if (top->vtable && top->vtable->on_render) {
                top->vtable->on_render(top);
            }
            break;
        case FocusIn:
            break;
        case FocusOut:
            if (top->vtable && top->vtable->on_focus_lost) {
                top->vtable->on_focus_lost(top);
            }
            break;
    }
}

int WindowManager_RunLinux(WindowManager* wm) {
        PROFILE_START(_pt);
XEvent xev;
    PROFILE_START(timer);
    if (!wm) { PROFILE_END_VAR(timer, "window_manager_x11:WindowManager_RunLinux"); return -1; }
    wm->running = true;
    x11_set_window_manager(wm);

    Display* dpy = wm->display;
    if (!dpy) { PROFILE_END_VAR(timer, "window_manager_x11:WindowManager_RunLinux"); return -1; }

    bool use_grab = should_use_grab();
    if (!use_grab) {
        g_x11_uinput_fd = -1;
    } else {
        g_x11_uinput_fd = init_uinput();
        if (g_x11_uinput_fd < 0) {
            log_warn("continuing without uinput (no swallow)");
        }
    }

    ScanAndHookKeyboards();

    if (hotkey_has_mouse_binding()) {
        x11_mouse_grab_begin();
    }

    if (remap_has_bindings()) {
        if (g_x11_uinput_fd < 0) {
            log_warn("remap forcing uinput init despite use_grab=%d", use_grab);
            g_x11_uinput_fd = init_uinput();
            if (g_x11_uinput_fd >= 0) log_warn("remap uinput forced fd=%d", g_x11_uinput_fd);
            else log_warn("remap uinput force failed, fallback XTest will be used");
        }
        if (g_x11_uinput_fd >= 0) {
            x11_clear_suppress();
            remap_reset();
            Platform_InputState_Clear();
            x11_sync_held_from_phys();
            for (int i=0;i<g_x11_keyboard_count;i++) {
                if (!g_x11_device_grabbed[i]) {
                    struct input_event drain_ev;
                    while (read(g_x11_keyboards[i].fd, &drain_ev, sizeof(drain_ev)) > 0) {}
                    if (ioctl(g_x11_keyboards[i].fd, EVIOCGRAB, 1)==0) {
                        g_x11_device_grabbed[i]=true;
                        unsigned long held[(KEY_MAX + 8) / 8] = {0};
                        bool have_held = ioctl(g_x11_keyboards[i].fd, EVIOCGKEY(sizeof(held)), held) >= 0;
                        if (have_held) {
                            for (int k = 0; k <= KEY_MAX; k++) {
                                if (held[k / (8 * (int)sizeof(unsigned long))] & (1UL << (k % (8 * (int)sizeof(unsigned long))))) {
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
                                    if (held[k / (8 * (int)sizeof(unsigned long))] & (1UL << (k % (8 * (int)sizeof(unsigned long))))) {
                                        XTestFakeKeyEvent(dpy, (unsigned int)(k + 8), False, CurrentTime);
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
            log_warn("remap grab skipped: no uinput, not grabbing to avoid bricking");
        }
    }
    if (dpy) {
        usleep(300000);
        XkbSetAutoRepeatRate(dpy, XkbUseCoreKbd, 300, 33);
        XSync(dpy, False);
    }
    pthread_t input_thr = 0;
    bool input_started = false;
    if (g_x11_keyboard_count > 0) {
        if (pthread_create(&input_thr, NULL, input_thread_func, wm) == 0) input_started = true;
        else log_error("failed to start input thread");
    } else {
        log_warn("no structural keyboard nodes under /dev/input/, hotkeys will use X11 events only");
    }


    while (wm->running) {
        Display* rdpy = (Display*)Platform_RenderThread_Display();
        struct pollfd xfds[2];
        int nfds = 0;
        int main_idx = -1, render_idx = -1;
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
        if (poll_res < 0) break;
        if (main_idx >= 0 && (xfds[main_idx].revents & POLLIN)) {
            while (XPending(dpy) > 0) {
                XNextEvent(dpy, &xev);
                Linux_ProcessXEvent(dpy, &xev, wm);
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
        hotkey_drain_pending();
        if (PromptController_PollSequenceTimeout()) {
            Popup* top = WindowManager_GetTopPopup(wm);
            if (top && top->vtable && top->vtable->on_render) top->vtable->on_render(top);
            XFlush(dpy);
        }
    }

    if (input_started) pthread_join(input_thr, NULL);

    for (int i = 0; i < g_x11_keyboard_count; i++) {
        if (g_x11_keyboards[i].fd >= 0) {
            ioctl(g_x11_keyboards[i].fd, EVIOCGRAB, 0);
            close(g_x11_keyboards[i].fd);
            g_x11_keyboards[i].fd = -1;
        }
    }
    destroy_uinput();
    Platform_RenderThread_Stop();
    x11_set_window_manager(NULL);

    PROFILE_END_VAR(timer, "window_manager_x11:WindowManager_RunLinux");
    { PROFILE_END_VAR(_pt, "WindowManager_RunLinux"); return 0; }
}

void WindowManager_ProcessEventsLinux(WindowManager* wm) {
        PROFILE_START(_pt);
if (!wm || !wm->display) { PROFILE_END_VAR(_pt, "WindowManager_ProcessEventsLinux"); return ; }

    Display* dpy = wm->display;

    struct pollfd xfd = { .fd = ConnectionNumber(dpy), .events = POLLIN };
    poll(&xfd, 1, 16);

    XEvent ev;

    while (XPending(dpy) > 0) {
        XNextEvent(dpy, &ev);
        Linux_ProcessXEvent(dpy, &ev, wm);
    }

    Display* rdpy = (Display*)Platform_RenderThread_Display();
    if (rdpy && rdpy != dpy) {
        struct pollfd rfd = { .fd = ConnectionNumber(rdpy), .events = POLLIN };
        poll(&rfd, 1, 0);
        while (XPending(rdpy) > 0) {
            XNextEvent(rdpy, &ev);
            if (ev.type == ButtonPress && ev.xbutton.button == Button1) {
                Platform_Widget_DispatchClick((long)ev.xbutton.window);
            }
        }
    }
}

void WindowManager_QuitLinux(WindowManager* wm) {
        PROFILE_START(_pt);
if (wm) wm->running = false;
}
