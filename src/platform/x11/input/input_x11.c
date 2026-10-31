#include "platform/x11/input_x11.h"
#include "core/prompt_controller.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <sys/time.h>
#include "core/timer.h"
#include "platform/x11/input_x11.h"
#include "core/remap_manager.h"

extern int pending_hotkey_vk;
extern int pending_hotkey_mod;
extern long pending_hotkey_timeMs;
extern bool g_x11_ctrl_held, g_x11_alt_held, g_x11_shift_held;

int x11_keysym_to_windows_vk(KeySym keysym) {
    if (keysym >= XK_a && keysym <= XK_z) return 'A' + (keysym - XK_a);
    if (keysym >= XK_A && keysym <= XK_Z) return 'A' + (keysym - XK_A);
    if (keysym >= XK_0 && keysym <= XK_9) return '0' + (keysym - XK_0);
    if (keysym >= XK_F1 && keysym <= XK_F12) return VK_F1 + (keysym - XK_F1);

    switch (keysym) {
        case XK_Escape:  return VK_ESCAPE;
        case XK_Return:
        case XK_KP_Enter:  return VK_RETURN;
        case XK_Tab:  return VK_TAB;
        case XK_BackSpace:  return VK_BACK;
        case XK_Up:  return VK_UP;
        case XK_Down:  return VK_DOWN;
        case XK_Left:  return VK_LEFT;
        case XK_Right:  return VK_RIGHT;
        case XK_Delete:  return VK_DELETE;
        case XK_Home:  return VK_HOME;
        case XK_End:  return VK_END;
        case XK_Page_Up:  return VK_PRIOR;
        case XK_Page_Down:  return VK_NEXT;
        case XK_space:  return VK_SPACE;
        case XK_Control_L:  return VK_LCONTROL;
        case XK_Control_R:  return VK_RCONTROL;
        case XK_Shift_L:  return VK_LSHIFT;
        case XK_Shift_R:  return VK_RSHIFT;
        case XK_Alt_L:  return VK_LMENU;
        case XK_Alt_R:  return VK_RMENU;
        case XK_comma: 
        case XK_less:  return VK_OEM_COMMA;
        case XK_period: 
        case XK_greater:  return VK_OEM_PERIOD;
        case XK_semicolon: 
        case XK_colon:  return VK_OEM_1;
        case XK_slash: 
        case XK_question:  return VK_OEM_2;
        case XK_apostrophe: 
        case XK_quotedbl:  return VK_OEM_7;
        case XK_bracketleft: 
        case XK_braceleft:  return VK_OEM_4;
        case XK_bracketright: 
        case XK_braceright:  return VK_OEM_6;
        case XK_grave: 
        case XK_asciitilde:  return VK_OEM_3;
        case XK_minus: 
        case XK_underscore:  return VK_OEM_MINUS;
        case XK_equal: 
        case XK_plus:  return VK_OEM_PLUS;
        case XK_backslash:
        case XK_bar:  return VK_OEM_5;
        case XK_exclam:  return '1';
        case XK_at:  return '2';
        case XK_numbersign:  return '3';
        case XK_dollar:  return '4';
        case XK_percent:  return '5';
        case XK_asciicircum:  return '6';
        case XK_ampersand:  return '7';
        case XK_asterisk:  return '8';
        case XK_parenleft:  return '9';
        case XK_parenright:  return '0';
        default:  return 0;
    }
    
}

