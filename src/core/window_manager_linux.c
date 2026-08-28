#include "core/window_manager.h"
#include "core/common_ui.h"
#include "core/modal.h"
#include "core/vim.h"
#include "platform/platform.h"
#include "platform/x11/ui_x11.h"
#include "helpers.h"
#include "window/subclasses_x11.h"
#include "features/focus_switcher.h"
#include "features/music_player.h"
#include "features/video_viewer.h"
#include "features/process_killer.h"
#include "core/feature_data.h"
#include "features/todo.h"
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
#include "x11_display.h"

static bool g_linux_ctrl_held = false;
static bool g_linux_alt_held = false;
static bool g_linux_shift_held = false;

extern char output_buffer[256];
extern bool selection_complete;

extern bool HandleGlobalKeyEvent(int key_code, int value);

int pending_hotkey_vk = 0;
int pending_hotkey_mod = 0;
long pending_hotkey_timeMs = 0;

static int g_uinput_fd = -1;
static bool g_suppress_release[KEY_MAX + 1] = {0};
static bool g_device_grabbed[16] = {0};

#define MAX_KEYBOARDS 16

typedef struct {
    int fd;
    char path[256];
} SystemKeyboard;

static SystemKeyboard active_kbs[MAX_KEYBOARDS];
static int active_kb_count = 0;

static bool should_use_grab(void) {
    if (getenv("WAYLAND_DISPLAY")) return false;
    if (getenv("HYPRLAND_INSTANCE_SIGNATURE")) return false;
    const char* t = getenv("XDG_SESSION_TYPE");
    if (t && strcmp(t, "wayland") == 0) return false;
    return true;
}

static int init_uinput(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) fd = open("/dev/input/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "[UINPUT] open failed: %s (swallow disabled)\n", strerror(errno));
        return -1;
    }
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) perror("[UINPUT] UI_SET_EVBIT EV_KEY");
    if (ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0) perror("[UINPUT] UI_SET_EVBIT EV_SYN");
    
    ioctl(fd, UI_SET_EVBIT, EV_MSC);
    ioctl(fd, UI_SET_MSCBIT, MSC_SCAN);
    for (int i = 0; i <= KEY_MAX; i++) {
        ioctl(fd, UI_SET_KEYBIT, i);
    }
    
    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x1234;
    setup.id.product = 0x5678;
    snprintf(setup.name, sizeof(setup.name), "myprogram-virtual-keyboard");
    if (ioctl(fd, UI_DEV_SETUP, &setup) == 0) {
        if (ioctl(fd, UI_DEV_CREATE) < 0) {
            perror("[UINPUT] UI_DEV_CREATE");
            close(fd);
            return -1;
        }
    } else {
        
        struct uinput_user_dev uud;
        memset(&uud, 0, sizeof(uud));
        snprintf(uud.name, sizeof(uud.name), "myprogram-virtual-keyboard");
        uud.id.bustype = BUS_USB;
        uud.id.vendor = 0x1234;
        uud.id.product = 0x5678;
        if (write(fd, &uud, sizeof(uud)) != sizeof(uud)) {
            perror("[UINPUT] write uud");
        }
        if (ioctl(fd, UI_DEV_CREATE) < 0) {
            perror("[UINPUT] UI_DEV_CREATE legacy");
            close(fd);
            return -1;
        }
    }
    
    usleep(150 * 1000);
    fprintf(stderr, "[UINPUT] virtual keyboard created (fd=%d)\n", fd);
    return fd;
}

static void destroy_uinput(void) {
    if (g_uinput_fd >= 0) {
        ioctl(g_uinput_fd, UI_DEV_DESTROY);
        close(g_uinput_fd);
        g_uinput_fd = -1;
    }
}

static void uinput_emit(const struct input_event *ev) {
    if (g_uinput_fd < 0) return;
    if (write(g_uinput_fd, ev, sizeof(*ev)) < 0) {
        if (errno != EAGAIN) perror("[UINPUT] write");
        return;
    }
    struct input_event syn;
    memset(&syn, 0, sizeof(syn));
    gettimeofday(&syn.time, NULL);
    syn.type = EV_SYN;
    syn.code = SYN_REPORT;
    syn.value = 0;
    if (write(g_uinput_fd, &syn, sizeof(syn)) < 0 && errno != EAGAIN) {
        perror("[UINPUT] write syn");
    }
}

static void ScanAndHookKeyboards(void) {
    DIR *dir = opendir("/dev/input");
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && active_kb_count < MAX_KEYBOARDS) {
        if (strncmp(entry->d_name, "event", 5) == 0) {
            char full_path[256];
            snprintf(full_path, sizeof(full_path), "/dev/input/%s", entry->d_name);

            int fd = open(full_path, O_RDONLY | O_NONBLOCK);
            if (fd < 0) continue;

            unsigned long ev_bits[EV_MAX / 8 + 1] = {0};
            if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) >= 0) {
                if (ev_bits[0] & (1 << EV_KEY)) {
                    char dev_name[256] = {0};
                    unsigned long key_bits[(KEY_MAX/8)+1] = {0};
                    bool has_alpha = false;
                    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) >= 0) {
                        if (key_bits[30/ (8*sizeof(unsigned long))] & (1UL << (30 % (8*sizeof(unsigned long))))) has_alpha = true;
                        if (!has_alpha && (key_bits[KEY_ESC/ (8*sizeof(unsigned long))] & (1UL << (KEY_ESC % (8*sizeof(unsigned long)))))) {
                            if (key_bits[KEY_SPACE/ (8*sizeof(unsigned long))] & (1UL << (KEY_SPACE % (8*sizeof(unsigned long))))) has_alpha = true;
                        }
                    }
                    active_kbs[active_kb_count].fd = fd;
                    strncpy(active_kbs[active_kb_count].path, full_path, 255);
                    active_kb_count++;
                } else {
                    close(fd);
                }
            } else {
                close(fd);
            }
        }
    }
    closedir(dir);
}

static int XKeysymToVK(KeySym ks) {
    if (ks >= XK_a && ks <= XK_z) return 'A' + (ks - XK_a);
    if (ks >= XK_A && ks <= XK_Z) return 'A' + (ks - XK_A);
    if (ks >= XK_0 && ks <= XK_9) return '0' + (ks - XK_0);
    if (ks >= XK_F1 && ks <= XK_F12) return VK_F1 + (ks - XK_F1);
    switch (ks) {
        case XK_Escape: return VK_ESCAPE;
        case XK_Return:
        case XK_KP_Enter: return VK_RETURN;
        case XK_Tab: return VK_TAB;
        case XK_BackSpace: return VK_BACK;
        case XK_Up: return VK_UP;
        case XK_Down: return VK_DOWN;
        case XK_Left: return VK_LEFT;
        case XK_Right: return VK_RIGHT;
        case XK_Delete: return VK_DELETE;
        case XK_Home: return VK_HOME;
        case XK_End: return VK_END;
        case XK_Page_Up: return VK_PRIOR;
        case XK_Page_Down: return VK_NEXT;
        case XK_space: return VK_SPACE;
        case XK_Control_L: return VK_LCONTROL;
        case XK_Control_R: return VK_RCONTROL;
        case XK_Shift_L: return VK_LSHIFT;
        case XK_Shift_R: return VK_RSHIFT;
        case XK_Alt_L: return VK_LMENU;
        case XK_Alt_R: return VK_RMENU;
        case XK_Super_L: return VK_LWIN;
        case XK_Super_R: return VK_RWIN;
        case XK_semicolon: case XK_colon: return VK_OEM_1;
        case XK_slash: case XK_question: return VK_OEM_2;
        case XK_bracketleft: case XK_braceleft: return VK_OEM_4;
        case XK_comma: case XK_less: return VK_OEM_COMMA;
        case XK_period: case XK_greater: return VK_OEM_PERIOD;
        case XK_apostrophe: case XK_quotedbl: return VK_OEM_7;
        case XK_bracketright: case XK_braceright: return VK_OEM_6;
        case XK_grave: case XK_asciitilde: return VK_OEM_3;
        case XK_minus: case XK_underscore: return VK_OEM_MINUS;
        case XK_equal: case XK_plus: return VK_OEM_PLUS;
        default: return 0;
    }
}

static void Linux_HandleKeyPress(Display* dpy, XKeyEvent* ev, WindowManager* wm) {
    KeySym ks;
    char buf[32];
    int len = XLookupString(ev, buf, sizeof(buf), &ks, NULL);

    // update modifier state
    if (ks == XK_Control_L || ks == XK_Control_R) g_linux_ctrl_held = true;
    else if (ks == XK_Alt_L || ks == XK_Alt_R) g_linux_alt_held = true;
    else if (ks == XK_Shift_L || ks == XK_Shift_R) g_linux_shift_held = true;

    int vk_code = 0;
    // for printable single char use the actual char to preserve case
    if (len == 1 && (unsigned char)buf[0] >= 32 && (unsigned char)buf[0] <= 126) {
        vk_code = (unsigned char)buf[0];
        // map lower a-z to upper VK_A..VK_Z for hotkey matching but keep char for insertion?
        // For prompt typing we need actual char, so keep buf[0] as vk_code.
        // For navigation keys fallback to keysym mapping.
        if (vk_code >= 'a' && vk_code <= 'z') {
            // keep lower for correct insertion, but also handle as VK for vim?
            // Keep as lower - PromptPopup_OnKey handles 32..126 as printable.
        }
    } else {
        vk_code = XKeysymToVK(ks);
    }
    bool pressed = true;
    int modifiers = 0;
    if (g_linux_alt_held) modifiers |= HOTKEY_MOD_ALT;
    if (g_linux_shift_held) modifiers |= HOTKEY_MOD_SHIFT;
    if (g_linux_ctrl_held) modifiers |= HOTKEY_MOD_CTRL;

    Popup* top_before = WindowManager_GetTopPopup(wm);
    if (top_before && top_before->vtable && top_before->vtable->on_key) {
        top_before->vtable->on_key(top_before, vk_code, pressed, modifiers);
    }
    Popup* top_after = WindowManager_GetTopPopup(wm);
    if (top_after && top_after->vtable && top_after->vtable->on_render) {
        top_after->vtable->on_render(top_after);
    }
    XFlush(dpy);
}

static void Linux_HandleKeyRelease(Display* dpy, XKeyEvent* ev) {
    int keycode = ev->keycode;
    KeySym ks = XKeycodeToKeysym(dpy, keycode, 0);
    
    if (ks == XK_Control_L || ks == XK_Control_R) g_linux_ctrl_held = false;
    else if (ks == XK_Alt_L || ks == XK_Alt_R) g_linux_alt_held = false;
    else if (ks == XK_Shift_L || ks == XK_Shift_R) g_linux_shift_held = false;
}

void Linux_ProcessXEvent(Display* dpy, XEvent* ev, WindowManager* wm) {
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) return;

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
}

int WindowManager_RunLinux(WindowManager* wm) {
    if (!wm) return -1;
    wm->running = true;

    Display* dpy = wm->display;
    if (!dpy) return -1;

    bool use_grab = should_use_grab();
    if (!use_grab) {
        g_uinput_fd = -1;
    } else {
        g_uinput_fd = init_uinput();
        if (g_uinput_fd < 0) {
            fprintf(stderr, "[SYSTEM] continuing without uinput (no swallow)\n");
        }
    }

    ScanAndHookKeyboards();
    if (active_kb_count == 0) {
        fprintf(stderr, "[ERROR] No structural keyboard nodes located under /dev/input/\n");
        destroy_uinput();
        return -1;
    }

    struct pollfd *fds = calloc(active_kb_count + 1, sizeof(struct pollfd));
    fds[0].fd = ConnectionNumber(dpy);
    fds[0].events = POLLIN;
    for (int i = 0; i < active_kb_count; i++) {
        fds[i + 1].fd = active_kbs[i].fd;
        fds[i + 1].events = POLLIN;
    }

    XEvent xev;
    struct input_event ev;
    const long HOTKEY_STALE_EVENT_MS = 200;

    while (wm->running) {
        int poll_res = poll(fds, active_kb_count + 1, -1);
        if (poll_res < 0) break;

        if (fds[0].revents & POLLIN) {
            while (XPending(dpy) > 0) {
                XNextEvent(dpy, &xev);
                Linux_ProcessXEvent(dpy, &xev, wm);
            }
        }

        for (int i = 0; i < active_kb_count; i++) {
            if (fds[i + 1].revents & POLLIN) {
                bool grabbed = g_device_grabbed[i];
                while (read(fds[i + 1].fd, &ev, sizeof(struct input_event)) > 0) {
                    
                    if (ev.type != EV_KEY) {
                        if (grabbed && g_uinput_fd >= 0) uinput_emit(&ev);
                        continue;
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
                            if (grabbed && g_uinput_fd >= 0) uinput_emit(&ev);
                            continue;
                        }
                        if (ev.value == 1) {
                            if (grabbed && g_uinput_fd >= 0) uinput_emit(&ev);
                            continue;
                        }
                    }

                    if (ev.value == 0 && ev.code <= KEY_MAX && g_suppress_release[ev.code]) {
                        g_suppress_release[ev.code] = false;
                        continue;
                    }
                    if (ev.value == 2 && ev.code <= KEY_MAX && g_suppress_release[ev.code]) {
                        continue;
                    }

                    if (HandleGlobalKeyEvent(ev.code, ev.value)) {
                        if (ev.value == 1 && ev.code <= KEY_MAX) {
                            g_suppress_release[ev.code] = true;
                        } else if (ev.value == 2 && ev.code <= KEY_MAX) {
                            g_suppress_release[ev.code] = true;
                        }
                        continue;
                    }
                    
                    if (grabbed) {
                        if (g_uinput_fd >= 0) {
                            uinput_emit(&ev);
                        } else {
                            static bool warned = false;
                            if (!warned) {
                                fprintf(stderr, "[WARN] grabbed but no uinput - key %d lost. Check /dev/uinput perms (need input group or ACL)\n", ev.code);
                                warned = true;
                            }
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i < active_kb_count; i++) {
        ioctl(active_kbs[i].fd, EVIOCGRAB, 0);
        close(active_kbs[i].fd);
    }
    free(fds);
    destroy_uinput();

    return 0;
}

void WindowManager_ProcessEventsLinux(WindowManager* wm) {
    if (!wm || !wm->display) return;
    
    Display* dpy = wm->display;
    XEvent ev;
    
    while (XPending(dpy) > 0) {
        XNextEvent(dpy, &ev);
        Linux_ProcessXEvent(dpy, &ev, wm);
    }
}

void WindowManager_QuitLinux(WindowManager* wm) {
    if (wm) wm->running = false;
}