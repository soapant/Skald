#include "core/window_manager.h"
#include "deps/log.h"

#include "core/common_ui.h"
#include "core/modal.h"
#include "core/vim.h"
#include "core/platform.h"
#include "platform/x11/ui_x11.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include "platform/x11/subclasses_x11.h"
#include "core/feature_data.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrandr.h>
#include <stdio.h>
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
#include <errno.h>
#include "platform/x11/display.h"
#include "platform/x11/input.h"
#include "core/timer.h"
#include "core/history.h"
#include "core/remap_manager.h"


extern bool HandleGlobalKeyEvent(int key_code, int value);

int pending_hotkey_vk = 0;
int pending_hotkey_mod = 0;
long pending_hotkey_timeMs = 0;

static bool should_use_grab(void) { return x11_should_use_grab(); }

static int init_uinput(void) { return x11_init_uinput(); }
static void destroy_uinput(void) { x11_destroy_uinput(); }
static void uinput_emit(const struct input_event *ev) { x11_uinput_emit(ev); }
static void ScanAndHookKeyboards(void) { x11_scan_keyboards(); }

static int XKeysymToVK(KeySym ks) { return x11_keysym_to_vk(ks); }

static void Linux_HandleKeyPress(Display* dpy, XKeyEvent* ev, WindowManager* wm) {
    PROFILE_START(timer);
    extern bool g_x11_ctrl_held, g_x11_alt_held, g_x11_shift_held;
    g_x11_ctrl_held = (ev->state & ControlMask) != 0;
    g_x11_ctrl_held = g_x11_ctrl_held;
    g_x11_alt_held = (ev->state & Mod1Mask) != 0;
    g_x11_alt_held = g_x11_alt_held;
    g_x11_shift_held = (ev->state & ShiftMask) != 0;
    g_x11_shift_held = g_x11_shift_held;

    KeySym ks;
    char buf[32];
    int len = XLookupString(ev, buf, sizeof(buf), &ks, NULL);

    if (ks == XK_Control_L || ks == XK_Control_R) { g_x11_ctrl_held = true; g_x11_ctrl_held = true; }
    else if (ks == XK_Alt_L || ks == XK_Alt_R) { g_x11_alt_held = true; g_x11_alt_held = true; }
    else if (ks == XK_Shift_L || ks == XK_Shift_R) { g_x11_shift_held = true; g_x11_shift_held = true; }

    int vk_code = 0;
    if (len == 1 && (unsigned char)buf[0] >= 32 && (unsigned char)buf[0] <= 126) {
        vk_code = (unsigned char)buf[0];
    } else {
        vk_code = XKeysymToVK(ks);
    }
    bool pressed = true;
    int modifiers = 0;
    if (g_x11_alt_held) modifiers |= HOTKEY_MOD_ALT;
    if (g_x11_shift_held) modifiers |= HOTKEY_MOD_SHIFT;
    if (g_x11_ctrl_held) modifiers |= HOTKEY_MOD_CTRL;

    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) { PROFILE_END_VAR(timer, "window_manager_x11:Linux_HandleKeyPress"); return; }

    if (top->type == POPUP_PROMPT) {
        PromptState* state = (PromptState*)top->user_data;
        XEvent xev; memset(&xev,0,sizeof(xev));
        xev.type = KeyPress;
        xev.xkey = *ev;
        bool handled = Linux_HandlePromptKey(&xev, state);
        if (!handled && IsInsertMode()) {
            char buf2[32]; KeySym ks2; int len2 = XLookupString(ev, buf2, sizeof(buf2), &ks2, NULL);
            if (len2 > 0) {
                extern int pending_hotkey_vk; extern int pending_hotkey_mod; extern long pending_hotkey_timeMs;
                bool swallow=false;
                if (pending_hotkey_vk!=0 && len2==1) {
                    struct timeval tv; gettimeofday(&tv,NULL); long now=tv.tv_sec*1000+tv.tv_usec/1000;
                    if (now - pending_hotkey_timeMs < 800) {
                        unsigned char c=(unsigned char)buf2[0];
                        int vkc=(c>='a'&&c<='z')?(c-'a'+'A'):(c>='A'&&c<='Z')?c:(c==' '?VK_SPACE:0);
                        if (vkc!=0 && vkc==pending_hotkey_vk) swallow=true;
                    }
                    if (swallow) pending_hotkey_vk=0;
                    else if (now - pending_hotkey_timeMs >=800) pending_hotkey_vk=0;
                }
                if (!swallow) {
                    bool is_printable=true;
                    for (int i=0;i<len2;i++) { unsigned char c=(unsigned char)buf2[i]; if (c<0x20||c==0x7F) {is_printable=false;break;}}
                    if (is_printable) {
                        size_t n=strlen(state->filter);
                        if (n+len2 < sizeof(state->filter)-1) {
                            if (state->cursor<0) state->cursor=0;
                            if ((size_t)state->cursor>n) state->cursor=n;
                            memmove(state->filter+state->cursor+len2, state->filter+state->cursor, n-state->cursor+1);
                            memcpy(state->filter+state->cursor, buf2, len2);
                            state->cursor+=len2;
                            History_ResetCursor(FlintContext_Get()->window.current_feature);
                            FlintContext* _c = FlintContext_Get();
                            if (_c->prompt_callbacks.refresh) _c->prompt_callbacks.refresh(state->filter, FlintContext_Get()->window.hListBox, state->has_listbox ? &state->listbox : NULL);
                            else if (state && state->listbox.populated_count >=0) {}
                        }
                    }
                }
            }
        }
        FlintContext* _c2 = FlintContext_Get();
        if (_c2->window.selection_complete) {
            _c2->window.selection_complete = false;
            WindowManager_PopPopup(wm);
            Popup* new_top = WindowManager_GetTopPopup(wm);
            if (new_top && new_top->vtable && new_top->vtable->on_render) new_top->vtable->on_render(new_top);
            XFlush(dpy);
            PROFILE_END_VAR(timer, "window_manager_x11:Linux_HandleKeyPress");
            return;
        }
        if (top->vtable && top->vtable->on_render) top->vtable->on_render(top);
        XFlush(dpy);
        PROFILE_END_VAR(timer, "window_manager_x11:Linux_HandleKeyPress");
        return;
    }

    Popup* top_before = top;
    if (top_before && top_before->vtable && top_before->vtable->on_key) {
        top_before->vtable->on_key(top_before, vk_code, pressed, modifiers);
    }
    Popup* top_after = WindowManager_GetTopPopup(wm);
    if (top_after && top_after->vtable && top_after->vtable->on_render) {
        top_after->vtable->on_render(top_after);
    }
    XFlush(dpy);
    PROFILE_END_VAR(timer, "window_manager_x11:Linux_HandleKeyPress");
}

static void Linux_HandleKeyRelease(Display* dpy, XKeyEvent* ev) {
    PROFILE_START(timer);
    int keycode = ev->keycode;
    KeySym ks = XKeycodeToKeysym(dpy, keycode, 0);
    extern bool g_x11_ctrl_held, g_x11_alt_held, g_x11_shift_held;
    if (ks == XK_Control_L || ks == XK_Control_R) { g_x11_ctrl_held = false; g_x11_ctrl_held = false; }
    else if (ks == XK_Alt_L || ks == XK_Alt_R) { g_x11_alt_held = false; g_x11_alt_held = false; }
    else if (ks == XK_Shift_L || ks == XK_Shift_R) { g_x11_shift_held = false; g_x11_shift_held = false; }
    PROFILE_END_VAR(timer, "window_manager_x11:Linux_HandleKeyRelease");
}

void Linux_ProcessXEvent(Display* dpy, XEvent* ev, WindowManager* wm) {
    PROFILE_START(timer);
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) { PROFILE_END_VAR(timer, "window_manager_x11:Linux_ProcessXEvent"); return; }
    switch (ev->type) {
        case KeyPress:
            Linux_HandleKeyPress(dpy, &ev->xkey, wm);
            break;
        case KeyRelease:
            Linux_HandleKeyRelease(dpy, &ev->xkey);
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
    PROFILE_END_VAR(timer, "window_manager_x11:Linux_ProcessXEvent");
}

int WindowManager_RunLinux(WindowManager* wm) {
    PROFILE_START(timer);
    if (!wm) { PROFILE_END_VAR(timer, "window_manager_x11:WindowManager_RunLinux"); return -1; }
    wm->running = true;

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

    if (remap_has_bindings()) {
        if (g_x11_uinput_fd < 0) {
            log_warn("remap forcing uinput init despite use_grab=%d", use_grab);
            g_x11_uinput_fd = init_uinput();
            if (g_x11_uinput_fd >= 0) log_warn("remap uinput forced fd=%d", g_x11_uinput_fd);
            else log_warn("remap uinput force failed, fallback XTest will be used");
        }
        for (int i=0;i<g_x11_keyboard_count;i++) {
            if (!g_x11_device_grabbed[i]) {
                if (ioctl(g_x11_keyboards[i].fd, EVIOCGRAB, 1)==0) {
                    g_x11_device_grabbed[i]=true;
                    log_warn("remap grabbed keyboard %d path=%s", i, g_x11_keyboards[i].path);
                } else {
                    log_warn("remap grab failed for %s: %s (run with correct perms / input group)", g_x11_keyboards[i].path, strerror(errno));
                }
            }
        }
    }
    if (g_x11_keyboard_count == 0) {
        log_error("No structural keyboard nodes located under /dev/input/");
        destroy_uinput();
        PROFILE_END_VAR(timer, "window_manager_x11:WindowManager_RunLinux");
        return -1;
    }

    struct pollfd *fds = calloc(g_x11_keyboard_count + 1, sizeof(struct pollfd));
    fds[0].fd = ConnectionNumber(dpy);
    fds[0].events = POLLIN;
    for (int i = 0; i < g_x11_keyboard_count; i++) {
        fds[i + 1].fd = g_x11_keyboards[i].fd;
        fds[i + 1].events = POLLIN;
    }

    XEvent xev;
    struct input_event ev;
    const long HOTKEY_STALE_EVENT_MS = 200;

    while (wm->running) {
        int poll_res = poll(fds, g_x11_keyboard_count + 1, 500);
        if (poll_res < 0) break;

        if (fds[0].revents & POLLIN) {
            while (XPending(dpy) > 0) {
                XNextEvent(dpy, &xev);
                Linux_ProcessXEvent(dpy, &xev, wm);
            }
        }

        for (int i = 0; i < g_x11_keyboard_count; i++) {
            if (fds[i + 1].revents & POLLIN) {
                bool grabbed = g_x11_device_grabbed[i];
                while (read(fds[i + 1].fd, &ev, sizeof(struct input_event)) > 0) {
                    Popup* top_for_hotkey = WindowManager_GetTopPopup(wm);
                    if (top_for_hotkey) {
                        extern bool g_x11_ctrl_held, g_x11_alt_held, g_x11_shift_held;
                        if (ev.type == EV_KEY) {
                            if (ev.code == KEY_LEFTCTRL || ev.code == KEY_RIGHTCTRL) {
                                if (ev.value == 0) g_x11_ctrl_held = false;
                                else if (ev.value == 1 || ev.value == 2) g_x11_ctrl_held = true;
                            } else if (ev.code == KEY_LEFTALT || ev.code == KEY_RIGHTALT) {
                                if (ev.value == 0) g_x11_alt_held = false;
                                else if (ev.value == 1 || ev.value == 2) g_x11_alt_held = true;
                            } else if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT) {
                                if (ev.value == 0) g_x11_shift_held = false;
                                else if (ev.value == 1 || ev.value == 2) g_x11_shift_held = true;
                            }
                            if (ev.code == KEY_LEFTCTRL || ev.code == KEY_RIGHTCTRL) {
                                if (ev.value == 0) g_x11_ctrl_held = false;
                                else g_x11_ctrl_held = true;
                            } else if (ev.code == KEY_LEFTALT || ev.code == KEY_RIGHTALT) {
                                if (ev.value == 0) g_x11_alt_held = false;
                                else g_x11_alt_held = true;
                            } else if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT) {
                                if (ev.value == 0) g_x11_shift_held = false;
                                else g_x11_shift_held = true;
                            }
                            if (grabbed && g_x11_uinput_fd >= 0) uinput_emit(&ev);
                        } else {
                            if (grabbed && g_x11_uinput_fd >= 0) uinput_emit(&ev);
                        }
                        continue;
                    }
                    
                    if (ev.type != EV_KEY) {
                        if (grabbed && g_x11_uinput_fd >= 0) uinput_emit(&ev);
                        continue;
                    }

                    if (remap_should_swallow_linux_key(ev.code, ev.value)) {
                        log_warn("window_manager_x11: remap swallowed code=%d val=%d grabbed=%d uinput=%d", ev.code, ev.value, grabbed, g_x11_uinput_fd);
                        if (ev.value == 1 && ev.code <= KEY_MAX) g_x11_suppress_release[ev.code] = true;
                        continue;
                    } else {
                        if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTALT || ev.code == KEY_W || ev.code == KEY_A || ev.code == KEY_S || ev.code == KEY_D || ev.code == KEY_I || ev.code == KEY_U) {
                            log_warn("window_manager_x11: remap NOT swallowed code=%d val=%d", ev.code, ev.value);
                        }
                        remap_update_phys(ConvertLinuxKeyToWindowsVK(ev.code), ev.value);
                    }

                    struct timeval now;
                    gettimeofday(&now, NULL);
                    long age_ms = (now.tv_sec - ev.time.tv_sec) * 1000L
                                + (now.tv_usec - ev.time.tv_usec) / 1000L;
                    if (age_ms > HOTKEY_STALE_EVENT_MS) {
                        bool is_modifier = (ev.code == KEY_LEFTALT || ev.code == KEY_RIGHTALT ||
                                            ev.code == KEY_LEFTCTRL || ev.code == KEY_RIGHTCTRL ||
                                            ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT ||
                                            ev.code == KEY_LEFTMETA || ev.code == KEY_RIGHTMETA);
                        if (!is_modifier) {
                            if (ev.value == 1 && !grabbed) {
                                if (HandleGlobalKeyEvent(ev.code, ev.value)) {
                                    continue;
                                }
                            }
                            if (grabbed && g_x11_uinput_fd >= 0) uinput_emit(&ev);
                            continue;
                        }
                        if (ev.value == 1) {
                            if (grabbed && g_x11_uinput_fd >= 0) uinput_emit(&ev);
                            continue;
                        }
                    }

                    if (ev.value == 0 && ev.code <= KEY_MAX && g_x11_suppress_release[ev.code]) {
                        g_x11_suppress_release[ev.code] = false;
                        continue;
                    }
                    if (ev.value == 2 && ev.code <= KEY_MAX && g_x11_suppress_release[ev.code]) {
                        continue;
                    }

                    if (HandleGlobalKeyEvent(ev.code, ev.value)) {
                        if (ev.value == 1 && ev.code <= KEY_MAX) {
                            g_x11_suppress_release[ev.code] = true;
                        } else if (ev.value == 2 && ev.code <= KEY_MAX) {
                            g_x11_suppress_release[ev.code] = true;
                        }
                        continue;
                    }
                    
                    if (grabbed) {
                        if (g_x11_uinput_fd >= 0) {
                            uinput_emit(&ev);
                        } else {
                            static bool warned = false;
                            if (!warned) {
                                log_warn("grabbed but no uinput - key %d lost. Check /dev/uinput perms (need input group or ACL)", ev.code);
                                warned = true;
                            }
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i < g_x11_keyboard_count; i++) {
        ioctl(g_x11_keyboards[i].fd, EVIOCGRAB, 0);
        close(g_x11_keyboards[i].fd);
    }
    free(fds);
    destroy_uinput();

    PROFILE_END_VAR(timer, "window_manager_x11:WindowManager_RunLinux");
    return 0;
}

void WindowManager_ProcessEventsLinux(WindowManager* wm) {
    PROFILE_START(timer);
    if (!wm || !wm->display) { PROFILE_END_VAR(timer, "window_manager_x11:WindowManager_ProcessEventsLinux"); return; }
    
    Display* dpy = wm->display;
    XEvent ev;
    
    while (XPending(dpy) > 0) {
        XNextEvent(dpy, &ev);
        Linux_ProcessXEvent(dpy, &ev, wm);
    }
    PROFILE_END_VAR(timer, "window_manager_x11:WindowManager_ProcessEventsLinux");
}

void Platform_ProcessNestedEvents(WindowManager* wm, Popup* popup) {
    PROFILE_START(timer);
    if (!wm || !wm->display) { PROFILE_END_VAR(timer, "platform:Platform_ProcessNestedEvents"); return; }
    
    Display* dpy = wm->display;
    XEvent ev;
    
    while (XPending(dpy) > 0) {
        XNextEvent(dpy, &ev);
        Linux_ProcessXEvent(dpy, &ev, wm);
    }
    
    extern int g_x11_keyboard_count;
    extern X11Keyboard g_x11_keyboards[];
    extern bool g_x11_device_grabbed[];
    extern int g_x11_uinput_fd;
    
    if (g_x11_keyboard_count > 0 && g_x11_uinput_fd >= 0) {
        struct pollfd fds[g_x11_keyboard_count + 1];
        fds[0].fd = ConnectionNumber(dpy);
        fds[0].events = POLLIN;
        
        for (int i = 0; i < g_x11_keyboard_count; i++) {
            fds[i + 1].fd = g_x11_keyboards[i].fd;
            fds[i + 1].events = POLLIN;
        }
        
        int poll_res = poll(fds, g_x11_keyboard_count + 1, 500);
        if (poll_res > 0) {
            for (int i = 0; i < g_x11_keyboard_count; i++) {
                if (fds[i + 1].revents & POLLIN) {
                    struct input_event ev;
                    bool grabbed = g_x11_device_grabbed[i];
                    while (read(fds[i + 1].fd, &ev, sizeof(struct input_event)) > 0) {
                        Popup* top_for_hotkey = WindowManager_GetTopPopup(wm);
                        if (top_for_hotkey) {
                            if (ev.type == EV_KEY) {
                                if (ev.code == KEY_LEFTCTRL || ev.code == KEY_RIGHTCTRL) {
                                    if (ev.value == 0) g_x11_ctrl_held = false;
                                    else if (ev.value == 1 || ev.value == 2) g_x11_ctrl_held = true;
                                } else if (ev.code == KEY_LEFTALT || ev.code == KEY_RIGHTALT) {
                                    if (ev.value == 0) g_x11_alt_held = false;
                                    else if (ev.value == 1 || ev.value == 2) g_x11_alt_held = true;
                                } else if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT) {
                                    if (ev.value == 0) g_x11_shift_held = false;
                                    else if (ev.value == 1 || ev.value == 2) g_x11_shift_held = true;
                                }
                                if (ev.code == KEY_LEFTCTRL || ev.code == KEY_RIGHTCTRL) {
                                    if (ev.value == 0) g_x11_ctrl_held = false;
                                    else g_x11_ctrl_held = true;
                                } else if (ev.code == KEY_LEFTALT || ev.code == KEY_RIGHTALT) {
                                    if (ev.value == 0) g_x11_alt_held = false;
                                    else g_x11_alt_held = true;
                                } else if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT) {
                                    if (ev.value == 0) g_x11_shift_held = false;
                                    else g_x11_shift_held = true;
                                }
                                if (grabbed && g_x11_uinput_fd >= 0) uinput_emit(&ev);
                            } else {
                                if (grabbed && g_x11_uinput_fd >= 0) uinput_emit(&ev);
                            }
                            continue;
                        }
                        
                        if (ev.type != EV_KEY) {
                            if (grabbed && g_x11_uinput_fd >= 0) uinput_emit(&ev);
                            continue;
                        }
                        
                        if (remap_should_swallow_linux_key(ev.code, ev.value)) {
                            log_warn("platform: nested remap swallowed code=%d val=%d", ev.code, ev.value);
                            if (ev.value == 1 && ev.code <= KEY_MAX) g_x11_suppress_release[ev.code] = true;
                            continue;
                        }
                        
                        struct timeval now;
                        gettimeofday(&now, NULL);
                        long age_ms = (now.tv_sec - ev.time.tv_sec) * 1000L
                                    + (now.tv_usec - ev.time.tv_usec) / 1000L;
                        const long HOTKEY_STALE_EVENT_MS = 200;
                        if (age_ms > HOTKEY_STALE_EVENT_MS) {
                            bool is_modifier = (ev.code == KEY_LEFTALT || ev.code == KEY_RIGHTALT ||
                                                ev.code == KEY_LEFTCTRL || ev.code == KEY_RIGHTCTRL ||
                                                ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT ||
                                                ev.code == KEY_LEFTMETA || ev.code == KEY_RIGHTMETA);
                            if (!is_modifier) {
                                if (ev.value == 1 && !grabbed) {
                                    if (HandleGlobalKeyEvent(ev.code, ev.value)) {
                                        continue;
                                    }
                                }
                                if (grabbed && g_x11_uinput_fd >= 0) uinput_emit(&ev);
                                continue;
                            }
                            if (ev.value == 1) {
                                if (grabbed && g_x11_uinput_fd >= 0) uinput_emit(&ev);
                                continue;
                            }
                        }
                        
                        if (ev.value == 0 && ev.code <= KEY_MAX && g_x11_suppress_release[ev.code]) {
                            g_x11_suppress_release[ev.code] = false;
                            continue;
                        }
                        if (ev.value == 2 && ev.code <= KEY_MAX && g_x11_suppress_release[ev.code]) {
                            continue;
                        }
                    }
                    
                    if (grabbed && g_x11_uinput_fd >= 0) uinput_emit(&ev);
                }
            }
        }
    }
    PROFILE_END_VAR(timer, "platform:Platform_ProcessNestedEvents");
}

void WindowManager_QuitLinux(WindowManager* wm) {
    PROFILE_START(timer);
    if (wm) wm->running = false;
    PROFILE_END_VAR(timer, "window_manager_x11:WindowManager_QuitLinux");
}
