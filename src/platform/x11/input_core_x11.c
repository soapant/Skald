#define _GNU_SOURCE
#include "platform/x11/input_x11.h"
#include "deps/log.h"
#include "core/timer.h"
#include "core/hotkey_manager.h"
#include "core/remap_manager.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_file.h"
#include "helpers/helpers_ui.h"
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
#include <signal.h>
#include <pthread.h>

X11Keyboard g_x11_keyboards[X11_MAX_KEYBOARDS];
int g_x11_keyboard_count = 0;
int g_x11_uinput_fd = -1;
bool g_x11_suppress_release[KEY_MAX + 1] = {0};
bool g_x11_device_grabbed[X11_MAX_KEYBOARDS] = {0};

bool g_x11_ctrl_held = false;
bool g_x11_alt_held = false;
bool g_x11_shift_held = false;
pthread_mutex_t g_input_mtx = PTHREAD_MUTEX_INITIALIZER;

extern int pending_hotkey_vk;
extern int pending_hotkey_mod;
extern long pending_hotkey_timeMs;

bool x11_should_use_grab(void) {
    PROFILE_START(timer);
    if (getenv("WAYLAND_DISPLAY")) { PROFILE_END_VAR(timer, "input:x11_should_use_grab"); return false; }
    if (getenv("HYPRLAND_INSTANCE_SIGNATURE")) { PROFILE_END_VAR(timer, "input:x11_should_use_grab"); return false; }
    const char* t = getenv("XDG_SESSION_TYPE");
    if (t && strcmp(t, "wayland") == 0) { PROFILE_END_VAR(timer, "input:x11_should_use_grab"); return false; }
    PROFILE_END_VAR(timer, "input:x11_should_use_grab");
    return true;
}

static struct WindowManager *g_x11_wm = NULL;
void x11_set_window_manager(struct WindowManager *wm) { g_x11_wm = wm; }

static void x11_force_cleanup(void);
static void x11_install_cleanup_handlers(void);
static volatile sig_atomic_t g_x11_cleanup_done = 0;

static void x11_sig_handler(int sig) {
    (void)sig;
    x11_force_cleanup();
    signal(sig, SIG_DFL);
    raise(sig);
}

static void x11_term_handler(int sig) {
    (void)sig;
    x11_force_cleanup();
    if (g_x11_wm) {
        extern void WindowManager_QuitLinux(struct WindowManager *wm);
        WindowManager_QuitLinux(g_x11_wm);
    }
    _exit(0);
}

static void x11_force_cleanup(void) {
    if (g_x11_cleanup_done) return;
    g_x11_cleanup_done = 1;
    for (int i=0;i<g_x11_keyboard_count;i++) {
        if (g_x11_keyboards[i].fd >= 0) {
            ioctl(g_x11_keyboards[i].fd, EVIOCGRAB, 0);
        }
    }
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
}

static void x11_atexit_handler(void) { x11_force_cleanup(); }

static void x11_install_cleanup_handlers(void) {
    static bool installed = false;
    if (installed) return;
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
    usleep(150 * 1000);
    log_info("virtual keyboard created (fd=%d)", fd);
    PROFILE_END_VAR(timer, "input:x11_init_uinput");
    return fd;
}

void x11_destroy_uinput(void) {
    PROFILE_START(timer);
    if (g_x11_uinput_fd >= 0) { ioctl(g_x11_uinput_fd, UI_DEV_DESTROY); close(g_x11_uinput_fd); g_x11_uinput_fd = -1; }
    PROFILE_END_VAR(timer, "input:x11_destroy_uinput");
}

void x11_uinput_emit(const struct input_event *ev) {
    PROFILE_START(timer);
    if (g_x11_uinput_fd < 0) { PROFILE_END_VAR(timer, "input:x11_uinput_emit"); return; }
    if (write(g_x11_uinput_fd, ev, sizeof(*ev)) < 0) { if (errno != EAGAIN) perror("[UINPUT] write"); PROFILE_END_VAR(timer, "input:x11_uinput_emit"); return; }
    struct input_event syn; memset(&syn, 0, sizeof(syn)); gettimeofday(&syn.time, NULL); syn.type = EV_SYN; syn.code = SYN_REPORT; syn.value = 0;
    if (write(g_x11_uinput_fd, &syn, sizeof(syn)) < 0 && errno != EAGAIN) perror("[UINPUT] write syn");
    PROFILE_END_VAR(timer, "input:x11_uinput_emit");
}

void x11_scan_keyboards(void) {
    PROFILE_START(timer);
    x11_install_cleanup_handlers();
    DIR *dir = opendir("/dev/input");
    if (!dir) { PROFILE_END_VAR(timer, "input:x11_scan_keyboards"); return; }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && g_x11_keyboard_count < X11_MAX_KEYBOARDS) {
        if (strncmp(entry->d_name, "event", 5) == 0) {
            char full_path[256]; snprintf(full_path, sizeof(full_path), "/dev/input/%s", entry->d_name);
            int fd = open(full_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0) continue;
            unsigned long ev_bits[EV_MAX / 8 + 1] = {0};
            unsigned long keybits[(KEY_MAX/8+sizeof(unsigned long)-1)/sizeof(unsigned long)+1] = {0};
            struct input_id id;
            bool is_kb = false;
            if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) >= 0 &&
                (ev_bits[0] & (1UL << EV_KEY)) &&
                ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) >= 0 &&
                ioctl(fd, EVIOCGID, &id) >= 0) {
                if (id.vendor == 0x1234 && id.product == 0x5678) {
                    close(fd);
                    continue;
                }
                if (ev_bits[0] & (1UL << EV_REL)) {
                    close(fd);
                    continue;
                }
                if (ev_bits[0] & (1UL << EV_ABS)) {
                    close(fd);
                    continue;
                }
                keybits[0] &= ~1UL;
                int nwords = (0xFF + 1 + (int)(sizeof(unsigned long)*8)-1)/(int)(sizeof(unsigned long)*8);
                for (int i=0;i<nwords;i++) if (keybits[i]) { is_kb = true; break; }
            }
            if (is_kb) {
                g_x11_keyboards[g_x11_keyboard_count].fd = fd;
                strncpy(g_x11_keyboards[g_x11_keyboard_count].path, full_path, 255);
                g_x11_keyboard_count++;
                log_warn("x11_scan_keyboards grabbed keyboard %s fd=%d", full_path, fd);
            } else {
                close(fd);
            }
        }
    }
    closedir(dir);
    log_warn("x11_scan_keyboards found %d keyboards", g_x11_keyboard_count);
    PROFILE_END_VAR(timer, "input:x11_scan_keyboards");
}

int x11_keysym_to_vk(KeySym ks) {
    if (ks >= XK_a && ks <= XK_z) return 'A' + (ks - XK_a);
    if (ks >= XK_A && ks <= XK_Z) return 'A' + (ks - XK_A);
    if (ks >= XK_0 && ks <= XK_9) return '0' + (ks - XK_0);
    if (ks >= XK_F1 && ks <= XK_F12) return VK_F1 + (ks - XK_F1);
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

int x11_vk_to_linux_key(int vk) {
    if (vk >= 'A' && vk <= 'Z') return KEY_A + (vk - 'A');
    if (vk >= '0' && vk <= '9') return KEY_1 + (vk - '1');
    if (vk == '0') return KEY_0;
    if (vk == VK_SPACE) return KEY_SPACE;
    if (vk == VK_RETURN) return KEY_ENTER;
    if (vk == VK_BACK) return KEY_BACKSPACE;
    if (vk == VK_TAB) return KEY_TAB;
    if (vk == VK_ESCAPE) return KEY_ESC;
    if (vk == VK_OEM_1) return KEY_SEMICOLON;
    if (vk == VK_OEM_2) return KEY_SLASH;
    if (vk == VK_OEM_4) return KEY_LEFTBRACE;
    if (vk == VK_OEM_6) return KEY_RIGHTBRACE;
    if (vk == VK_OEM_COMMA) return KEY_COMMA;
    if (vk == VK_OEM_PERIOD) return KEY_DOT;
    if (vk == VK_OEM_7) return KEY_APOSTROPHE;
    if (vk == VK_OEM_3) return KEY_GRAVE;
    if (vk == VK_OEM_PLUS) return KEY_EQUAL;
    if (vk == VK_OEM_MINUS) return KEY_MINUS;
    if (vk == VK_UP) return KEY_UP;
    if (vk == VK_DOWN) return KEY_DOWN;
    if (vk == VK_LEFT) return KEY_LEFT;
    if (vk == VK_RIGHT) return KEY_RIGHT;
    return 0;
}

void x11_sync_held_from_phys(void) {
    int m = remap_current_mods();
    g_x11_ctrl_held = (m & HOTKEY_MOD_CTRL) != 0;
    g_x11_alt_held = (m & HOTKEY_MOD_ALT) != 0;
    g_x11_shift_held = (m & HOTKEY_MOD_SHIFT) != 0;
}

void x11_clear_suppress(void) {
    for (int i=0;i<=KEY_MAX;i++) g_x11_suppress_release[i]=false;
}

bool x11_is_suppressed(int linux_code) {
    if (linux_code<0 || linux_code>KEY_MAX) return false;
    return g_x11_suppress_release[linux_code];
}

void x11_set_suppress(int linux_code, bool v) {
    if (linux_code<0 || linux_code>KEY_MAX) return;
    g_x11_suppress_release[linux_code]=v;
}

bool x11_handle_global_key(int key_code, int value) {
    PROFILE_START(timer);
    remap_update_phys(ConvertLinuxKeyToWindowsVK(key_code), value);
    x11_sync_held_from_phys();
    log_info("x11_handle_global_key code=%d val=%d ctrl=%d alt=%d shift=%d", key_code, value, g_x11_ctrl_held, g_x11_alt_held, g_x11_shift_held);
    log_warn("x11_handle_global_key code=%d val=%d ctrl=%d alt=%d shift=%d", key_code, value, g_x11_ctrl_held, g_x11_alt_held, g_x11_shift_held);
    double seconds = Platform_GetUniversalTimeSeconds();
    if (seconds >= 300) { Platform_ResetUniversalTimer(); log_info("SUPPOSED TO CLEAR FILE"); }
    if (value != 1) { PROFILE_END_VAR(timer, "input:x11_handle_global_key"); return false; }
    int pressed_vk = ConvertLinuxKeyToWindowsVK(key_code);
    int modifiers = remap_current_mods();
    log_info("x11_handle_global_key vk=%d mods=%d", pressed_vk, modifiers);
    log_warn("x11_handle_global_key vk=%d mods=%d", pressed_vk, modifiers);
    if (hotkey_process(pressed_vk, modifiers)) {
        log_info("x11_handle_global_key HOTKEY MATCH vk=%d mods=%d", pressed_vk, modifiers);
        log_warn("x11_handle_global_key HOTKEY MATCH vk=%d mods=%d", pressed_vk, modifiers);
        pending_hotkey_vk = pressed_vk; pending_hotkey_mod = modifiers; struct timeval tv; gettimeofday(&tv,NULL); pending_hotkey_timeMs = tv.tv_sec*1000 + tv.tv_usec/1000;
        PROFILE_END_VAR(timer, "input:x11_handle_global_key");
        return true;
    }
    log_info("x11_handle_global_key no match vk=%d mods=%d", pressed_vk, modifiers);
    log_warn("x11_handle_global_key no match vk=%d mods=%d", pressed_vk, modifiers);
    PROFILE_END_VAR(timer, "input:x11_handle_global_key");
    return false;
}

bool x11_handle_evdev(struct input_event *ev, bool has_popup, bool grabbed) {
    PROFILE_START(timer);
    if (!ev) { PROFILE_END_VAR(timer, "input:x11_handle_evdev"); return false; }
    if (ev->type != EV_KEY) {
        if (grabbed && g_x11_uinput_fd >= 0) x11_uinput_emit(ev);
        PROFILE_END_VAR(timer, "input:x11_handle_evdev");
        return false;
    }
    int code = ev->code;
    int val = ev->value;
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
            if (grabbed && g_x11_uinput_fd>=0) x11_uinput_emit(ev);
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
        if (g_x11_uinput_fd >= 0) x11_uinput_emit(ev);
        else { static bool warned=false; if(!warned){ log_warn("grabbed but no uinput - key %d lost", code); warned=true; } }
    }
    PROFILE_END_VAR(timer, "input:x11_handle_evdev");
    return false;
}

void x11_cleanup_keyboards(void) {
    for (int i = 0; i < g_x11_keyboard_count; i++) {
        ioctl(g_x11_keyboards[i].fd, EVIOCGRAB, 0);
        close(g_x11_keyboards[i].fd);
    }
    g_x11_keyboard_count = 0;
}
