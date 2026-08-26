#include "common.h"
#include "core/modal.h"
#include "config.h"
#include "platform/platform.h"
#include "helpers.h"
#include "core/common_ui.h"
#include "core/vim.h"
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
#include <sys/time.h>
#include <string.h>


extern int pending_hotkey_vk;
extern int pending_hotkey_mod;
extern long pending_hotkey_timeMs;
static bool should_swallow_pending(int wParam, int mods) {
    if (pending_hotkey_vk == 0) return false;
    struct timeval tv; gettimeofday(&tv,NULL);
    long now = tv.tv_sec*1000 + tv.tv_usec/1000;
    if (now - pending_hotkey_timeMs > 800) {
        pending_hotkey_vk = 0; pending_hotkey_mod = 0; return false;
    }
    
    
    if (wParam == pending_hotkey_vk) {
        pending_hotkey_vk = 0; pending_hotkey_mod = 0;
        return true;
    }
    
    return false;
}

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
extern bool g_Linux_CtrlHeld, g_Linux_AltHeld, g_Linux_ShiftHeld;

static void ReadModifiers(const XEvent* ev, bool* ctrl_pressed, bool* shift_pressed, bool* alt_pressed) {
    *ctrl_pressed = ((ev->xkey.state & ControlMask) != 0) || g_Linux_CtrlHeld;
    *shift_pressed = ((ev->xkey.state & ShiftMask) != 0) || g_Linux_ShiftHeld;
    *alt_pressed = ((ev->xkey.state & Mod1Mask) != 0) || g_Linux_AltHeld;
}

static bool TryVimMotion(int wParam, bool ctrl_pressed, bool shift_pressed, bool alt_pressed) {
    int mod = HOTKEY_MOD_NONE;
    if (ctrl_pressed) mod |= HOTKEY_MOD_CTRL;
    if (shift_pressed) mod |= HOTKEY_MOD_SHIFT;
    if (alt_pressed) mod |= HOTKEY_MOD_ALT;
    MotionAction act; const char* seq; int typed;
    if (CheckVimMotion(wParam, mod, &act, &seq, &typed)) {
        ExecuteMotion(act, seq, typed);
        return true;
    }
    return false;
}

static void EditBufferBackspace(char* buf, size_t* cursor) {
    size_t n = strlen(buf);
    if (*cursor > 0) {
        memmove(buf + *cursor - 1,
                buf + *cursor,
                n - *cursor + 1);
        (*cursor)--;
    }
}

static bool EditBufferDeleteChar(char* buf, size_t* cursor) {
    size_t n = strlen(buf);
    if (*cursor >= n) return false;
    memmove(buf + *cursor,
            buf + *cursor + 1,
            n - *cursor);
    return true;
}

bool Linux_HandlePromptKey(const XEvent* ev,PromptState *state) {
    if (ev->type != KeyPress) return false;

    int wParam = Linux_KeysymToWindowsVK(XLookupKeysym(&ev->xkey, 0));
    bool ctrl_pressed, shift_pressed, alt_pressed;
    ReadModifiers(ev, &ctrl_pressed, &shift_pressed, &alt_pressed);
    if (should_swallow_pending(wParam, (ctrl_pressed?HOTKEY_MOD_CTRL:0)|(shift_pressed?HOTKEY_MOD_SHIFT:0)|(alt_pressed?HOTKEY_MOD_ALT:0))) return true;
    if (TryVimMotion(wParam, ctrl_pressed, shift_pressed, alt_pressed)) return true;
    if (wParam == VK_BACK) {
        if (ctrl_pressed) {
            state->filter[0] = '\0';
            state->cursor = 0;
        } else {
            EditBufferBackspace(state->filter, &state->cursor);
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
    
    if (wParam >= '0' && wParam <= '9') {
      if (ctrl_pressed) {
        
        HandleNumberShortcut(wParam, hListBox, g_ActiveEditControl, state->has_listbox ? &state->listbox : NULL);
        return true;
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
      if (ctrl_pressed) {
        if (state) {
          VimContext ctx = Vim_FromState(state);
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
          VimContext ctx = Vim_FromState(state);
          SetCursorToEndOfText(&ctx);
        }
        return true;
      }
      if (state->has_listbox) UI_NavigateDown(&state->listbox, hListBox);
      return true;
    }
    if (wParam == VK_DELETE) {
      if (state && EditBufferDeleteChar(state->filter, &state->cursor)) {
        if (g_refresh_callback) g_refresh_callback(state->filter, hListBox, state->has_listbox ? &state->listbox : NULL);
      }
      return true;
    }
    if (wParam == VK_HOME) {
      if (state) {
        VimContext ctx = Vim_FromState(state);
        SetCursorToStartOfText(&ctx);
      }
      return true;
    }
    if (wParam == VK_END) {
      if (state) {
        VimContext ctx = Vim_FromState(state);
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
        if (g_ctrl_enter_callback) {
          g_ctrl_enter_callback(sel, &state->listbox);
        }
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
bool Linux_HandleSimplePromptKey(const XEvent* ev,SimplePromptState *state) {
    if (ev->type != KeyPress) return false;

    int wParam = Linux_KeysymToWindowsVK(XLookupKeysym(&ev->xkey, 0));
    bool ctrl_pressed, shift_pressed, alt_pressed;
    ReadModifiers(ev, &ctrl_pressed, &shift_pressed, &alt_pressed);
    if (should_swallow_pending(wParam, (ctrl_pressed?HOTKEY_MOD_CTRL:0)|(shift_pressed?HOTKEY_MOD_SHIFT:0)|(alt_pressed?HOTKEY_MOD_ALT:0))) return true;
    if (TryVimMotion(wParam, ctrl_pressed, shift_pressed, alt_pressed)) return true;
    if (wParam == VK_BACK) {
        if (ctrl_pressed) {
            state->user_input[0] = '\0';
            state->cursor = 0;
        } else {
            EditBufferBackspace(state->user_input, &state->cursor);
        }
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
    if (wParam == VK_RIGHT) {
      CursorForward(&state->cursor, strlen(state->user_input));
      return true;
    }
    if (wParam == VK_LEFT) {
       CursorBack(&state->cursor, strlen(state->user_input));
      return true;
    }
    if (wParam == VK_RETURN) {
      selection_complete = true;
      return true;
    }
    return false;
}

bool Linux_HandleEditPopupKey(const XEvent* ev, SimplePromptState* state, bool* confirmed, bool* cancelled) {
    *confirmed = false;
    *cancelled = false;
    if (ev->type != KeyPress) return false;

    int wParam = Linux_KeysymToWindowsVK(XLookupKeysym(&ev->xkey, 0));
    bool ctrl_pressed, shift_pressed, alt_pressed;
    ReadModifiers(ev, &ctrl_pressed, &shift_pressed, &alt_pressed);
    if (should_swallow_pending(wParam, (ctrl_pressed?HOTKEY_MOD_CTRL:0)|(shift_pressed?HOTKEY_MOD_SHIFT:0)|(alt_pressed?HOTKEY_MOD_ALT:0))) return true;
    if (TryVimMotion(wParam, ctrl_pressed, shift_pressed, alt_pressed)) return true;
    if (wParam == VK_BACK) {
        EditBufferBackspace(state->user_input, &state->cursor);
        return true;
    }
    if (wParam == VK_ESCAPE) {
      *cancelled = true;
      return true;
    }
    if (wParam == VK_RETURN) {
      *confirmed = true;
      return true;
    }
    if (wParam == VK_RIGHT) {
      CursorForward(&state->cursor, strlen(state->user_input));
      return true;
    }
    if (wParam == VK_LEFT) {
       CursorBack(&state->cursor, strlen(state->user_input));
      return true;
    }
    if (wParam == VK_DELETE) {
      EditBufferDeleteChar(state->user_input, &state->cursor);
      return true;
    }
    if (wParam == VK_HOME) {
      state->cursor = 0;
      return true;
    }
    if (wParam == VK_END) {
      state->cursor = strlen(state->user_input);
      return true;
    }
    return false;
}

void DrawSimplePromptUserEditControl(Display *dpy, Window *win, SimplePromptState *state) {
    int screen = DefaultScreen(dpy);
    XftDraw* xd = Linux_CreateXftDraw(dpy, *win);

    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    XftFont* xf = FH_TO_XFT(font);

    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, xf, (FcChar8*)state->display_message, strlen(state->display_message), &ext);
    int text_width = ext.xOff;

    Visual* visual = DefaultVisual(dpy, screen);
    Colormap cmap = DefaultColormap(dpy, screen);

    XftColor bg_color, text_color, vim_text_color;
    Linux_XftAllocColor(dpy, visual, cmap, rgb_to_hex(COLOR_BG), OpacityAlpha16(), &bg_color);
    Linux_XftAllocColor(dpy, visual, cmap, rgb_to_hex(COLOR_TEXT), 0xFFFF, &text_color);
    Linux_XftAllocColor(dpy, visual, cmap, VimModeTextColor(), 0xFFFF, &vim_text_color);

    int centerX = state->width / 2;
    int centerY = state->height / 2;
    int topLeftX = centerX - state->width / 2;
    int topLeftY = centerY - state->height / 2;
    XftDrawRect(xd, &bg_color, topLeftX, topLeftY, state->width, state->height);

    draw_wrapped_text(dpy, xd, &vim_text_color, font, g_vim_mode, 0, "LEFT", 5);
    draw_wrapped_text(dpy, xd, &text_color, font, state->window_title, 0, "RIGHT", 5);
    int centered_x = (state->width - text_width) / 2;
    draw_wrapped_text(dpy, xd, &text_color, font, state->display_message, 70, "LEFT", centered_x);

    int cursor_x = 0;
    int text_len = strlen(state->user_input);
    int scroll = Linux_UpdateEditScrollOffset(dpy, xf, state->user_input, text_len, state->cursor, state->width, state->scroll_offset, &cursor_x);
    state->scroll_offset = scroll;
    int draw_x = 20 - scroll;
    int draw_y = 40 + (xf ? xf->ascent : FontSize);
    if (xf && text_len > 0) {
        XftDrawStringUtf8(xd, &text_color, xf, draw_x, draw_y, (FcChar8*)state->user_input, text_len);
    }
    Linux_DrawEditCursor(xd, &text_color, cursor_x, scroll, state->width);

    XftColorFree(dpy, visual, cmap, &bg_color);
    XftColorFree(dpy, visual, cmap, &text_color);
    XftColorFree(dpy, visual, cmap, &vim_text_color);
    XftDrawDestroy(xd);
    Platform_DestroyFont(font);
}

void DrawPromptUserEditControl(Display *dpy,Window* win,PromptState *state) {
    int screen = DefaultScreen(dpy);
    XftDraw* xd = Linux_CreateXftDraw(dpy, *win);

    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    XftFont* xf = FH_TO_XFT(font);

    unsigned long window_title_text_color_map;
    eval_window_title_color(state,&window_title_text_color_map);

    Visual* visual = DefaultVisual(dpy, screen);
    Colormap cmap = DefaultColormap(dpy, screen);

    XftColor bg_color, text_color, window_title_text_color, vim_text_color;
    Linux_XftAllocColor(dpy, visual, cmap, rgb_to_hex(COLOR_BG), OpacityAlpha16(), &bg_color);
    Linux_XftAllocColor(dpy, visual, cmap, rgb_to_hex(COLOR_TEXT), 0xFFFF, &text_color);
    Linux_XftAllocColor(dpy, visual, cmap, window_title_text_color_map, 0xFFFF, &window_title_text_color);
    Linux_XftAllocColor(dpy, visual, cmap, VimModeTextColor(), 0xFFFF, &vim_text_color);

    int centerX = state->width / 2;
    int centerY = state->height / 2;
    int topLeftX = centerX - state->width / 2;
    int topLeftY = centerY - state->height / 2;
    XftDrawRect(xd, &bg_color, topLeftX, topLeftY, state->width,100);
    draw_wrapped_text(dpy, xd, &vim_text_color, font,g_vim_mode,0,"LEFT",5);
    draw_wrapped_text(dpy, xd, &window_title_text_color,font,state->window_title,0,"RIGHT",5);

    int cursor_x = 0;
    int text_len = strlen(state->filter);
    int scroll = Linux_UpdateEditScrollOffset(dpy, xf, state->filter, text_len, state->cursor, state->width, state->scroll_offset, &cursor_x);
    state->scroll_offset = scroll;
    int draw_x = 20 - scroll;
    int draw_y = 40 + (xf ? xf->ascent : FontSize);
    if (xf && text_len > 0) {
        XftDrawStringUtf8(xd, &text_color, xf, draw_x, draw_y, (FcChar8*)state->filter, text_len);
    }
    Linux_DrawEditCursor(xd, &text_color, cursor_x, scroll, state->width);

    XftColorFree(dpy, visual, cmap, &bg_color);
    XftColorFree(dpy, visual, cmap, &text_color);
    XftColorFree(dpy, visual, cmap, &vim_text_color);
    XftColorFree(dpy, visual, cmap, &window_title_text_color);
    XftDrawDestroy(xd);
    Platform_DestroyFont(font);
}

void DrawPromptUserListBox(Display *dpy,Window* win,PromptState *state) {
      if (!state->has_listbox) return;
      ListBoxInfo *lb = &state->listbox;
      int screen = DefaultScreen(dpy);
      XftDraw* xd = Linux_CreateXftDraw(dpy, *win);
      FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);

      int y0 = 100;
      int itemH = FontSize + 14;
      {
        Visual *visClear = DefaultVisual(dpy, screen);
        Colormap cmapClear = DefaultColormap(dpy, screen);
        XftColor c0;
        Linux_XftAllocColor(dpy, visClear, cmapClear, rgb_to_hex(COLOR_BG), OpacityAlpha16(), &c0);
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

        XftColor bgc;
        Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(sel ? COLOR_SELECTED_TEXT : COLOR_BG), OpacityAlpha16(), &bgc);
        XftDrawRect(xd, &bgc, 0, y, state->width, itemH);

        XftColor fcoltext, fcolalternate, fcolrainbow;
        Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_TEXT), 0xFFFF, &fcoltext);
        Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_ALTERNATE), 0xFFFF, &fcolalternate);
        float ft = (float)idx / 32.0f;
        Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(GetRainbowColor(ft, 0.55f, GradientBrightness)), 0xFFFF, &fcolrainbow);

        XftColor *num_color = &fcoltext;
        XftColor *body_color = &fcoltext;
        XftColor *plain_color = &fcolrainbow;
        int text_x = 45;
        if (RainbowText) {
            num_color = body_color = plain_color = &fcolrainbow;
        } else if (AlternatingText) {
            if (idx % 2 == 0) {
                num_color = body_color = plain_color = &fcolalternate;
            }
        }

        DrawListBoxItemText(dpy, xd, vis, cmap, font, num_color, body_color, plain_color, lb, idx, y, text_x);

        XftColorFree(dpy, vis, cmap, &bgc);
        XftColorFree(dpy, vis, cmap, &fcoltext);
        XftColorFree(dpy, vis, cmap, &fcolalternate);
        XftColorFree(dpy, vis, cmap, &fcolrainbow);
      }
      XftDrawDestroy(xd);
      Platform_DestroyFont(font);
}

bool Linux_RenderPromptUser(Display *dpy, Window *win, PromptState *state) {
    if (g_render_callback != NULL) {
      g_render_callback(dpy,win,state);
    } else {
      if (state->has_listbox) {
          DrawPromptUserListBox(dpy,win,state);
      }
    }
    DrawPromptUserEditControl(dpy, win,state);
    XFlush(dpy);
    return true;
}
bool Linux_RenderSimplePromptUser(Display *dpy, Window *win, SimplePromptState *state) {
    DrawSimplePromptUserEditControl(dpy, win,state);
    XFlush(dpy);
    return true;
}
