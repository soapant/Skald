#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include "platform/x11/drawing_x11.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include "core/vim.h"
#include "platform/x11/input_x11.h"
#include "bindings/bindings.h"
#include "bindings/vim_motions.h"
#include "bindings/vim_helpers.h"
#include "core/history.h"
#include "core/prompt_controller.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <sys/time.h>
#include <string.h>
#include "core/timer.h"
#include "platform/draw.h"
#include "platform/x11/input_x11.h"
#include "core/remap_manager.h"

extern int pending_hotkey_vk;
extern int pending_hotkey_mod;
extern long pending_hotkey_timeMs;
extern bool g_x11_ctrl_held, g_x11_alt_held, g_x11_shift_held;

bool should_swallow_pending(int wParam, int mods) {
    PROFILE_START(timer);
    if (pending_hotkey_vk == 0) { PROFILE_END_VAR(timer, "subclasses_x11:should_swallow_pending"); return false; }
    struct timeval tv; gettimeofday(&tv,NULL);
    long now = tv.tv_sec*1000 + tv.tv_usec/1000;
    if (now - pending_hotkey_timeMs > 800) {
        pending_hotkey_vk = 0; pending_hotkey_mod = 0; PROFILE_END_VAR(timer, "subclasses_x11:should_swallow_pending"); return false;
    }
    if (wParam == pending_hotkey_vk) {
        pending_hotkey_vk = 0; pending_hotkey_mod = 0;
        PROFILE_END_VAR(timer, "subclasses_x11:should_swallow_pending");
        return true;
    }
    PROFILE_END_VAR(timer, "subclasses_x11:should_swallow_pending");
    return false;
}

int x11_keysym_to_windows_vk(KeySym keysym) {
    PROFILE_START(timer);
    if (keysym >= XK_a && keysym <= XK_z) { PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return 'A' + (keysym - XK_a); }
    if (keysym >= XK_A && keysym <= XK_Z) { PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return 'A' + (keysym - XK_A); }
    if (keysym >= XK_0 && keysym <= XK_9) { PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '0' + (keysym - XK_0); }
    if (keysym >= XK_F1 && keysym <= XK_F12) { PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_F1 + (keysym - XK_F1); }

    switch (keysym) {
        case XK_Escape: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_ESCAPE;
        case XK_Return:
        case XK_KP_Enter: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_RETURN;
        case XK_Tab: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_TAB;
        case XK_BackSpace: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_BACK;
        case XK_Up: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_UP;
        case XK_Down: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_DOWN;
        case XK_Left: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_LEFT;
        case XK_Right: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_RIGHT;
        case XK_Delete: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_DELETE;
        case XK_Home: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_HOME;
        case XK_End: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_END;
        case XK_Page_Up: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_PRIOR;
        case XK_Page_Down: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_NEXT;
        case XK_space: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_SPACE;
        case XK_Control_L: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_LCONTROL;
        case XK_Control_R: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_RCONTROL;
        case XK_Shift_L: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_LSHIFT;
        case XK_Shift_R: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_RSHIFT;
        case XK_Alt_L: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_LMENU;
        case XK_Alt_R: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_RMENU;
        case XK_comma: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_OEM_COMMA;
        case XK_period: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_OEM_PERIOD;
        case XK_semicolon: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_OEM_1;
        case XK_slash: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_OEM_2;
        case XK_apostrophe: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_OEM_7;
        case XK_bracketleft: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_OEM_4;
        case XK_bracketright: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_OEM_6;
        case XK_grave: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_OEM_3;
        case XK_minus: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_OEM_MINUS;
        case XK_equal: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_OEM_PLUS;
        case XK_colon: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return VK_OEM_1;
        case XK_less: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '<';
        case XK_greater: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '>';
        case XK_question: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '?';
        case XK_quotedbl: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '"';
        case XK_braceleft: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '{';
        case XK_braceright: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '}';
        case XK_asciitilde: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '~';
        case XK_underscore: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '_';
        case XK_plus: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '+';
        case XK_exclam: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '!';
        case XK_at: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '@';
        case XK_numbersign: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '#';
        case XK_dollar: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '$';
        case XK_percent: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '%';
        case XK_asciicircum: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '^';
        case XK_ampersand: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '&';
        case XK_asterisk: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '*';
        case XK_parenleft: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '(';
        case XK_parenright: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return ')';
        case XK_bar: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return '|';
        default: PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk"); return 0;
    }
    PROFILE_END_VAR(timer, "subclasses_x11:x11_keysym_to_windows_vk");
}

void ReadModifiers(const XEvent* ev, bool* ctrl_pressed, bool* shift_pressed, bool* alt_pressed) {
    PROFILE_START(timer);
    int m = remap_current_mods();
    *ctrl_pressed = ((ev->xkey.state & ControlMask) != 0) || (m & HOTKEY_MOD_CTRL);
    *shift_pressed = ((ev->xkey.state & ShiftMask) != 0) || (m & HOTKEY_MOD_SHIFT);
    *alt_pressed = ((ev->xkey.state & Mod1Mask) != 0) || (m & HOTKEY_MOD_ALT);
    PROFILE_END_VAR(timer, "subclasses_x11:ReadModifiers");
}

bool TryVimMotion(int wParam, bool ctrl_pressed, bool shift_pressed, bool alt_pressed) {
    PROFILE_START(timer);
    bool r = PromptController_TryVimMotion(wParam, ctrl_pressed, shift_pressed, alt_pressed);
    PROFILE_END_VAR(timer, "subclasses_x11:TryVimMotion");
    return r;
}

