#define _GNU_SOURCE
#include "platform/x11/input.h"
#include "deps/log.h"
#include "core/timer.h"
#include "core/hotkey_manager.h"
#include "core/remap_manager.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
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

X11Keyboard g_x11_keyboards[X11_MAX_KEYBOARDS];
int g_x11_keyboard_count = 0;
int g_x11_uinput_fd = -1;
bool g_x11_suppress_release[KEY_MAX + 1] = {0};
bool g_x11_device_grabbed[X11_MAX_KEYBOARDS] = {0};

bool g_x11_ctrl_held = false;
bool g_x11_alt_held = false;
bool g_x11_shift_held = false;

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

int x11_init_uinput(void) {
    PROFILE_START(timer);
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) fd = open("/dev/input/uinput", O_WRONLY | O_NONBLOCK);
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
    DIR *dir = opendir("/dev/input");
    if (!dir) { PROFILE_END_VAR(timer, "input:x11_scan_keyboards"); return; }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && g_x11_keyboard_count < X11_MAX_KEYBOARDS) {
        if (strncmp(entry->d_name, "event", 5) == 0) {
            char full_path[256]; snprintf(full_path, sizeof(full_path), "/dev/input/%s", entry->d_name);
            int fd = open(full_path, O_RDONLY | O_NONBLOCK);
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

bool x11_handle_global_key(int key_code, int value) {
    PROFILE_START(timer);
    log_warn("x11_handle_global_key code=%d val=%d ctrl=%d alt=%d shift=%d", key_code, value, g_x11_ctrl_held, g_x11_alt_held, g_x11_shift_held);
    if (key_code == KEY_LEFTCTRL || key_code == KEY_RIGHTCTRL) {
        g_x11_ctrl_held = (value != 0);
    } else if (key_code == KEY_LEFTALT || key_code == KEY_RIGHTALT) {
        g_x11_alt_held = (value != 0);
    } else if (key_code == KEY_LEFTSHIFT || key_code == KEY_RIGHTSHIFT) {
        g_x11_shift_held = (value != 0);
    }
    remap_update_phys(ConvertLinuxKeyToWindowsVK(key_code), value);
    double seconds = Platform_GetUniversalTimeSeconds();
    if (seconds >= 300) { Platform_ResetUniversalTimer(); log_info("SUPPOSED TO CLEAR FILE"); }
    if (value != 1) { PROFILE_END_VAR(timer, "input:x11_handle_global_key"); return false; }
    int pressed_vk = ConvertLinuxKeyToWindowsVK(key_code);
    int modifiers = HOTKEY_MOD_NONE;
    if (g_x11_alt_held) modifiers |= HOTKEY_MOD_ALT;
    if (g_x11_shift_held) modifiers |= HOTKEY_MOD_SHIFT;
    if (g_x11_ctrl_held) modifiers |= HOTKEY_MOD_CTRL;
    log_warn("x11_handle_global_key vk=%d mods=%d", pressed_vk, modifiers);
    if (hotkey_process(pressed_vk, modifiers)) {
        log_warn("x11_handle_global_key HOTKEY MATCH vk=%d mods=%d", pressed_vk, modifiers);
        if (value == 1) { pending_hotkey_vk = pressed_vk; pending_hotkey_mod = modifiers; struct timeval tv; gettimeofday(&tv,NULL); pending_hotkey_timeMs = tv.tv_sec*1000 + tv.tv_usec/1000; }
        PROFILE_END_VAR(timer, "input:x11_handle_global_key");
        return true;
    }
    log_warn("x11_handle_global_key no match vk=%d mods=%d", pressed_vk, modifiers);
    PROFILE_END_VAR(timer, "input:x11_handle_global_key");
    return false;
}

void x11_cleanup_keyboards(void) {
    for (int i = 0; i < g_x11_keyboard_count; i++) {
        ioctl(g_x11_keyboards[i].fd, EVIOCGRAB, 0);
        close(g_x11_keyboards[i].fd);
    }
    g_x11_keyboard_count = 0;
}
