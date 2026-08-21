#include "common.h"
#include "core/modal.h"
#include "config.h"
#include "platform/platform.h"
#include "helpers.h"
#include "core/common_ui.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

static int Linux_KeysymToWindowsVK(KeySym keysym) {
    if (keysym >= XK_a && keysym <= XK_z) return 'A' + (keysym - XK_a);
    if (keysym >= XK_A && keysym <= XK_Z) return 'A' + (keysym - XK_A);
    if (keysym >= XK_0 && keysym <= XK_9) return '0' + (keysym - XK_0);
    if (keysym >= XK_F1 && keysym <= XK_F12) return VK_F1 + (keysym - XK_F1);

    switch (keysym) {
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
        case XK_comma: return VK_OEM_COMMA;
        case XK_period: return VK_OEM_PERIOD;
        case XK_semicolon: return VK_OEM_1;
        case XK_slash: return VK_OEM_2;
        case XK_apostrophe: return VK_OEM_7;
        case XK_bracketleft: return VK_OEM_4;
        case XK_bracketright: return VK_OEM_6;
        case XK_grave: return VK_OEM_3;
        case XK_minus: return VK_OEM_MINUS;
        case XK_equal: return VK_OEM_PLUS;
        default: return 0;
    }
}
bool Linux_HandlePromptKey(const XEvent* ev,PromptState *state) {
    if (ev->type != KeyPress) return false;

    int wParam = Linux_KeysymToWindowsVK(XLookupKeysym(&ev->xkey, 0));
    bool ctrl_pressed = (ev->xkey.state & ControlMask) != 0;
    bool shift_pressed = (ev->xkey.state & ShiftMask) != 0;
    if (wParam == VK_BACK) {
        size_t n = strlen(state->filter);
        if (state->cursor > 0) {
            memmove(state->filter + state->cursor - 1,
                    state->filter + state->cursor,
                    n - state->cursor + 1);
            state->cursor--;
        }
        if (g_refresh_callback) g_refresh_callback(state->filter, hListBox, state->has_listbox ? &state->listbox : NULL);
        return true;
    }
    if (wParam == VK_ESCAPE) {
      if (IsInsertMode()) {
        SetMode(MODE_NORMAL);
        return true;
      } else {
        selection_complete = true;
      }
    }
    if (wParam == VK_RIGHT) {
      CursorForward(&state->cursor, strlen(state->filter));
      return true;
    }
    if (wParam == VK_LEFT) {
       CursorBack(&state->cursor, strlen(state->filter));
      return true;
    }
    if (wParam == VK_UP) {
      if (state->has_listbox) UI_NavigateUp(&state->listbox, hListBox);
      return true;
    }
    if (wParam == VK_DOWN) {
      if (state->has_listbox) UI_NavigateDown(&state->listbox, hListBox);
      return true;
    }
    if (wParam == VK_RETURN) {
      if (state->has_listbox && UI_IsListBoxSelected(&state->listbox)) {
        const char* sel = state->listbox.populated_cache[state->listbox.selected_index];
        if (g_enter_callback) g_enter_callback(sel, &state->listbox);
        selection_complete = true;
      } else {
        if (g_enter_callback) g_enter_callback(state->filter, state->has_listbox ? &state->listbox : NULL);
        selection_complete = true;
      }
      return true;
    }

    return false;
}

void DrawEditControl(Display *dpy,Window* win,PromptState *state) {
    int screen = DefaultScreen(dpy);
    XftDraw* xd = XftDrawCreate(dpy, *win, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));

    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);

    int textW = 0, textH = 0;
    Platform_GetTextExtents(font,state->filter,state->width, &textW, &textH);

    unsigned long text_color_map = rgb_to_hex(COLOR_TEXT);
    unsigned long bg_color_map = rgb_to_hex(COLOR_BG);

    unsigned long vim_text_color_map;
    if (g_current_mode == MODE_NORMAL) {
      vim_text_color_map = rgb_to_hex(COLOR_NORMAL_MODE_TEXT);
    } else if (g_current_mode == MODE_INSERT) {
      vim_text_color_map = rgb_to_hex(COLOR_INSERT_MODE_TEXT);
    } else {
      vim_text_color_map = rgb_to_hex(COLOR_VISUAL_MODE_TEXT);
    }

    Visual* visual = DefaultVisual(dpy, screen);
    Colormap cmap = DefaultColormap(dpy, screen);

    XRenderColor rc_bg = {
        ((bg_color_map >> 16) & 0xFF) * 0x101,
        ((bg_color_map >> 8) & 0xFF) * 0x101,
        (bg_color_map & 0xFF) * 0x101,
        0xFFFF
    };
    XftColor bg_color;
    XftColorAllocValue(dpy, visual, cmap, &rc_bg, &bg_color);

    XRenderColor rc_text = {
        ((text_color_map >> 16) & 0xFF) * 0x101, ((text_color_map >> 8) & 0xFF) * 0x101, (text_color_map & 0xFF) * 0x101,
        0xFFFF
    };
    XftColor text_color;
    XftColorAllocValue(dpy, visual, cmap, &rc_text, &text_color);

    XRenderColor rc_vim_text_color = {
        ((vim_text_color_map >> 16) & 0xFF) * 0x101,
        ((vim_text_color_map >> 8) & 0xFF) * 0x101,
        (vim_text_color_map & 0xFF) * 0x101,
        0xFFFF
    };
    XftColor vim_text_color;
    XftColorAllocValue(dpy, visual, cmap, &rc_vim_text_color, &vim_text_color);
    int centerX = state->width / 2;
    int centerY = state->height / 2;
    int topLeftX = centerX - state->width / 2;                                                  
    int topLeftY = centerY - state->height / 2; 
    XftDrawRect(xd, &bg_color, topLeftX, topLeftY, state->width,80);                       
    draw_wrapped_text(dpy, xd, &text_color, font,state->filter,40,"LEFT",20);
    draw_wrapped_text(dpy, xd, &vim_text_color, font,g_vim_mode,0,"LEFT",5);
    draw_wrapped_text(dpy, xd, &text_color,font,state->window_title,0,"RIGHT",5);
    if (IsInsertMode()) {
        XftFont* xf = FH_TO_XFT(font);
        XGlyphInfo ext;
        XftTextExtentsUtf8(dpy, xf, (FcChar8*)state->filter, state->cursor, &ext);
        XftDrawRect(xd, &text_color,22 + ext.xOff, 47, 2,FontSize);
    } else {
        XftFont* xf = FH_TO_XFT(font);
        XGlyphInfo ext;
        XftTextExtentsUtf8(dpy, xf, (FcChar8*)state->filter, state->cursor, &ext);
        XftDrawRect(xd, &text_color,22 + ext.xOff, 47, 12,FontSize);
    }
}

bool Linux_RenderPromptUser(Display *dpy, Window *win, PromptState *state) {
    DrawEditControl(dpy, win,state);
    if (state->has_listbox && state->listbox.populated_count > 0) {
        // Minimal listbox rendering: could be expanded later
        // For now just ensure selection is maintained
    }
    return true;
}
