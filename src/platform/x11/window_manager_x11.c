#include "core/window_manager.h"
#include "deps/log.h"

#include "platform/ui.h"
#include "core/modal.h"
#include "core/vim.h"
#include "core/platform.h"
#include "platform/x11/ui_x11.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_file.h"
#include "helpers/helpers_ui.h"
#include "platform/x11/subclasses_x11.h"
#include "platform/input_state.h"
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
#include "platform/x11/display_x11.h"
#include "platform/x11/input_x11.h"
#include "core/timer.h"
#include "core/history.h"
#include "core/remap_manager.h"
#include "core/hotkey_manager.h"


extern bool HandleGlobalKeyEvent(int key_code, int value);

int pending_hotkey_vk = 0;
int pending_hotkey_mod = 0;
long pending_hotkey_timeMs = 0;

static bool should_use_grab(void) {
  return x11_should_use_grab(); 
}

static int init_uinput(void) {
  return x11_init_uinput(); 
}
static void destroy_uinput(void) {
  x11_destroy_uinput(); 
}
static void ScanAndHookKeyboards(void) {
  x11_scan_keyboards(); 
}

static int XKeysymToVK(KeySym ks) {
  return x11_keysym_to_vk(ks); 
}

static void* input_thread_func(void* arg) {
    WindowManager* wm = (WindowManager*)arg;
    if (g_x11_keyboard_count == 0) return NULL;
    struct pollfd *fds = calloc(g_x11_keyboard_count, sizeof(struct pollfd));
    for (int i=0;i<g_x11_keyboard_count;i++) { fds[i].fd = g_x11_keyboards[i].fd; fds[i].events = POLLIN; }
    struct input_event ev;
    while (wm->running) {
        int r = poll(fds, g_x11_keyboard_count, 100);
        if (r > 0) {
            for (int i=0;i<g_x11_keyboard_count;i++) if (fds[i].revents & POLLIN) {
                bool grabbed = g_x11_device_grabbed[i];
                while (read(fds[i].fd, &ev, sizeof(ev)) > 0) {
                    bool has_popup = WindowManager_GetTopPopup(wm) != NULL;
                    x11_handle_evdev(&ev, has_popup, grabbed);
                }
            }
        }
    }
    free(fds);
    return NULL;
}

static void Linux_HandleKeyPress(Display* dpy, XKeyEvent* ev, WindowManager* wm) {
    PROFILE_START(timer);
    int mods = remap_current_mods();
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
    bool pressed = true;
    int modifiers = mods;
    if(vk_code) Platform_InputState_Update(vk_code, true, modifiers);

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
            log_info("Linux_HandleKeyPress prompt insert check mods=%d ks=%lu len2=%d buf2='%.*s' pending=%d", mods, (unsigned long)ks2, len2, len2, buf2, pending_hotkey_vk);
            if (len2 > 0) {
                extern int pending_hotkey_vk; extern int pending_hotkey_mod; extern long pending_hotkey_timeMs;
                bool swallow=false;
                if (pending_hotkey_vk!=0 && len2==1) {
                    struct timeval tv; gettimeofday(&tv,NULL); long now=tv.tv_sec*1000+tv.tv_usec/1000;
                    if (now - pending_hotkey_timeMs < 800) {
                        unsigned char c=(unsigned char)buf2[0];
                        int vkc=0;
                        if (c>='a'&&c<='z') vkc=c-'a'+'A';
                        else if (c>='A'&&c<='Z') vkc=c;
                        else if (c==' ') vkc=VK_SPACE;
                        else if (c=='['||c=='{') vkc=VK_OEM_4;
                        else if (c==']'||c=='}') vkc=VK_OEM_6;
                        else if (c==';'||c==':') vkc=VK_OEM_1;
                        else if (c=='/'||c=='?') vkc=VK_OEM_2;
                        if (vkc!=0 && vkc==pending_hotkey_vk) swallow=true;
                    }
                    pending_hotkey_vk=0;
                    pending_hotkey_mod=0;
                }
                if (!swallow && (mods & (HOTKEY_MOD_ALT|HOTKEY_MOD_CTRL))) swallow=true;
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

char *clipboard_text = NULL;
Atom clipboard_atom;
Atom targets_atom;
Atom utf8_string_atom;

static void Linux_ClipboardUpdate(XEvent *event) {

    Display *dpy = get_display();

    if (!clipboard_text)
        return;

    if (event->type != SelectionRequest)
        return;

    XSelectionRequestEvent *request = &event->xselectionrequest;

    XEvent response = {0};

    response.xselection.type = SelectionNotify;
    response.xselection.display = request->display;
    response.xselection.requestor = request->requestor;
    response.xselection.selection = request->selection;
    response.xselection.target = request->target;
    response.xselection.property = None;
    response.xselection.time = request->time;

    if (request->target == targets_atom) {
        Atom targets[] = {
            targets_atom,
            utf8_string_atom,
            XA_STRING
        };

        XChangeProperty(
            dpy,
            request->requestor,
            request->property,
            XA_ATOM,
            32,
            PropModeReplace,
            (unsigned char *)targets,
            sizeof(targets) / sizeof(targets[0])
        );

        response.xselection.property = request->property;
    }

    else if (request->target == utf8_string_atom ||
             request->target == XA_STRING) {

        Atom type = request->target == utf8_string_atom
                  ? utf8_string_atom
                  : XA_STRING;

        XChangeProperty(
            dpy,
            request->requestor,
            request->property,
            type,
            8,
            PropModeReplace,
            (unsigned char *)clipboard_text,
            strlen(clipboard_text)
        );

        response.xselection.property = request->property;
    }

    XSendEvent(
        dpy,
        request->requestor,
        False,
        0,
        &response
    );

    XFlush(dpy);
}

static void Linux_HandleKeyRelease(Display* dpy, XKeyEvent* ev) {
    PROFILE_START(timer);
    KeySym ks; char buf[32]; int len=XLookupString(ev, buf, sizeof(buf), &ks, NULL);
    int vk=0; if(len==1 && (unsigned char)buf[0]>=32 && (unsigned char)buf[0]<=126) vk=(unsigned char)buf[0]; else vk=XKeysymToVK(ks);
    int mods=remap_current_mods();
    if(vk) Platform_InputState_Update(vk, false, mods);
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
        case SelectionRequest:
           Linux_ClipboardUpdate(ev);
           break;
    }
    PROFILE_END_VAR(timer, "window_manager_x11:Linux_ProcessXEvent");
}

int WindowManager_RunLinux(WindowManager* wm) {
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

    if (remap_has_bindings()) {
        if (g_x11_uinput_fd < 0) {
            log_warn("remap forcing uinput init despite use_grab=%d", use_grab);
            g_x11_uinput_fd = init_uinput();
            if (g_x11_uinput_fd >= 0) log_warn("remap uinput forced fd=%d", g_x11_uinput_fd);
            else log_warn("remap uinput force failed, fallback XTest will be used");
        }
        if (g_x11_uinput_fd >= 0) {
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
        } else {
            log_warn("remap grab skipped: no uinput, not grabbing to avoid bricking");
        }
    }
    if (g_x11_keyboard_count == 0) {
        log_error("No structural keyboard nodes located under /dev/input/");
        destroy_uinput();
        PROFILE_END_VAR(timer, "window_manager_x11:WindowManager_RunLinux");
        return -1;
    }

    pthread_t input_thr = 0;
    bool input_started = false;
    if (g_x11_keyboard_count > 0) {
        if (pthread_create(&input_thr, NULL, input_thread_func, wm) == 0) input_started = true;
        else log_error("failed to start input thread");
    }

    XEvent xev;
    while (wm->running) {
        struct pollfd xfd = { .fd = ConnectionNumber(dpy), .events = POLLIN };
        int poll_res = poll(&xfd, 1, 50);
        if (poll_res < 0) break;
        if (xfd.revents & POLLIN) {
            while (XPending(dpy) > 0) {
                XNextEvent(dpy, &xev);
                Linux_ProcessXEvent(dpy, &xev, wm);
            }
        }
        hotkey_drain_pending();
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
    x11_set_window_manager(NULL);

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
    hotkey_drain_pending();
    PROFILE_END_VAR(timer, "platform:Platform_ProcessNestedEvents");
}

void WindowManager_QuitLinux(WindowManager* wm) {
    PROFILE_START(timer);
    if (wm) wm->running = false;
    PROFILE_END_VAR(timer, "window_manager_x11:WindowManager_QuitLinux");
}
