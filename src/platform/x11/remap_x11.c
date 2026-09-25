#include "core/platform.h"
#include "platform/x11/input_x11.h"
#include "platform/x11/display_x11.h"
#include "deps/log.h"
#include <linux/input.h>
#include <sys/time.h>
#include <string.h>
#if __has_include(<X11/extensions/XTest.h>)
#include <X11/extensions/XTest.h>
#endif

void Platform_EmitKey(int vk_code, int down) {
    int linux_code = x11_vk_to_linux_key(vk_code);
    if (!linux_code) {
        if (vk_code == VK_LCONTROL) linux_code = KEY_LEFTCTRL;
        else if (vk_code == VK_RCONTROL) linux_code = KEY_RIGHTCTRL;
        else if (vk_code == VK_LSHIFT) linux_code = KEY_LEFTSHIFT;
        else if (vk_code == VK_RSHIFT) linux_code = KEY_RIGHTSHIFT;
        else if (vk_code == VK_LMENU) linux_code = KEY_LEFTALT;
        else if (vk_code == VK_RMENU) linux_code = KEY_RIGHTALT;
        else if (vk_code == VK_LWIN) linux_code = KEY_LEFTMETA;
        else if (vk_code == VK_RWIN) linux_code = KEY_RIGHTMETA;
        else if (vk_code == 0xDC) linux_code = KEY_BACKSLASH;
        else if (vk_code == VK_UP) linux_code = KEY_UP;
        else if (vk_code == VK_DOWN) linux_code = KEY_DOWN;
        else if (vk_code == VK_LEFT) linux_code = KEY_LEFT;
        else if (vk_code == VK_RIGHT) linux_code = KEY_RIGHT;
        else if (vk_code == VK_RETURN) linux_code = KEY_ENTER;
        else if (vk_code == VK_SPACE) linux_code = KEY_SPACE;
        else if (vk_code == VK_TAB) linux_code = KEY_TAB;
        else if (vk_code == VK_ESCAPE) linux_code = KEY_ESC;
    }
    if (!linux_code) { log_warn("Platform_EmitKey no linux_code for vk=%d", vk_code); return; }
    log_warn("Platform_EmitKey vk=%d linux=%d down=%d uinput_fd=%d", vk_code, linux_code, down, g_x11_uinput_fd);
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    gettimeofday(&ev.time, NULL);
    ev.type = EV_KEY;
    ev.code = linux_code;
    ev.value = down ? 1 : 0;
    if (g_x11_uinput_fd >= 0) {
        x11_uinput_emit(&ev);
    } else {
#if __has_include(<X11/extensions/XTest.h>)
        Display* dpy = get_display();
        if (dpy) {
            log_warn("Platform_EmitKey fallback XTest for vk=%d (no uinput)", vk_code);
            KeySym ks = 0;
            if (vk_code >= 'A' && vk_code <= 'Z') ks = XK_A + (vk_code - 'A');
            else if (vk_code >= '0' && vk_code <= '9') ks = XK_0 + (vk_code - '0');
            else if (vk_code == VK_UP) ks = XK_Up;
            else if (vk_code == VK_DOWN) ks = XK_Down;
            else if (vk_code == VK_LEFT) ks = XK_Left;
            else if (vk_code == VK_RIGHT) ks = XK_Right;
            else if (vk_code == VK_RETURN) ks = XK_Return;
            else if (vk_code == VK_SPACE) ks = XK_space;
            else if (vk_code == VK_TAB) ks = XK_Tab;
            else if (vk_code == VK_ESCAPE) ks = XK_Escape;
            else if (vk_code == VK_OEM_3) ks = XK_grave;
            else if (vk_code == 0xDC) ks = XK_backslash;
            else if (vk_code == VK_LSHIFT) ks = XK_Shift_L;
            else if (vk_code == VK_RSHIFT) ks = XK_Shift_R;
            else if (vk_code == VK_LCONTROL) ks = XK_Control_L;
            else if (vk_code == VK_RCONTROL) ks = XK_Control_R;
            else if (vk_code == VK_LMENU) ks = XK_Alt_L;
            else if (vk_code == VK_RMENU) ks = XK_Alt_R;
            if (ks) {
                KeyCode kc = XKeysymToKeycode(dpy, ks);
                if (kc) {
                    XTestFakeKeyEvent(dpy, kc, down, CurrentTime);
                    XFlush(dpy);
                    log_warn("Platform_EmitKey XTest emitted ks=0x%lx kc=%d down=%d", ks, kc, down);
                } else {
                    log_warn("Platform_EmitKey XTest no keycode for ks=0x%lx vk=%d", ks, vk_code);
                }
            } else {
                log_warn("Platform_EmitKey XTest no keysym for vk=%d", vk_code);
            }
        } else {
            log_warn("Platform_EmitKey no display for XTest fallback vk=%d", vk_code);
        }
#else
        log_warn("Platform_EmitKey no uinput and no XTest for vk=%d", vk_code);
#endif
    }
}
