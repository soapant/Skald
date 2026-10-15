#define _GNU_SOURCE
#include "platform/x11/input_x11.h"
#include "deps/log.h"
#include "core/timer.h"
#include "core/hotkey_manager.h"
#include "core/remap_manager.h"
#include "core/snippet_manager.h"
#include "config.h"
#include "platform/platform.h"
#include "platform/input_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/uinput.h>
#include <errno.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "platform/x11/display_x11.h"
#include <signal.h>
#include <pthread.h>

X11Keyboard g_x11_keyboards[X11_MAX_KEYBOARDS];
int g_x11_keyboard_count = 0;
X11Keyboard g_x11_mice[X11_MAX_MICE];
int g_x11_mouse_count = 0;
bool g_x11_mouse_grabbed[X11_MAX_MICE] = {false};
int g_x11_uinput_mouse_fd = -1;
int g_x11_uinput_fd = -1;
bool g_x11_suppress_release[KEY_MAX + 1] = {0};
static long g_x11_suppress_ms[KEY_MAX + 1] = {0};
bool g_x11_device_grabbed[X11_MAX_KEYBOARDS] = {0};

#define X11_REJECTED_MAX 128
static int g_x11_rejected_fds[X11_REJECTED_MAX];
static int g_x11_rejected_count = 0;

static int x11_linux_button_to_mouse_impl(int code);

static int x11_linux_button_to_mouse(int code) {
        PROFILE_START(_pt);
int btn = x11_linux_button_to_mouse_impl(code);
{ PROFILE_END_VAR(_pt, "x11_linux_button_to_mouse"); return btn; }
}

static int x11_linux_button_to_mouse_impl(int code) {
    switch (code) {
        case BTN_LEFT:    return MOUSE_BTN_LEFT;
        case BTN_RIGHT:   return MOUSE_BTN_RIGHT;
        case BTN_MIDDLE:  return MOUSE_BTN_MIDDLE;
        case BTN_SIDE:    return MOUSE_BTN_SIDE1;
        case BTN_EXTRA:   return MOUSE_BTN_SIDE2;
        case BTN_BACK:    return MOUSE_BTN_BACK;
        case BTN_FORWARD: return MOUSE_BTN_FORWARD;
        case BTN_TASK:    return MOUSE_BTN_TASK;
        default: break;
    }
    if (code >= BTN_0 && code <= BTN_9) {
        int n = MOUSE_BTN_EXTRA1 + (code - BTN_0);
        return n <= FL_MOUSE_BTN_MAX ? n : 0;
    }
    if (code >= 0x14e && code <= 0x159) {
        int n = 0x13 + (code - 0x14e);
        return (n > 0 && n <= FL_MOUSE_BTN_MAX) ? n : 0;
    }
    return 0;
}

static void x11_queue_mouse_event(struct input_event* ev) {
        PROFILE_START(_pt);
if (!ev || ev->type != EV_KEY) { PROFILE_END_VAR(_pt, "x11_queue_mouse_event"); return ; }
    int btn = x11_linux_button_to_mouse(ev->code);
    if (btn == 0) { PROFILE_END_VAR(_pt, "x11_queue_mouse_event"); return ; }
    bool down = (ev->value != 0);
    bool repeat = (ev->value == 2);
    Platform_UpdateMouseButton(btn, down);
    int mx = 0, my = 0;
    Platform_GetMousePos(&mx, &my);
    PlatformMouseEvent mev;
    mev.button = btn;
    mev.x = mx;
    mev.y = my;
    mev.mods = remap_current_mods();
    mev.down = down;
    mev.repeat = repeat;
    mev.t_us = 0;
    Platform_GlobalMouse_Push(&mev);
    if (down && !repeat && btn > 0 && btn <= FL_MOUSE_BTN_MAX) {
        hotkey_process(FL_MOUSE_VK(btn), mev.mods);
    }
}

bool x11_handle_evdev_mouse(struct input_event *ev) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (!ev) { PROFILE_END_VAR(timer, "input:x11_handle_evdev_mouse"); return false; }
    x11_queue_mouse_event(ev);
    PROFILE_END_VAR(timer, "input:x11_handle_evdev_mouse");
    { PROFILE_END_VAR(_pt, "x11_handle_evdev_mouse"); return true; }
}

bool Platform_GetMousePos(int* out_x, int* out_y) {
        PROFILE_START(_pt);
Display* dpy = get_display();
    if (!dpy) { PROFILE_END_VAR(_pt, "Platform_GetMousePos"); return false; }
    Window root = DefaultRootWindow(dpy);
    Window rw = 0, cw = 0;
    int rx = 0, ry = 0, wx = 0, wy = 0;
    int mask = 0;
    if (!XQueryPointer(dpy, root, &rw, &cw, &rx, &ry, &wx, &wy, &mask)) { PROFILE_END_VAR(_pt, "Platform_GetMousePos"); return false; }
    if (out_x) *out_x = rx;
    if (out_y) *out_y = ry;
    { PROFILE_END_VAR(_pt, "Platform_GetMousePos"); return true; }
}

static void x11_release_rejected_devices(void) {
        PROFILE_START(_pt);
for (int i = 0; i < g_x11_rejected_count; i++) {
        if (g_x11_rejected_fds[i] >= 0) {
            close(g_x11_rejected_fds[i]);
            g_x11_rejected_fds[i] = -1;
        }
    }
    g_x11_rejected_count = 0;
}

bool g_x11_ctrl_held = false;
bool g_x11_alt_held = false;
bool g_x11_shift_held = false;
pthread_mutex_t g_input_mtx = PTHREAD_MUTEX_INITIALIZER;

extern int pending_hotkey_vk;
extern int pending_hotkey_mod;
extern long pending_hotkey_timeMs;

bool x11_should_use_grab(void) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (getenv("WAYLAND_DISPLAY")) { PROFILE_END_VAR(timer, "input:x11_should_use_grab"); return false; }
    if (getenv("HYPRLAND_INSTANCE_SIGNATURE")) { PROFILE_END_VAR(timer, "input:x11_should_use_grab"); return false; }
    const char* t = getenv("XDG_SESSION_TYPE");
    if (t && strcmp(t, "wayland") == 0) { PROFILE_END_VAR(timer, "input:x11_should_use_grab"); return false; }
    PROFILE_END_VAR(timer, "input:x11_should_use_grab");
    { PROFILE_END_VAR(_pt, "x11_should_use_grab"); return true; }
}

static struct WindowManager *g_x11_wm = NULL;
void x11_set_window_manager(struct WindowManager *wm) {
    PROFILE_START(_pt);
g_x11_wm = wm; 
}

static void x11_force_cleanup(void);
static void x11_install_cleanup_handlers(void);
void x11_mouse_grab_end(void);
static volatile sig_atomic_t g_x11_cleanup_done = 0;

static void x11_sig_handler(int sig) {
        PROFILE_START(_pt);
(void)sig;
    x11_force_cleanup();
    signal(sig, SIG_DFL);
    raise(sig);
}

static void x11_term_handler(int sig) {
        PROFILE_START(_pt);
(void)sig;
    x11_force_cleanup();
    if (g_x11_wm) {
        extern void WindowManager_QuitLinux(struct WindowManager *wm);
        WindowManager_QuitLinux(g_x11_wm);
    }
    _exit(0);
}

static void x11_force_cleanup(void) {
        PROFILE_START(_pt);
if (g_x11_cleanup_done) { PROFILE_END_VAR(_pt, "x11_force_cleanup"); return ; }
    g_x11_cleanup_done = 1;
    for (int i=0;i<g_x11_keyboard_count;i++) {
        if (g_x11_keyboards[i].fd >= 0) {
            ioctl(g_x11_keyboards[i].fd, EVIOCGRAB, 0);
        }
    }
    x11_mouse_grab_end();
    for (int i=0;i<256;i++) {
        extern bool remap_is_virt_held(int vk);
        if (remap_is_virt_held(i)) Platform_EmitKey(i, 0);
    }
    remap_reset();
    x11_clear_suppress();
    if (g_x11_uinput_fd >= 0) {
        ioctl(g_x11_uinput_fd, UI_DEV_DESTROY);
        close(g_x11_uinput_fd);
        g_x11_uinput_fd = -1;
    }
    for (int i=0;i<g_x11_keyboard_count;i++) {
        if (g_x11_keyboards[i].fd >= 0) {
            close(g_x11_keyboards[i].fd);
            g_x11_keyboards[i].fd = -1;
        }
    }
    for (int i=0;i<g_x11_mouse_count;i++) {
        if (g_x11_mice[i].fd >= 0) {
            close(g_x11_mice[i].fd);
            g_x11_mice[i].fd = -1;
        }
    }
    g_x11_mouse_count = 0;
    x11_release_rejected_devices();
}

static void x11_atexit_handler(void) {
    PROFILE_START(_pt);
x11_force_cleanup(); 
}

static void x11_install_cleanup_handlers(void) {
        PROFILE_START(_pt);
static bool installed = false;
    if (installed) { PROFILE_END_VAR(_pt, "x11_install_cleanup_handlers"); return ; }
    installed = true;
    atexit(x11_atexit_handler);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = x11_term_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sa.sa_handler = x11_sig_handler;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
}

int x11_init_uinput(void) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    x11_install_cleanup_handlers();
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) fd = open("/dev/input/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        log_error("open failed: %s (swallow disabled)", strerror(errno));
        PROFILE_END_VAR(timer, "input:x11_init_uinput");
        return -1;
    }
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) perror("[UINPUT] UI_SET_EVBIT EV_KEY");
    if (ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0) perror("[UINPUT] UI_SET_EVBIT EV_SYN");
    ioctl(fd, UI_SET_EVBIT, EV_MSC);
    ioctl(fd, UI_SET_MSCBIT, MSC_SCAN);
    for (int i = 0; i <= KEY_MAX; i++) ioctl(fd, UI_SET_KEYBIT, i);
    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x1234;
    setup.id.product = 0x5678;
    snprintf(setup.name, sizeof(setup.name), "flint-virtual-keyboard");
    if (ioctl(fd, UI_DEV_SETUP, &setup) == 0) {
        if (ioctl(fd, UI_DEV_CREATE) < 0) { perror("[UINPUT] UI_DEV_CREATE"); close(fd); PROFILE_END_VAR(timer, "input:x11_init_uinput"); return -1; }
    } else {
        struct uinput_user_dev uud;
        memset(&uud, 0, sizeof(uud));
        snprintf(uud.name, sizeof(uud.name), "flint-virtual-keyboard");
        uud.id.bustype = BUS_USB; uud.id.vendor = 0x1234; uud.id.product = 0x5678;
        if (write(fd, &uud, sizeof(uud)) != sizeof(uud)) perror("[UINPUT] write uud");
        if (ioctl(fd, UI_DEV_CREATE) < 0) { perror("[UINPUT] UI_DEV_CREATE legacy"); close(fd); PROFILE_END_VAR(timer, "input:x11_init_uinput"); return -1; }
    }
    log_info("virtual keyboard created (fd=%d)", fd);
    PROFILE_END_VAR(timer, "input:x11_init_uinput");
    { PROFILE_END_VAR(_pt, "x11_init_uinput"); return fd; }
}

void x11_mouse_grab_end(void) {
    for (int i = 0; i < g_x11_mouse_count; i++) {
        if (g_x11_mice[i].fd >= 0 && g_x11_mouse_grabbed[i]) {
            ioctl(g_x11_mice[i].fd, EVIOCGRAB, 0);
            log_info("ungrabbed mouse fd=%d", (int)g_x11_mice[i].fd);
        }
        g_x11_mouse_grabbed[i] = false;
    }
    if (g_x11_uinput_mouse_fd >= 0) {
        ioctl(g_x11_uinput_mouse_fd, UI_DEV_DESTROY);
        close(g_x11_uinput_mouse_fd);
        g_x11_uinput_mouse_fd = -1;
    }
}

void x11_destroy_uinput(void) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (g_x11_uinput_fd >= 0) { ioctl(g_x11_uinput_fd, UI_DEV_DESTROY); close(g_x11_uinput_fd); g_x11_uinput_fd = -1; }
    PROFILE_END_VAR(timer, "input:x11_destroy_uinput");
}

static int x11_init_uinput_mouse(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) fd = open("/dev/input/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) { log_error("uinput mouse open failed: %s", strerror(errno)); return -1; }

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_REL);
    ioctl(fd, UI_SET_EVBIT, EV_MSC);
    ioctl(fd, UI_SET_EVBIT, EV_SYN);

    for (int c = BTN_MISC; c <= BTN_TASK; c++) ioctl(fd, UI_SET_KEYBIT, c);
    for (int c = BTN_0; c <= BTN_9; c++) ioctl(fd, UI_SET_KEYBIT, c);
    for (int c = 0x14e; c <= 0x159; c++) ioctl(fd, UI_SET_KEYBIT, c);

    for (int r = REL_X; r <= REL_MAX; r++) ioctl(fd, UI_SET_RELBIT, r);
    ioctl(fd, UI_SET_MSCBIT, MSC_SCAN);

    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x1234;
    setup.id.product = 0x5679;
    snprintf(setup.name, sizeof(setup.name), "flint-virtual-mouse");
    if (ioctl(fd, UI_DEV_SETUP, &setup) == 0) {
        if (ioctl(fd, UI_DEV_CREATE) < 0) { perror("[UINPUT-M] UI_DEV_CREATE"); close(fd); return -1; }
    } else {
        struct uinput_user_dev uud;
        memset(&uud, 0, sizeof(uud));
        snprintf(uud.name, sizeof(uud.name), "flint-virtual-mouse");
        uud.id.bustype = BUS_USB; uud.id.vendor = 0x1234; uud.id.product = 0x5679;
        if (write(fd, &uud, sizeof(uud)) != sizeof(uud)) perror("[UINPUT-M] write uud");
        if (ioctl(fd, UI_DEV_CREATE) < 0) { perror("[UINPUT-M] UI_DEV_CREATE legacy"); close(fd); return -1; }
    }
    log_info("virtual mouse created (fd=%d)", fd);
    return fd;
}

void x11_mouse_grab_begin(void) {
    if (g_x11_uinput_mouse_fd < 0) {
        g_x11_uinput_mouse_fd = x11_init_uinput_mouse();
        if (g_x11_uinput_mouse_fd < 0) { log_warn("mouse grab disabled, uinput mouse failed"); return; }
    }
    for (int i = 0; i < g_x11_mouse_count; i++) {
        if (g_x11_mice[i].fd < 0 || g_x11_mouse_grabbed[i]) continue;
        struct input_event drain;
        while (read(g_x11_mice[i].fd, &drain, sizeof(drain)) > 0) {}
        if (ioctl(g_x11_mice[i].fd, EVIOCGRAB, 1) == 0) {
            g_x11_mouse_grabbed[i] = true;
            log_info("grabbed mouse fd=%d", (int)g_x11_mice[i].fd);
        }
    }
}

void x11_mouse_forward(const struct input_event *ev, int grabbed) {
    if (!grabbed || g_x11_uinput_mouse_fd < 0) return;
    if (ev->type == EV_KEY) {
        int btn = x11_linux_button_to_mouse_impl(ev->code);
        if (btn > 0 && hotkey_mouse_button_bound(btn)) return;
    }
    if (write(g_x11_uinput_mouse_fd, ev, sizeof(*ev)) < 0) {
        if (errno != EAGAIN) perror("[UINPUT-M] write");
        return;
    }
    if (ev->type != EV_SYN) {
        struct input_event syn;
        memset(&syn, 0, sizeof(syn));
        gettimeofday(&syn.time, NULL);
        syn.type = EV_SYN; syn.code = SYN_REPORT; syn.value = 0;
        if (write(g_x11_uinput_mouse_fd, &syn, sizeof(syn)) < 0 && errno != EAGAIN) perror("[UINPUT-M] write syn");
    }
}

void x11_uinput_emit(const struct input_event *ev) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (g_x11_uinput_fd < 0) { PROFILE_END_VAR(timer, "input:x11_uinput_emit"); return; }
    if (write(g_x11_uinput_fd, ev, sizeof(*ev)) < 0) { if (errno != EAGAIN) perror("[UINPUT] write"); PROFILE_END_VAR(timer, "input:x11_uinput_emit"); return; }
    struct input_event syn; memset(&syn, 0, sizeof(syn)); gettimeofday(&syn.time, NULL); syn.type = EV_SYN; syn.code = SYN_REPORT; syn.value = 0;
    if (write(g_x11_uinput_fd, &syn, sizeof(syn)) < 0 && errno != EAGAIN) perror("[UINPUT] write syn");
    PROFILE_END_VAR(timer, "input:x11_uinput_emit");
}

void x11_scan_keyboards(void) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    x11_install_cleanup_handlers();
    x11_release_rejected_devices();
    DIR *dir = opendir("/dev/input");
    if (!dir) { PROFILE_END_VAR(timer, "input:x11_scan_keyboards"); return; }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) == 0) {
            char full_path[256]; snprintf(full_path, sizeof(full_path), "/dev/input/%s", entry->d_name);
            int fd = open(full_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0) continue;
            unsigned long ev_bits[EV_MAX / 8 + 1] = {0};
            unsigned long keybits[(KEY_MAX/8+sizeof(unsigned long)-1)/sizeof(unsigned long)+1] = {0};
            struct input_id id;
            bool is_kb = false;
            bool is_mouse = false;
            if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) >= 0 &&
                (ev_bits[0] & (1UL << EV_KEY)) &&
                ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) >= 0 &&
                ioctl(fd, EVIOCGID, &id) >= 0) {
                bool self = (id.vendor == 0x1234 && id.product == 0x5678);
                bool has_rel = (ev_bits[0] & (1UL << EV_REL)) != 0;
                bool has_abs = (ev_bits[0] & (1UL << EV_ABS)) != 0;
                bool has_btn = false;
                for (int b = BTN_MISC; b <= BTN_TASK; b++) {
                    int w = b / (8 * (int)sizeof(unsigned long));
                    int k = b % (8 * (int)sizeof(unsigned long));
                    if (w < (int)(sizeof(keybits)/sizeof(keybits[0])) && (keybits[w] & (1UL << k))) { has_btn = true; break; }
                }
                if (!self && !has_rel && !has_abs) {
                    keybits[0] &= ~1UL;
                    int nwords = (0xFF + 1 + (int)(sizeof(unsigned long)*8)-1)/(int)(sizeof(unsigned long)*8);
                    for (int i=0;i<nwords;i++) if (keybits[i]) { is_kb = true; break; }
                } else if (!self && has_btn && (has_rel || has_abs)) {
                    is_mouse = true;
                }
            }
            if (is_kb && g_x11_keyboard_count < X11_MAX_KEYBOARDS) {
                g_x11_keyboards[g_x11_keyboard_count].fd = fd;
                strncpy(g_x11_keyboards[g_x11_keyboard_count].path, full_path, 255);
                g_x11_keyboard_count++;
                log_warn("x11_scan_keyboards grabbed keyboard %s fd=%d", full_path, fd);
            } else if (is_mouse && g_x11_mouse_count < X11_MAX_MICE) {
                g_x11_mice[g_x11_mouse_count].fd = fd;
                strncpy(g_x11_mice[g_x11_mouse_count].path, full_path, 255);
                g_x11_mouse_count++;
                log_warn("x11_scan_keyboards opened mouse %s fd=%d (not grabbed)", full_path, fd);
            } else if (g_x11_rejected_count < X11_REJECTED_MAX) {
                g_x11_rejected_fds[g_x11_rejected_count++] = fd;
            } else {
                close(fd);
            }
        }
    }
    closedir(dir);
    log_warn("x11_scan_keyboards found %d keyboards %d mice held=%d", g_x11_keyboard_count, g_x11_mouse_count, g_x11_rejected_count);
    PROFILE_END_VAR(timer, "input:x11_scan_keyboards");
}

int x11_keysym_to_vk(KeySym ks) {
        PROFILE_START(_pt);
if (ks >= XK_a && ks <= XK_z) { PROFILE_END_VAR(_pt, "x11_keysym_to_vk"); return 'A' + (ks - XK_a); }
    if (ks >= XK_A && ks <= XK_Z) { PROFILE_END_VAR(_pt, "x11_keysym_to_vk"); return 'A' + (ks - XK_A); }
    if (ks >= XK_0 && ks <= XK_9) { PROFILE_END_VAR(_pt, "x11_keysym_to_vk"); return '0' + (ks - XK_0); }
    if (ks >= XK_F1 && ks <= XK_F12) { PROFILE_END_VAR(_pt, "x11_keysym_to_vk"); return VK_F1 + (ks - XK_F1); }
    switch (ks) {
        case XK_Escape: return VK_ESCAPE;
        case XK_Return: case XK_KP_Enter: return VK_RETURN;
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
        case XK_comma: case XK_less: return VK_OEM_COMMA;
        case XK_period: case XK_greater: return VK_OEM_PERIOD;
        case XK_apostrophe: case XK_quotedbl: return VK_OEM_7;
        case XK_bracketleft: case XK_braceleft: return VK_OEM_4;
        case XK_bracketright: case XK_braceright: return VK_OEM_6;
        case XK_grave: case XK_asciitilde: return VK_OEM_3;
        case XK_minus: case XK_underscore: return VK_OEM_MINUS;
        case XK_equal: case XK_plus: return VK_OEM_PLUS;
        default: return 0;
    }
}

int x11_vk_to_linux_key(int vk) {
        PROFILE_START(_pt);
switch(vk){
        case 'A': return KEY_A; case 'B': return KEY_B; case 'C': return KEY_C; case 'D': return KEY_D;
        case 'E': return KEY_E; case 'F': return KEY_F; case 'G': return KEY_G; case 'H': return KEY_H;
        case 'I': return KEY_I; case 'J': return KEY_J; case 'K': return KEY_K; case 'L': return KEY_L;
        case 'M': return KEY_M; case 'N': return KEY_N; case 'O': return KEY_O; case 'P': return KEY_P;
        case 'Q': return KEY_Q; case 'R': return KEY_R; case 'S': return KEY_S; case 'T': return KEY_T;
        case 'U': return KEY_U; case 'V': return KEY_V; case 'W': return KEY_W; case 'X': return KEY_X;
        case 'Y': return KEY_Y; case 'Z': return KEY_Z;
    }
    if (vk >= '0' && vk <= '9') { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_1 + (vk - '1'); }
    if (vk == '0') { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_0; }
    if (vk == VK_SPACE) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_SPACE; }
    if (vk == VK_RETURN) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_ENTER; }
    if (vk == VK_BACK) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_BACKSPACE; }
    if (vk == VK_TAB) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_TAB; }
    if (vk == VK_ESCAPE) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_ESC; }
    if (vk == VK_OEM_1) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_SEMICOLON; }
    if (vk == VK_OEM_2) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_SLASH; }
    if (vk == VK_OEM_4) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_LEFTBRACE; }
    if (vk == VK_OEM_6) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_RIGHTBRACE; }
    if (vk == VK_OEM_COMMA) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_COMMA; }
    if (vk == VK_OEM_PERIOD) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_DOT; }
    if (vk == VK_OEM_7) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_APOSTROPHE; }
    if (vk == VK_OEM_3) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_GRAVE; }
    if (vk == VK_OEM_PLUS) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_EQUAL; }
    if (vk == VK_OEM_MINUS) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_MINUS; }
    if (vk == VK_UP) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_UP; }
    if (vk == VK_DOWN) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_DOWN; }
    if (vk == VK_LEFT) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_LEFT; }
    if (vk == VK_RIGHT) { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return KEY_RIGHT; }
    { PROFILE_END_VAR(_pt, "x11_vk_to_linux_key"); return 0; }
}

void x11_sync_held_from_phys(void) {
        PROFILE_START(_pt);
int m = remap_current_mods();
    g_x11_ctrl_held = (m & HOTKEY_MOD_CTRL) != 0;
    g_x11_alt_held = (m & HOTKEY_MOD_ALT) != 0;
    g_x11_shift_held = (m & HOTKEY_MOD_SHIFT) != 0;
}

void x11_clear_suppress(void) {
        PROFILE_START(_pt);
for (int i=0;i<=KEY_MAX;i++) { g_x11_suppress_release[i]=false; g_x11_suppress_ms[i]=0; }
}

static long x11_now_ms(void) {
        PROFILE_START(_pt);
struct timeval tv;
    gettimeofday(&tv, NULL);
    { PROFILE_END_VAR(_pt, "x11_now_ms"); return tv.tv_sec*1000L + tv.tv_usec/1000L; }
}

bool x11_is_suppressed(int linux_code) {
        PROFILE_START(_pt);
if (linux_code<0 || linux_code>KEY_MAX) { PROFILE_END_VAR(_pt, "x11_is_suppressed"); return false; }
    if (!g_x11_suppress_release[linux_code]) { PROFILE_END_VAR(_pt, "x11_is_suppressed"); return false; }
    long now = x11_now_ms();
    if (now - g_x11_suppress_ms[linux_code] > 5000) {
        g_x11_suppress_release[linux_code]=false;
        g_x11_suppress_ms[linux_code]=0;
        return false;
    }
    { PROFILE_END_VAR(_pt, "x11_is_suppressed"); return true; }
}

void x11_set_suppress(int linux_code, bool v) {
        PROFILE_START(_pt);
if (linux_code<0 || linux_code>KEY_MAX) { PROFILE_END_VAR(_pt, "x11_set_suppress"); return ; }
    g_x11_suppress_release[linux_code]=v;
    g_x11_suppress_ms[linux_code]= v ? x11_now_ms() : 0;
}

bool x11_handle_global_key(int key_code, int value) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    remap_update_phys(ConvertLinuxKeyToWindowsVK(key_code), value);
    x11_sync_held_from_phys();
    if (value != 1) { hotkey_key_up(ConvertLinuxKeyToWindowsVK(key_code)); PROFILE_END_VAR(timer, "input:x11_handle_global_key"); return false; }
    int pressed_vk = ConvertLinuxKeyToWindowsVK(key_code);
    int modifiers = remap_current_mods();
    if (pressed_vk && hotkey_process(pressed_vk, modifiers)) {
        pending_hotkey_vk = pressed_vk; pending_hotkey_mod = modifiers; struct timeval tv; gettimeofday(&tv,NULL); pending_hotkey_timeMs = tv.tv_sec*1000 + tv.tv_usec/1000;
        PROFILE_END_VAR(timer, "input:x11_handle_global_key");
        return true;
    }

    PROFILE_END_VAR(timer, "input:x11_handle_global_key");
    { PROFILE_END_VAR(_pt, "x11_handle_global_key"); return false; }
}

bool x11_handle_evdev(struct input_event *ev, bool has_popup, bool grabbed) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (!ev) { PROFILE_END_VAR(timer, "input:x11_handle_evdev"); return false; }
    if (ev->type != EV_KEY) {
        if (grabbed && g_x11_uinput_fd >= 0) x11_uinput_emit(ev);
        PROFILE_END_VAR(timer, "input:x11_handle_evdev");
        return false;
    }
    int code = ev->code;
    int val = ev->value;
    if (ev->type == EV_KEY && val != 0) Platform_DismissIndefiniteMessage();
    if (code < 0 || code > KEY_MAX) {
        if (grabbed && g_x11_uinput_fd >= 0) x11_uinput_emit(ev);
        PROFILE_END_VAR(timer, "input:x11_handle_evdev");
        return false;
    }
    if (remap_should_swallow_linux_key(code, val)) {
        x11_sync_held_from_phys();
        if (val == 1) x11_set_suppress(code, true);
        else if (val == 0) x11_set_suppress(code, false);
        log_info("x11_handle_evdev remap swallowed code=%d val=%d popup=%d", code, val, has_popup);
        log_warn("x11_handle_evdev remap swallowed code=%d val=%d popup=%d", code, val, has_popup);
        PROFILE_END_VAR(timer, "input:x11_handle_evdev");
        return true;
    }
    if (val != 2) {
        remap_update_phys(ConvertLinuxKeyToWindowsVK(code), val);
        x11_sync_held_from_phys();
    }
    {
        PlatformKeyEvent gev;
        gev.vk = ConvertLinuxKeyToWindowsVK(code);
        gev.scancode = code;
        gev.mods = remap_current_mods();
        gev.down = val != 0;
        gev.repeat = (val == 2);
        Platform_GlobalKey_Push(&gev);
    }
    if (val == 0 && x11_is_suppressed(code)) { x11_set_suppress(code, false); PROFILE_END_VAR(timer, "input:x11_handle_evdev"); return true; }
    if (val == 2 && x11_is_suppressed(code)) { PROFILE_END_VAR(timer, "input:x11_handle_evdev"); return true; }
    if (has_popup) {
        if (val==1 && x11_handle_global_key(code, val)) { x11_set_suppress(code, true); PROFILE_END_VAR(timer, "input:x11_handle_evdev"); return true; }
        if (grabbed && g_x11_uinput_fd >= 0) x11_uinput_emit(ev);
        PROFILE_END_VAR(timer, "input:x11_handle_evdev");
        return false;
    }
    struct timeval now; gettimeofday(&now, NULL);
    long age_ms = (now.tv_sec - ev->time.tv_sec)*1000L + (now.tv_usec - ev->time.tv_usec)/1000L;
    const long HOTKEY_STALE_EVENT_MS = 200;
    if (age_ms > HOTKEY_STALE_EVENT_MS) {
        bool is_mod = (code==KEY_LEFTALT||code==KEY_RIGHTALT||code==KEY_LEFTCTRL||code==KEY_RIGHTCTRL||code==KEY_LEFTSHIFT||code==KEY_RIGHTSHIFT||code==KEY_LEFTMETA||code==KEY_RIGHTMETA);
        if (!is_mod) {
            if (val==1 && !grabbed) {
                if (x11_handle_global_key(code, val)) { x11_set_suppress(code, true); PROFILE_END_VAR(timer, "input:x11_handle_evdev"); return true; }
            }
            if (grabbed && g_x11_uinput_fd>=0) {
                if (snippet_feed_vk(ConvertLinuxKeyToWindowsVK(code), val, true)) { PROFILE_END_VAR(timer, "input:x11_handle_evdev"); return false; }
                x11_uinput_emit(ev);
            }
            PROFILE_END_VAR(timer, "input:x11_handle_evdev");
            return false;
        }
        if (val==1) { if (grabbed && g_x11_uinput_fd>=0) x11_uinput_emit(ev); PROFILE_END_VAR(timer, "input:x11_handle_evdev"); return false; }
    }
    if (x11_handle_global_key(code, val)) {
        if (val==1) x11_set_suppress(code, true);
        PROFILE_END_VAR(timer, "input:x11_handle_evdev");
        return true;
    }
    if (grabbed) {
        if (g_x11_uinput_fd >= 0) {
            if (snippet_feed_vk(ConvertLinuxKeyToWindowsVK(code), val, true)) { PROFILE_END_VAR(timer, "input:x11_handle_evdev"); return false; }
            x11_uinput_emit(ev);
        }
        else { static bool warned=false; if(!warned){ log_warn("grabbed but no uinput - key %d lost", code); warned=true; } }
    }
    else {
        snippet_feed_vk(ConvertLinuxKeyToWindowsVK(code), val, false);
    }

    PROFILE_END_VAR(timer, "input:x11_handle_evdev");
    { PROFILE_END_VAR(_pt, "x11_handle_evdev"); return false; }
}



int Platform_CharForKey(int vk, int scancode, int mods) {
    PROFILE_START(_pt);
(void)vk;
  Display *dpy = get_display();
  if (!dpy) { PROFILE_END_VAR(_pt, "Platform_CharForKey"); return 0; }
  KeySym ks = 0;
  char buf[32];
  XKeyEvent xe;
  memset(&xe, 0, sizeof(xe));
  xe.type = KeyPress;
  xe.display = dpy;
  xe.keycode = (KeyCode)(scancode + 8);
  if (mods & HOTKEY_MOD_SHIFT) xe.state |= ShiftMask;
  if (mods & HOTKEY_MOD_CTRL) xe.state |= ControlMask;
  if (mods & HOTKEY_MOD_ALT) xe.state |= Mod1Mask;
  int len = XLookupString(&xe, buf, sizeof(buf), &ks, NULL);
  if (len <= 0) { PROFILE_END_VAR(_pt, "Platform_CharForKey"); return 0; }
  { PROFILE_END_VAR(_pt, "Platform_CharForKey"); return (int)(unsigned char)buf[0]; }
}
