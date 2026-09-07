#include "core/context.h"
#include "core/modal.h"
#include "core/modal.h"
#include "config.h"
#include "platform/x11/drawing.h"
#include "core/platform.h"
#include "helpers.h"
#include "core/common_ui.h"
#include "core/vim.h"
#include "bindings/bindings.h"
#include "bindings/vim_motions.h"
#include "bindings/vim_helpers.h"
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
#include "core/timer.h"


extern int pending_hotkey_vk;
extern int pending_hotkey_mod;
extern long pending_hotkey_timeMs;
static bool should_swallow_pending(int wParam, int mods) {
    WinTimer timer = TimerStart();
    if (pending_hotkey_vk == 0) { TimerEnd(timer, "subclasses_x11:should_swallow_pending"); return false; }
    struct timeval tv; gettimeofday(&tv,NULL);
    long now = tv.tv_sec*1000 + tv.tv_usec/1000;
    if (now - pending_hotkey_timeMs > 800) {
        pending_hotkey_vk = 0; pending_hotkey_mod = 0; TimerEnd(timer, "subclasses_x11:should_swallow_pending"); return false;
    }
    
    
    if (wParam == pending_hotkey_vk) {
        pending_hotkey_vk = 0; pending_hotkey_mod = 0;
        TimerEnd(timer, "subclasses_x11:should_swallow_pending");
        return true;
    }
    
    TimerEnd(timer, "subclasses_x11:should_swallow_pending");
    return false;
}

static int Linux_KeysymToWindowsVK(KeySym keysym) {
    WinTimer timer = TimerStart();
    if (keysym >= XK_a && keysym <= XK_z) { TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return 'A' + (keysym - XK_a); }
    if (keysym >= XK_A && keysym <= XK_Z) { TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return 'A' + (keysym - XK_A); }
    if (keysym >= XK_0 && keysym <= XK_9) { TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return '0' + (keysym - XK_0); }
    if (keysym >= XK_F1 && keysym <= XK_F12) { TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_F1 + (keysym - XK_F1); }

    switch (keysym) {
        case XK_Escape: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_ESCAPE;
        case XK_Return:
        case XK_KP_Enter: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_RETURN;
        case XK_Tab: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_TAB;
        case XK_BackSpace: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_BACK;
        case XK_Up: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_UP;
        case XK_Down: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_DOWN;
        case XK_Left: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_LEFT;
        case XK_Right: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_RIGHT;
        case XK_Delete: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_DELETE;
        case XK_Home: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_HOME;
        case XK_End: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_END;
        case XK_Page_Up: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_PRIOR;
        case XK_Page_Down: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_NEXT;
        case XK_space: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_SPACE;
        case XK_Control_L: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_LCONTROL;
        case XK_Control_R: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_RCONTROL;
        case XK_Shift_L: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_LSHIFT;
        case XK_Shift_R: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_RSHIFT;
        case XK_Alt_L: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_LMENU;
        case XK_Alt_R: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_RMENU;
        case XK_comma: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_COMMA;
        case XK_period: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_PERIOD;
        case XK_semicolon: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_1;
        case XK_slash: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_2;
        case XK_apostrophe: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_7;
        case XK_bracketleft: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_4;
        case XK_bracketright: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_6;
        case XK_grave: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_3;
        case XK_minus: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_MINUS;
        case XK_equal: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_PLUS;
        default: TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return 0;
    }
    TimerEnd(timer, "subclasses_x11:Linux_KeysymToWindowsVK");
}
extern bool g_Linux_CtrlHeld, g_Linux_AltHeld, g_Linux_ShiftHeld;

static void ReadModifiers(const XEvent* ev, bool* ctrl_pressed, bool* shift_pressed, bool* alt_pressed) {
    WinTimer timer = TimerStart();
    *ctrl_pressed = ((ev->xkey.state & ControlMask) != 0) || g_Linux_CtrlHeld;
    *shift_pressed = ((ev->xkey.state & ShiftMask) != 0) || g_Linux_ShiftHeld;
    *alt_pressed = ((ev->xkey.state & Mod1Mask) != 0) || g_Linux_AltHeld;
    TimerEnd(timer, "subclasses_x11:ReadModifiers");
}

static bool TryVimMotion(int wParam, bool ctrl_pressed, bool shift_pressed, bool alt_pressed) {
    WinTimer timer = TimerStart();
    int mod = HOTKEY_MOD_NONE;
    if (ctrl_pressed) mod |= HOTKEY_MOD_CTRL;
    if (shift_pressed) mod |= HOTKEY_MOD_SHIFT;
    if (alt_pressed) mod |= HOTKEY_MOD_ALT;
    MotionAction act; const char* seq; int typed;
    if (CheckVimMotion(wParam, mod, &act, &seq, &typed)) {
        ExecuteMotion(act, seq, typed);
        TimerEnd(timer, "subclasses_x11:TryVimMotion");
        return true;
    }
    TimerEnd(timer, "subclasses_x11:TryVimMotion");
    return false;
}

void EditBufferBackspace(char* buf, size_t* cursor) {
    WinTimer timer = TimerStart();
    size_t n = strlen(buf);
    if (*cursor > 0) {
        memmove(buf + *cursor - 1,
                buf + *cursor,
                n - *cursor + 1);
        (*cursor)--;
    }
    TimerEnd(timer, "subclasses_x11:EditBufferBackspace");
}

bool EditBufferDeleteChar(char* buf, size_t* cursor) {
    WinTimer timer = TimerStart();
    size_t n = strlen(buf);
    if (*cursor >= n) { TimerEnd(timer, "subclasses_x11:EditBufferDeleteChar"); return false; }
    memmove(buf + *cursor,
            buf + *cursor + 1,
            n - *cursor);
    TimerEnd(timer, "subclasses_x11:EditBufferDeleteChar");
    return true;
}

bool Linux_HandlePromptKey(const XEvent* ev,PromptState *state) {
    WinTimer timer = TimerStart();
    if (ev->type != KeyPress) { TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey"); return false; }

    int wParam = Linux_KeysymToWindowsVK(XLookupKeysym(&ev->xkey, 0));
    bool ctrl_pressed, shift_pressed, alt_pressed;
    ReadModifiers(ev, &ctrl_pressed, &shift_pressed, &alt_pressed);
    if (should_swallow_pending(wParam, (ctrl_pressed?HOTKEY_MOD_CTRL:0)|(shift_pressed?HOTKEY_MOD_SHIFT:0)|(alt_pressed?HOTKEY_MOD_ALT:0))) { TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey"); return true; }
    if (TryVimMotion(wParam, ctrl_pressed, shift_pressed, alt_pressed)) { TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey"); return true; }
    if (wParam == VK_BACK) {
        if (ctrl_pressed) {
            state->filter[0] = '\0';
            state->cursor = 0;
        } else {
            EditBufferBackspace(state->filter, &state->cursor);
        }
        if (FlintContext_Get()->prompt_callbacks.refresh) FlintContext_Get()->prompt_callbacks.refresh(state->filter, FlintContext_Get()->window.hListBox, state->has_listbox ? &state->listbox : NULL);
        TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
    }
    if (wParam == VK_ESCAPE) {
      if (IsInsertMode()) {
        SetMode(MODE_NORMAL);
        TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      } else {
        FlintContext_Get()->window.selection_complete = true;
        TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      }
    }
    
    if (wParam >= '0' && wParam <= '9') {
      if (ctrl_pressed) {
        HandleNumberShortcut(wParam, FlintContext_Get()->window.hListBox, FlintContext_Get()->window.g_ActiveEditControl,&state->listbox);
        TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      }
    }
    if (wParam == VK_RIGHT) {
      
      CursorForward(&state->cursor, strlen(state->filter));
      TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_LEFT) {
       CursorBack(&state->cursor, strlen(state->filter));
      TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_UP) {
      if (ctrl_pressed) {
        if (state) {
          VimContext ctx = Vim_FromState(state);
          SetCursorToStartOfText(&ctx);
        }
        TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      }
      if (state->has_listbox) UI_NavigateUp(&state->listbox, FlintContext_Get()->window.hListBox);
      TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_DOWN) {
      if (ctrl_pressed) {
        if (state) {
          VimContext ctx = Vim_FromState(state);
          SetCursorToEndOfText(&ctx);
        }
        TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      }
      if (state->has_listbox) UI_NavigateDown(&state->listbox, FlintContext_Get()->window.hListBox);
      TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_DELETE) {
      if (state && EditBufferDeleteChar(state->filter, &state->cursor)) {
        if (FlintContext_Get()->prompt_callbacks.refresh) FlintContext_Get()->prompt_callbacks.refresh(state->filter, FlintContext_Get()->window.hListBox, state->has_listbox ? &state->listbox : NULL);
      }
      TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_HOME) {
      if (state) {
        VimContext ctx = Vim_FromState(state);
        SetCursorToStartOfText(&ctx);
      }
      TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_END) {
      if (state) {
        VimContext ctx = Vim_FromState(state);
        SetCursorToEndOfText(&ctx);
      }
      TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_TAB) {
      if (FlintContext_Get()->prompt_callbacks.tab) FlintContext_Get()->prompt_callbacks.tab(&state->listbox);
      TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
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
        {
          FlintContext* _c = FlintContext_Get();
          bool should_close = false;
          if (_c->prompt_callbacks.ctrl_enter) should_close = _c->prompt_callbacks.ctrl_enter(sel, &state->listbox);
          else if (_c->prompt_callbacks.enter) should_close = _c->prompt_callbacks.enter(sel, &state->listbox);
          _c->window.selection_complete = should_close;
        }
        TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      } else {
        {
          FlintContext* _c2 = FlintContext_Get();
          bool should_close = true;
          if (_c2->prompt_callbacks.enter) should_close = _c2->prompt_callbacks.enter(sel, &state->listbox);
          _c2->window.selection_complete = should_close;
        }
        TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      }
    }
    // if (wParam == COPY_SELECTION_KEY) {
    //   if (shift_pressed || ctrl_pressed) {
    //     const char* sel = "";
    //     if (state->has_listbox && UI_IsListBoxSelected(&state->listbox)) sel = state->listbox.populated_cache[state->listbox.selected_index];
    //     else sel = state->filter;
    //     int type = shift_pressed ? 0 : 1;
    //     if (FlintContext_Get()->prompt_callbacks.copy) {
    //       int idx = UI_IsListBoxSelected(&state->listbox) ? state->listbox.selected_index : -1;
    //       FlintContext_Get()->prompt_callbacks.copy(sel, idx, type);
    //     } else {
    //       Platform_CopyToClipboard(sel);
    //     }
    //     return true;
    //   }
    // }

    TimerEnd(timer, "subclasses_x11:Linux_HandlePromptKey");
    return false;
}
bool Linux_HandleSimplePromptKey(const XEvent* ev,SimplePromptState *state) {
    WinTimer timer = TimerStart();
    if (ev->type != KeyPress) { TimerEnd(timer, "subclasses_x11:Linux_HandleSimplePromptKey"); return false; }

    int wParam = Linux_KeysymToWindowsVK(XLookupKeysym(&ev->xkey, 0));
    bool ctrl_pressed, shift_pressed, alt_pressed;
    ReadModifiers(ev, &ctrl_pressed, &shift_pressed, &alt_pressed);
    if (should_swallow_pending(wParam, (ctrl_pressed?HOTKEY_MOD_CTRL:0)|(shift_pressed?HOTKEY_MOD_SHIFT:0)|(alt_pressed?HOTKEY_MOD_ALT:0))) { TimerEnd(timer, "subclasses_x11:Linux_HandleSimplePromptKey"); return true; }
    if (TryVimMotion(wParam, ctrl_pressed, shift_pressed, alt_pressed)) { TimerEnd(timer, "subclasses_x11:Linux_HandleSimplePromptKey"); return true; }
    if (wParam == VK_BACK) {
        if (ctrl_pressed) {
            state->user_input[0] = '\0';
            state->cursor = 0;
        } else {
            EditBufferBackspace(state->user_input, &state->cursor);
        }
        TimerEnd(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
        return true;
    }
    if (wParam == VK_ESCAPE) {
      if (IsInsertMode()) {
        SetMode(MODE_NORMAL);
        TimerEnd(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
        return true;
      } else {
        FlintContext_Get()->window.selection_complete = true;
        TimerEnd(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
        return true;
      }
    }
    if (wParam == VK_RIGHT) {
      CursorForward(&state->cursor, strlen(state->user_input));
      TimerEnd(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
      return true;
    }
    if (wParam == VK_LEFT) {
       CursorBack(&state->cursor, strlen(state->user_input));
      TimerEnd(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
      return true;
    }
    if (wParam == VK_RETURN) {
      FlintContext_Get()->window.selection_complete = true;
      TimerEnd(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
      return true;
    }
    TimerEnd(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
    return false;
}

bool Linux_HandleEditPopupKey(const XEvent* ev, SimplePromptState* state, bool* confirmed, bool* cancelled) {
    WinTimer timer = TimerStart();
    *confirmed = false;
    *cancelled = false;
    if (ev->type != KeyPress) { TimerEnd(timer, "subclasses_x11:Linux_HandleEditPopupKey"); return false; }

    int wParam = Linux_KeysymToWindowsVK(XLookupKeysym(&ev->xkey, 0));
    bool ctrl_pressed, shift_pressed, alt_pressed;
    ReadModifiers(ev, &ctrl_pressed, &shift_pressed, &alt_pressed);
    if (should_swallow_pending(wParam, (ctrl_pressed?HOTKEY_MOD_CTRL:0)|(shift_pressed?HOTKEY_MOD_SHIFT:0)|(alt_pressed?HOTKEY_MOD_ALT:0))) { TimerEnd(timer, "subclasses_x11:Linux_HandleEditPopupKey"); return true; }
    if (TryVimMotion(wParam, ctrl_pressed, shift_pressed, alt_pressed)) { TimerEnd(timer, "subclasses_x11:Linux_HandleEditPopupKey"); return true; }
    if (wParam == VK_BACK) {
        EditBufferBackspace(state->user_input, &state->cursor);
        TimerEnd(timer, "subclasses_x11:Linux_HandleEditPopupKey");
        return true;
    }
    if (wParam == VK_ESCAPE) {
      *cancelled = true;
      TimerEnd(timer, "subclasses_x11:Linux_HandleEditPopupKey");
      return true;
    }
    if (wParam == VK_RETURN) {
      *confirmed = true;
      TimerEnd(timer, "subclasses_x11:Linux_HandleEditPopupKey");
      return true;
    }
    if (wParam == VK_RIGHT) {
      CursorForward(&state->cursor, strlen(state->user_input));
      TimerEnd(timer, "subclasses_x11:Linux_HandleEditPopupKey");
      return true;
    }
    if (wParam == VK_LEFT) {
       CursorBack(&state->cursor, strlen(state->user_input));
      TimerEnd(timer, "subclasses_x11:Linux_HandleEditPopupKey");
      return true;
    }
    if (wParam == VK_DELETE) {
      EditBufferDeleteChar(state->user_input, &state->cursor);
      TimerEnd(timer, "subclasses_x11:Linux_HandleEditPopupKey");
      return true;
    }
    if (wParam == VK_HOME) {
      state->cursor = 0;
      TimerEnd(timer, "subclasses_x11:Linux_HandleEditPopupKey");
      return true;
    }
    if (wParam == VK_END) {
      state->cursor = strlen(state->user_input);
      TimerEnd(timer, "subclasses_x11:Linux_HandleEditPopupKey");
      return true;
    }
    TimerEnd(timer, "subclasses_x11:Linux_HandleEditPopupKey");
    return false;
}


// --- OnDraw variants for double buffering (avoid creating XftDraw per call) ---
void DrawSimplePromptUserEditControlOnDraw(Display *dpy, XftDraw* xd, Visual* vis, Colormap cmap, SimplePromptState *state) {
    WinTimer timer = TimerStart();
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    XftFont* xf = FH_TO_XFT(font);

    XGlyphInfo ext;
    if (xf && state->display_message)
        XftTextExtentsUtf8(dpy, xf, (FcChar8*)state->display_message, strlen(state->display_message), &ext);
    else ext.xOff = 0;
    int text_width = ext.xOff;

    XftColor text_color, vim_text_color;
    Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_TEXT), 0xFFFF, &text_color);
    Linux_XftAllocColor(dpy, vis, cmap, VimModeTextColor(), 0xFFFF, &vim_text_color);

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

    XftColorFree(dpy, vis, cmap, &text_color);
    XftColorFree(dpy, vis, cmap, &vim_text_color);
    Platform_DestroyFont(font);
    TimerEnd(timer, "subclasses_x11:DrawSimplePromptUserEditControlOnDraw");
}

void DrawPromptUserEditControlOnDraw(Display *dpy, XftDraw* xd, Visual* vis, Colormap cmap, PromptState *state) {
    WinTimer timer = TimerStart();
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    XftFont* xf = FH_TO_XFT(font);

    unsigned long window_title_text_color_map;
    eval_window_title_color(state,&window_title_text_color_map);

    XftColor text_color, window_title_text_color, vim_text_color;
    Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_TEXT), 0xFFFF, &text_color);
    Linux_XftAllocColor(dpy, vis, cmap, window_title_text_color_map, 0xFFFF, &window_title_text_color);
    Linux_XftAllocColor(dpy, vis, cmap, VimModeTextColor(), 0xFFFF, &vim_text_color);

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

    XftColorFree(dpy, vis, cmap, &text_color);
    XftColorFree(dpy, vis, cmap, &vim_text_color);
    XftColorFree(dpy, vis, cmap, &window_title_text_color);
    Platform_DestroyFont(font);
    TimerEnd(timer, "subclasses_x11:DrawPromptUserEditControlOnDraw");
}

void DrawPromptUserListBoxOnDraw(Display *dpy, XftDraw* xd, Visual* vis, Colormap cmap, PromptState *state) {
      WinTimer timer = TimerStart();
      if (!state->has_listbox) { TimerEnd(timer, "subclasses_x11:DrawPromptUserListBoxOnDraw"); return; }
      ListBoxInfo *lb = &state->listbox;
      FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);

      int y0 = 100;
      int itemH = FontSize + 14;
      if (!lb->populated_count) {
        Platform_DestroyFont(font);
        TimerEnd(timer, "subclasses_x11:DrawPromptUserListBoxOnDraw");
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

      for (int i = 0; i < visible; i++) {
        int idx = lb->view_offset + i;
        int y = y0 + i * itemH;
        bool sel = (idx == lb->selected_index);

        if (sel) {
            XftColor bgc;
            Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_SELECTED_TEXT), OpacityAlpha16(), &bgc);
            XftDrawRect(xd, &bgc, 0, y, state->width, itemH);
            XftColorFree(dpy, vis, cmap, &bgc);
        } else {
            if (!EnableShaders || ShaderPreset == 0) {
                XftColor bgc;
                Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_BG), OpacityAlpha16(), &bgc);
                XftDrawRect(xd, &bgc, 0, y, state->width, itemH);
                XftColorFree(dpy, vis, cmap, &bgc);
            } else {
                // shader background already drawn by Linux_DrawBackground, keep transparent
            }
        }

        XftColor fcoltext, fcolalternate, fcolrainbow;
        Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_TEXT), 0xFFFF, &fcoltext);
        Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_ALTERNATE), 0xFFFF, &fcolalternate);
        float ft = (float)idx / GradientItems;
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

        XftColorFree(dpy, vis, cmap, &fcoltext);
        XftColorFree(dpy, vis, cmap, &fcolalternate);
        XftColorFree(dpy, vis, cmap, &fcolrainbow);
      }
      Platform_DestroyFont(font);
      TimerEnd(timer, "subclasses_x11:DrawPromptUserListBoxOnDraw");
}

// Wrappers keep old API (create XftDraw and delegate)
void DrawSimplePromptUserEditControl(Display *dpy, Window *win, SimplePromptState *state) {
    WinTimer timer = TimerStart();
    XWindowAttributes attrs;
    Visual* vis = NULL;
    Colormap cmap = 0;
    if (XGetWindowAttributes(dpy, *win, &attrs)) {
        vis = attrs.visual; cmap = attrs.colormap;
    } else {
        int screen = DefaultScreen(dpy);
        vis = DefaultVisual(dpy, screen);
        cmap = DefaultColormap(dpy, screen);
    }
    XftDraw* xd = XftDrawCreate(dpy, *win, vis, cmap);
    if (!xd) { TimerEnd(timer, "subclasses_x11:DrawSimplePromptUserEditControl"); return; }
    DrawSimplePromptUserEditControlOnDraw(dpy, xd, vis, cmap, state);
    XftDrawDestroy(xd);
    TimerEnd(timer, "subclasses_x11:DrawSimplePromptUserEditControl");
}

void DrawPromptUserEditControl(Display *dpy,Window* win,PromptState *state) {
    WinTimer timer = TimerStart();
    XWindowAttributes attrs;
    Visual* vis = NULL;
    Colormap cmap = 0;
    if (XGetWindowAttributes(dpy, *win, &attrs)) {
        vis = attrs.visual; cmap = attrs.colormap;
    } else {
        int screen = DefaultScreen(dpy);
        vis = DefaultVisual(dpy, screen);
        cmap = DefaultColormap(dpy, screen);
    }
    XftDraw* xd = XftDrawCreate(dpy, *win, vis, cmap);
    if (!xd) { TimerEnd(timer, "subclasses_x11:DrawPromptUserEditControl"); return; }
    DrawPromptUserEditControlOnDraw(dpy, xd, vis, cmap, state);
    XftDrawDestroy(xd);
    TimerEnd(timer, "subclasses_x11:DrawPromptUserEditControl");
}

void DrawPromptUserListBox(Display *dpy,Window* win,PromptState *state) {
      WinTimer timer = TimerStart();
      XWindowAttributes attrs;
      Visual* vis = NULL;
      Colormap cmap = 0;
      if (XGetWindowAttributes(dpy, *win, &attrs)) {
          vis = attrs.visual; cmap = attrs.colormap;
      } else {
          int screen = DefaultScreen(dpy);
          vis = DefaultVisual(dpy, screen);
          cmap = DefaultColormap(dpy, screen);
      }
      XftDraw* xd = XftDrawCreate(dpy, *win, vis, cmap);
      if (!xd) { TimerEnd(timer, "subclasses_x11:DrawPromptUserListBox"); return; }
      DrawPromptUserListBoxOnDraw(dpy, xd, vis, cmap, state);
      XftDrawDestroy(xd);
      TimerEnd(timer, "subclasses_x11:DrawPromptUserListBox");
}

bool Linux_RenderPromptUser(Display *dpy, Window *win, PromptState *state) {
    WinTimer timer = TimerStart();
    int w = state->width;
    int h = state->height;
    if (w <= 0 || h <= 0) { TimerEnd(timer, "subclasses_x11:Linux_RenderPromptUser"); return false; }
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, *win, &attrs)) {
        Linux_DrawBackground(dpy, *win, w, h);
        if (FlintContext_Get()->prompt_callbacks.render != NULL) {
          FlintContext_Get()->prompt_callbacks.render(dpy,win,state);
        } else {
          if (state->has_listbox) DrawPromptUserListBox(dpy,win,state);
        }
        DrawPromptUserEditControl(dpy, win,state);
        XFlush(dpy);
        TimerEnd(timer, "subclasses_x11:Linux_RenderPromptUser");
        return true;
    }
    Visual* vis = attrs.visual;
    int depth = attrs.depth;
    Colormap cmap = attrs.colormap;
    Pixmap dbl = XCreatePixmap(dpy, *win, w, h, depth);
    if (!dbl) {
        Linux_DrawBackground(dpy, *win, w, h);
        if (FlintContext_Get()->prompt_callbacks.render != NULL) {
          FlintContext_Get()->prompt_callbacks.render(dpy,win,state);
        } else {
          if (state->has_listbox) DrawPromptUserListBox(dpy,win,state);
        }
        DrawPromptUserEditControl(dpy, win,state);
        XFlush(dpy);
        TimerEnd(timer, "subclasses_x11:Linux_RenderPromptUser");
        return true;
    }
    XftDraw* xdDbl = XftDrawCreate(dpy, dbl, vis, cmap);
    if (!xdDbl) {
        XFreePixmap(dpy, dbl);
        Linux_DrawBackground(dpy, *win, w, h);
        if (FlintContext_Get()->prompt_callbacks.render != NULL) {
          FlintContext_Get()->prompt_callbacks.render(dpy,win,state);
        } else {
          if (state->has_listbox) DrawPromptUserListBox(dpy,win,state);
        }
        DrawPromptUserEditControl(dpy, win,state);
        XFlush(dpy);
        TimerEnd(timer, "subclasses_x11:Linux_RenderPromptUser");
        return true;
    }
    Linux_SetDblVisual(dpy, vis, cmap, depth);
    Pixmap bg = Linux_GetCachedBackground(dpy, *win, w, h);
    if (bg) {
        GC gc = XCreateGC(dpy, dbl, 0, NULL);
        XCopyArea(dpy, bg, dbl, gc, 0, 0, w, h, 0, 0);
        XFreeGC(dpy, gc);
    } else {
        Linux_DrawBackgroundToDrawable(dpy, dbl, w, h);
    }
    if (FlintContext_Get()->prompt_callbacks.render != NULL) {
        Window dbl_win = (Window)dbl;
        FlintContext_Get()->prompt_callbacks.render(dpy, &dbl_win, state);
        DrawPromptUserEditControlOnDraw(dpy, xdDbl, vis, cmap, state);
    } else {
        if (state->has_listbox) {
            DrawPromptUserListBoxOnDraw(dpy, xdDbl, vis, cmap, state);
        }
        DrawPromptUserEditControlOnDraw(dpy, xdDbl, vis, cmap, state);
    }
    Linux_ClearDblVisual();
    GC gc = XCreateGC(dpy, *win, 0, NULL);
    XCopyArea(dpy, dbl, *win, gc, 0, 0, w, h, 0, 0);
    XFreeGC(dpy, gc);
    XftDrawDestroy(xdDbl);
    XFreePixmap(dpy, dbl);
    XFlush(dpy);
    TimerEnd(timer, "subclasses_x11:Linux_RenderPromptUser");
    return true;
}
bool Linux_RenderSimplePromptUser(Display *dpy, Window *win, SimplePromptState *state) {
    WinTimer timer = TimerStart();
    int w = state->width;
    int h = state->height;
    if (w <= 0 || h <= 0) { TimerEnd(timer, "subclasses_x11:Linux_RenderSimplePromptUser"); return false; }
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, *win, &attrs)) {
        Linux_DrawBackground(dpy, *win, w, h);
        DrawSimplePromptUserEditControl(dpy, win,state);
        XFlush(dpy);
        TimerEnd(timer, "subclasses_x11:Linux_RenderSimplePromptUser");
        return true;
    }
    Visual* vis = attrs.visual;
    int depth = attrs.depth;
    Colormap cmap = attrs.colormap;
    Pixmap dbl = XCreatePixmap(dpy, *win, w, h, depth);
    if (!dbl) {
        Linux_DrawBackground(dpy, *win, w, h);
        DrawSimplePromptUserEditControl(dpy, win,state);
        XFlush(dpy);
        TimerEnd(timer, "subclasses_x11:Linux_RenderSimplePromptUser");
        return true;
    }
    XftDraw* xdDbl = XftDrawCreate(dpy, dbl, vis, cmap);
    if (!xdDbl) {
        XFreePixmap(dpy, dbl);
        Linux_DrawBackground(dpy, *win, w, h);
        DrawSimplePromptUserEditControl(dpy, win,state);
        XFlush(dpy);
        TimerEnd(timer, "subclasses_x11:Linux_RenderSimplePromptUser");
        return true;
    }
    Pixmap bg = Linux_GetCachedBackground(dpy, *win, w, h);
    if (bg) {
        GC gc = XCreateGC(dpy, dbl, 0, NULL);
        XCopyArea(dpy, bg, dbl, gc, 0, 0, w, h, 0, 0);
        XFreeGC(dpy, gc);
    } else {
        Linux_DrawBackgroundToDrawable(dpy, dbl, w, h);
    }
    DrawSimplePromptUserEditControlOnDraw(dpy, xdDbl, vis, cmap, state);
    GC gc = XCreateGC(dpy, *win, 0, NULL);
    XCopyArea(dpy, dbl, *win, gc, 0, 0, w, h, 0, 0);
    XFreeGC(dpy, gc);
    XftDrawDestroy(xdDbl);
    XFreePixmap(dpy, dbl);
    XFlush(dpy);
    TimerEnd(timer, "subclasses_x11:Linux_RenderSimplePromptUser");
    return true;
}

