#include "common.h"
#include "core/modal.h"
#include "config.h"
#include "platform/platform.h"
#include "helpers.h"
#include "core/common_ui.h"
#include "core/vim_context.h"
#include "bindings/bindings.h"
#include "bindings/vim_motions.h"
#include "bindings/vim_helpers.h"
#include "bindings/hotkey_definitions.h"
#include "core/number_shortcut.h"
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
    bool alt_pressed = (ev->xkey.state & Mod1Mask) != 0;
    {
        int mod = HOTKEY_MOD_NONE;
        if (ctrl_pressed) mod |= HOTKEY_MOD_CTRL;
        if (shift_pressed) mod |= HOTKEY_MOD_SHIFT;
        if (alt_pressed) mod |= HOTKEY_MOD_ALT;
        MotionAction act; const char* seq; int typed;
        if (CheckVimMotion(wParam, mod, &act, &seq, &typed)) {
            ExecuteMotion(act, seq, typed);
            return true;
        }
    }
    if (wParam == VK_BACK) {
        if (ctrl_pressed) {
            state->filter[0] = '\0';
            state->cursor = 0;
            if (g_refresh_callback) g_refresh_callback(state->filter, hListBox, state->has_listbox ? &state->listbox : NULL);
            return true;
        }
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
        return true;
      }
    }
    // Number shortcuts: Ctrl+0..9
    if (wParam >= '0' && wParam <= '9') {
      if (ctrl_pressed) {
        // HandleNumberShortcut expects WindowHandles; on X11 hListBox/hEditControl are dummy
        HandleNumberShortcut(wParam, hListBox, g_ActiveEditControl, state->has_listbox ? &state->listbox : NULL);
        return true;
      }
    }
    if (wParam == VK_RIGHT) {
      // Allow cursor movement even when list has selection (match Win32 behavior)
      CursorForward(&state->cursor, strlen(state->filter));
      return true;
    }
    if (wParam == VK_LEFT) {
       CursorBack(&state->cursor, strlen(state->filter));
      return true;
    }
    if (wParam == VK_UP) {
      if (ctrl_pressed) {
        if (state) {
          VimContext ctx = VimCtx_FromState(state);
          SetCursorToStartOfText(&ctx);
        }
        return true;
      }
      if (state->has_listbox) UI_NavigateUp(&state->listbox, hListBox);
      return true;
    }
    if (wParam == VK_DOWN) {
      if (ctrl_pressed) {
        if (state) {
          VimContext ctx = VimCtx_FromState(state);
          SetCursorToEndOfText(&ctx);
        }
        return true;
      }
      if (state->has_listbox) UI_NavigateDown(&state->listbox, hListBox);
      return true;
    }
    if (wParam == VK_DELETE) {
      if (state && state->cursor < (int)strlen(state->filter)) {
        size_t n = strlen(state->filter);
        memmove(state->filter + state->cursor,
                state->filter + state->cursor + 1,
                n - state->cursor);
        if (g_refresh_callback) g_refresh_callback(state->filter, hListBox, state->has_listbox ? &state->listbox : NULL);
      }
      return true;
    }
    if (wParam == VK_HOME) {
      if (state) {
        VimContext ctx = VimCtx_FromState(state);
        SetCursorToStartOfText(&ctx);
      }
      return true;
    }
    if (wParam == VK_END) {
      if (state) {
        VimContext ctx = VimCtx_FromState(state);
        SetCursorToEndOfText(&ctx);
      }
      return true;
    }
    if (wParam == VK_TAB) {
      if (g_tab_callback) g_tab_callback(&state->listbox);
      return true;
    }
    if (wParam == VK_RETURN) {
      bool is_ctrl = ctrl_pressed;
      const char* sel = NULL;
      char filter_copy[sizeof(state->filter)];
      if (state->has_listbox && UI_IsListBoxSelected(&state->listbox)) {
        sel = state->listbox.populated_cache[state->listbox.selected_index];
      } else {
        strncpy(filter_copy, state->filter, sizeof(filter_copy)-1);
        filter_copy[sizeof(filter_copy)-1]='\0';
        sel = filter_copy;
      }
      if (is_ctrl) {
        if (g_ctrl_enter_callback) g_ctrl_enter_callback(sel, &state->listbox);
        else if (g_enter_callback) g_enter_callback(sel, &state->listbox);
        return true;
      } else {
        if (g_enter_callback) g_enter_callback(sel, &state->listbox);
        selection_complete = true;
        return true;
      }
    }
    if (wParam == EDIT_TEXT_KEY) {
      bool do_edit = (ctrl_pressed && IsInsertMode()) || IsNormalMode();
      if (do_edit && g_edit_callback) {
        const char* sel = "";
        if (state->has_listbox && UI_IsListBoxSelected(&state->listbox)) sel = state->listbox.populated_cache[state->listbox.selected_index];
        else sel = state->filter;
        g_edit_callback(sel, &state->listbox);
        return true;
      }
    }
    if (wParam == COPY_SELECTION_KEY) {
      if (shift_pressed || ctrl_pressed) {
        const char* sel = "";
        if (state->has_listbox && UI_IsListBoxSelected(&state->listbox)) sel = state->listbox.populated_cache[state->listbox.selected_index];
        else sel = state->filter;
        int type = shift_pressed ? 0 : 1;
        if (g_copy_callback) {
          int idx = UI_IsListBoxSelected(&state->listbox) ? state->listbox.selected_index : -1;
          g_copy_callback(sel, idx, type);
        } else {
          Platform_CopyToClipboard(sel);
        }
        return true;
      }
    }

    return false;
}

void DrawPromptUserEditControl(Display *dpy,Window* win,PromptState *state) {
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
    XftColorFree(dpy, visual, cmap, &bg_color);
    XftColorFree(dpy, visual, cmap, &text_color);
    XftColorFree(dpy, visual, cmap, &vim_text_color);
    XftDrawDestroy(xd);
    Platform_DestroyFont(font);
}

void DrawPromptUserListBox(Display *dpy,Window* win,PromptState *state) {
      if (!state->has_listbox) return;
      ListBoxInfo *lb = &state->listbox;
      int screen = DefaultScreen(dpy);
      XftDraw *xd = XftDrawCreate(dpy, *win, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));
      FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
      XftFont *xf = FH_TO_XFT(font);

      int y0 = 100;
      int itemH = FontSize + 34;
      // Always clear list area, even when empty, to avoid stale items
      {
        Visual *visClear = DefaultVisual(dpy, screen);
        Colormap cmapClear = DefaultColormap(dpy, screen);
        XRenderColor rc0={(rgb_to_hex(COLOR_BG)>>16&0xFF)*0x101,(rgb_to_hex(COLOR_BG)>>8&0xFF)*0x101,(rgb_to_hex(COLOR_BG)&0xFF)*0x101,0xFFFF};
        XftColor c0; XftColorAllocValue(dpy, visClear, cmapClear, &rc0, &c0);
        XftDrawRect(xd, &c0, 0, y0, state->width, state->height - y0);
        XftColorFree(dpy, visClear, cmapClear, &c0);
      }
      if (!lb->populated_count) {
        XftDrawDestroy(xd);
        Platform_DestroyFont(font);
        return;
      }
      int visible = (state->height - y0) / itemH;
      if (visible < 1) visible = 1;
      if (visible > lb->populated_count) visible = lb->populated_count;

      if (lb->selected_index >= 0) {
        if (lb->selected_index < lb->view_offset + PaddingForScrolling)
          lb->view_offset = lb->selected_index - PaddingForScrolling;
        else if (lb->selected_index >= lb->view_offset + visible - PaddingForScrolling)
          lb->view_offset = lb->selected_index - visible + PaddingForScrolling + 1;
      }
      if (lb->view_offset < 0) lb->view_offset = 0;
      if (lb->view_offset + visible > lb->populated_count)
        lb->view_offset = lb->populated_count - visible;
      if (lb->view_offset < 0) lb->view_offset = 0;

      Visual *vis = DefaultVisual(dpy, screen);
      Colormap cmap = DefaultColormap(dpy, screen);
      for (int i = 0; i < visible; i++) {
        int idx = lb->view_offset + i;
        int y = y0 + i * itemH;
        bool sel = (idx == lb->selected_index);
        unsigned long bg = sel ? rgb_to_hex(COLOR_SELECTED_TEXT) : rgb_to_hex(COLOR_BG);
        XRenderColor rc={ (bg>>16&0xFF)*0x101,(bg>>8&0xFF)*0x101,(bg&0xFF)*0x101,0xFFFF};
        XftColor bgc; XftColorAllocValue(dpy, vis, cmap, &rc, &bgc);
        XftDrawRect(xd, &bgc, 0, y, state->width, itemH);

        unsigned long fg_text = rgb_to_hex(COLOR_TEXT);
        XRenderColor rc2_text={(fg_text>>16&0xFF)*0x101,(fg_text>>8&0xFF)*0x101,(fg_text&0xFF)*0x101,0xFFFF};
        XftColor fcoltext; XftColorAllocValue(dpy, vis, cmap, &rc2_text, &fcoltext);

        float ft = (float)idx / 32.0f;
        unsigned long fg_rainbow = rgb_to_hex(GetRainbowColor(ft, 0.55f, GradientBrightness));
        XRenderColor rc2_rainbow={(fg_rainbow>>16&0xFF)*0x101,(fg_rainbow>>8&0xFF)*0x101,(fg_rainbow&0xFF)*0x101,0xFFFF};
        XftColor fcolrainbow; XftColorAllocValue(dpy, vis, cmap, &rc2_rainbow, &fcolrainbow);

        if (RainbowText) {
            char num[8];
            snprintf(num,sizeof(num),"%d.",idx+1);
            draw_wrapped_text(dpy, xd, &fcolrainbow, font, num, y, "LEFT",10);
            /* Truncate cached text to avoid X11 BadLength on X_PolyText8 */
            char cache_trunc[256];
            snprintf(cache_trunc, sizeof(cache_trunc), "%.80s", lb->populated_cache[idx]);
            draw_wrapped_text(dpy, xd, &fcolrainbow, font, cache_trunc, y, "LEFT", 55);
        } else {
            char num[8];
            snprintf(num,sizeof(num),"%d.",idx+1);
            draw_wrapped_text(dpy, xd, &fcoltext, font, num, y, "LEFT", 10);
            /* Truncate cached text to avoid X11 BadLength on X_PolyText8 */
            char cache_trunc[256];
            snprintf(cache_trunc, sizeof(cache_trunc), "%.80s", lb->populated_cache[idx]);
            draw_wrapped_text(dpy, xd, &fcoltext, font, cache_trunc, y, "LEFT", 45);
        }

        XftColorFree(dpy, vis, cmap, &bgc);
        XftColorFree(dpy, vis, cmap, &fcoltext);
        XftColorFree(dpy, vis, cmap, &fcolrainbow);
      }
      XftDrawDestroy(xd);
      Platform_DestroyFont(font);
}

bool Linux_RenderPromptUser(Display *dpy, Window *win, PromptState *state) {
    DrawPromptUserEditControl(dpy, win,state);
    if (state->has_listbox) {
        DrawPromptUserListBox(dpy,win,state);
    }
    XFlush(dpy);
    return true;
}
