#include "core/context.h"
#include "core/modal.h"
#include "core/modal.h"
#include "config.h"
#include "platform/x11/drawing.h"
#include "core/platform.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include "helpers/flag.h"
#include "core/common_ui.h"
#include "core/vim.h"
#include "platform/x11/input.h"
#include "bindings/bindings.h"
#include "bindings/vim_motions.h"
#include "bindings/vim_helpers.h"
#include "core/number_shortcut.h"
#include "core/history.h"
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
#include "platform/draw.h"


extern int pending_hotkey_vk;
extern int pending_hotkey_mod;
extern long pending_hotkey_timeMs;
static bool should_swallow_pending(int wParam, int mods) {
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

static int Linux_KeysymToWindowsVK(KeySym keysym) {
    PROFILE_START(timer);
    if (keysym >= XK_a && keysym <= XK_z) { PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return 'A' + (keysym - XK_a); }
    if (keysym >= XK_A && keysym <= XK_Z) { PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return 'A' + (keysym - XK_A); }
    if (keysym >= XK_0 && keysym <= XK_9) { PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return '0' + (keysym - XK_0); }
    if (keysym >= XK_F1 && keysym <= XK_F12) { PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_F1 + (keysym - XK_F1); }

    switch (keysym) {
        case XK_Escape: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_ESCAPE;
        case XK_Return:
        case XK_KP_Enter: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_RETURN;
        case XK_Tab: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_TAB;
        case XK_BackSpace: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_BACK;
        case XK_Up: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_UP;
        case XK_Down: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_DOWN;
        case XK_Left: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_LEFT;
        case XK_Right: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_RIGHT;
        case XK_Delete: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_DELETE;
        case XK_Home: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_HOME;
        case XK_End: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_END;
        case XK_Page_Up: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_PRIOR;
        case XK_Page_Down: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_NEXT;
        case XK_space: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_SPACE;
        case XK_Control_L: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_LCONTROL;
        case XK_Control_R: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_RCONTROL;
        case XK_Shift_L: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_LSHIFT;
        case XK_Shift_R: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_RSHIFT;
        case XK_Alt_L: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_LMENU;
        case XK_Alt_R: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_RMENU;
        case XK_comma: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_COMMA;
        case XK_period: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_PERIOD;
        case XK_semicolon: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_1;
        case XK_slash: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_2;
        case XK_apostrophe: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_7;
        case XK_bracketleft: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_4;
        case XK_bracketright: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_6;
        case XK_grave: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_3;
        case XK_minus: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_MINUS;
        case XK_equal: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return VK_OEM_PLUS;
        default: PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK"); return 0;
    }
    PROFILE_END_VAR(timer, "subclasses_x11:Linux_KeysymToWindowsVK");
}
extern bool g_x11_ctrl_held, g_x11_alt_held, g_x11_shift_held;

static void ReadModifiers(const XEvent* ev, bool* ctrl_pressed, bool* shift_pressed, bool* alt_pressed) {
    PROFILE_START(timer);
    *ctrl_pressed = ((ev->xkey.state & ControlMask) != 0) || g_x11_ctrl_held;
    *shift_pressed = ((ev->xkey.state & ShiftMask) != 0) || g_x11_shift_held;
    *alt_pressed = ((ev->xkey.state & Mod1Mask) != 0) || g_x11_alt_held;
    PROFILE_END_VAR(timer, "subclasses_x11:ReadModifiers");
}

static bool TryVimMotion(int wParam, bool ctrl_pressed, bool shift_pressed, bool alt_pressed) {
    PROFILE_START(timer);
    int mod = HOTKEY_MOD_NONE;
    if (ctrl_pressed) mod |= HOTKEY_MOD_CTRL;
    if (shift_pressed) mod |= HOTKEY_MOD_SHIFT;
    if (alt_pressed) mod |= HOTKEY_MOD_ALT;
    MotionAction act = MOTION_NONE; const char* seq = NULL; int typed = 0;
    if (CheckVimMotion(wParam, mod, &act, &seq, &typed)) {
        if (act != MOTION_NONE) ExecuteMotion(act, seq, typed);
        PROFILE_END_VAR(timer, "subclasses_x11:TryVimMotion");
        return true;
    }
    PROFILE_END_VAR(timer, "subclasses_x11:TryVimMotion");
    return false;
}

void EditBufferBackspace(char* buf, size_t* cursor) {
    PROFILE_START(timer);
    size_t n = strlen(buf);
    if (*cursor > 0) {
        memmove(buf + *cursor - 1,
                buf + *cursor,
                n - *cursor + 1);
        (*cursor)--;
    }
    PROFILE_END_VAR(timer, "subclasses_x11:EditBufferBackspace");
}

bool EditBufferDeleteChar(char* buf, size_t* cursor) {
    PROFILE_START(timer);
    size_t n = strlen(buf);
    if (*cursor >= n) { PROFILE_END_VAR(timer, "subclasses_x11:EditBufferDeleteChar"); return false; }
    memmove(buf + *cursor,
            buf + *cursor + 1,
            n - *cursor);
    PROFILE_END_VAR(timer, "subclasses_x11:EditBufferDeleteChar");
    return true;
}

void HandleCommandExecution(ListBoxInfo* lb, char* string) {
    if (!lb) return;
    int num = atoi(string);
    UI_SetSelection(lb, FlintContext_Get()->window.hListBox, num - 1);
    UI_LB_SetCaretIndex(FlintContext_Get()->window.hListBox, num, TRUE);
}

static void command_confirm(const char* new_text, void* user_data) {
    PromptState* state = (PromptState*)user_data;
    HandleCommandExecution(&state->listbox, new_text);
}

bool Linux_HandlePromptKey(const XEvent* ev,PromptState *state) {
    PROFILE_START(timer);
    if (ev->type != KeyPress) { PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey"); return false; }

    int wParam = Linux_KeysymToWindowsVK(XLookupKeysym(&ev->xkey, 0));
    bool ctrl_pressed, shift_pressed, alt_pressed;
    ReadModifiers(ev, &ctrl_pressed, &shift_pressed, &alt_pressed);
    if (should_swallow_pending(wParam, (ctrl_pressed?HOTKEY_MOD_CTRL:0)|(shift_pressed?HOTKEY_MOD_SHIFT:0)|(alt_pressed?HOTKEY_MOD_ALT:0))) { PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey"); return true; }
    if (TryVimMotion(wParam, ctrl_pressed, shift_pressed, alt_pressed)) { PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey"); return true; }
    if (wParam == VK_BACK) {
        if (ctrl_pressed) {
            state->filter[0] = '\0';
            state->cursor = 0;
        } else {
            EditBufferBackspace(state->filter, &state->cursor);
        }
        History_ResetCursor(FlintContext_Get()->window.current_feature);
        if (FlintContext_Get()->prompt_callbacks.refresh) FlintContext_Get()->prompt_callbacks.refresh(state->filter, FlintContext_Get()->window.hListBox, state->has_listbox ? &state->listbox : NULL);
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
    }
    if (wParam == VK_ESCAPE) {
      if (IsInsertMode()) {
        SetMode(MODE_NORMAL);
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      } else {
        FlintContext_Get()->window.selection_complete = true;
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      }
    }
    
    if (wParam >= '0' && wParam <= '9') {
      if (ctrl_pressed) {
        HandleNumberShortcut(wParam, FlintContext_Get()->window.hListBox, FlintContext_Get()->window.g_ActiveEditControl,&state->listbox);
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      }
    }
    if (wParam == VK_RIGHT) {
      
      CursorForward(&state->cursor, strlen(state->filter));
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_LEFT) {
       CursorBack(&state->cursor, strlen(state->filter));
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_UP) {
      if (ctrl_pressed) {
        if (state) {
          VimContext ctx = Vim_FromState(state);
          SetCursorToStartOfText(&ctx);
        }
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      }
      if (state->has_listbox) {
        if (state->listbox.selected_index == -1) {
          char hist[512];
          const char* feat = FlintContext_Get()->window.current_feature;
          if (feat && History_CycleUp(feat, hist, sizeof(hist))) {
            strncpy(state->filter, hist, sizeof(state->filter)-1);
            state->filter[sizeof(state->filter)-1]='\0';
            state->cursor = (int)strlen(state->filter);
            if (FlintContext_Get()->prompt_callbacks.refresh) FlintContext_Get()->prompt_callbacks.refresh(state->filter, FlintContext_Get()->window.hListBox, state->has_listbox ? &state->listbox : NULL);
            PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
            return true;
          }
        }
        UI_NavigateUp(&state->listbox, FlintContext_Get()->window.hListBox);
      }
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_DOWN) {
      if (ctrl_pressed) {
        if (state) {
          VimContext ctx = Vim_FromState(state);
          SetCursorToEndOfText(&ctx);
        }
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      }
      if (state->has_listbox) UI_NavigateDown(&state->listbox, FlintContext_Get()->window.hListBox);
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_DELETE) {
      if (state && EditBufferDeleteChar(state->filter, &state->cursor)) {
        History_ResetCursor(FlintContext_Get()->window.current_feature);
        if (FlintContext_Get()->prompt_callbacks.refresh) FlintContext_Get()->prompt_callbacks.refresh(state->filter, FlintContext_Get()->window.hListBox, state->has_listbox ? &state->listbox : NULL);
      }
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_HOME) {
      if (state) {
        VimContext ctx = Vim_FromState(state);
        SetCursorToStartOfText(&ctx);
      }
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_END) {
      if (state) {
        VimContext ctx = Vim_FromState(state);
        SetCursorToEndOfText(&ctx);
      }
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_TAB) {
      if (FlintContext_Get()->prompt_callbacks.tab) FlintContext_Get()->prompt_callbacks.tab(&state->listbox);
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
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
      const char* feat = FlintContext_Get()->window.current_feature;
      if (feat && sel && sel[0]) History_Add(feat, sel);
      if (is_ctrl) {
        {
          FlintContext* _c = FlintContext_Get();
          bool should_close = false;
          if (_c->prompt_callbacks.ctrl_enter) should_close = _c->prompt_callbacks.ctrl_enter(sel, &state->listbox);
          else if (_c->prompt_callbacks.enter) should_close = _c->prompt_callbacks.enter(sel, &state->listbox);
          _c->window.selection_complete = should_close;
        }
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      } else {
        {
          FlintContext* _c2 = FlintContext_Get();
          bool should_close = true;
          if (_c2->prompt_callbacks.enter) should_close = _c2->prompt_callbacks.enter(sel, &state->listbox);
          _c2->window.selection_complete = should_close;
        }
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      }
    }
    if (wParam == COPY_SELECTION_KEY) {
      if (shift_pressed || ctrl_pressed) {
        const char* sel = "";
        if (state->has_listbox && UI_IsListBoxSelected(&state->listbox)) sel = state->listbox.populated_cache[state->listbox.selected_index];
        else sel = state->filter;
        int type = shift_pressed ? 0 : 1;
        if (FlintContext_Get()->prompt_callbacks.copy) {
          int idx = UI_IsListBoxSelected(&state->listbox) ? state->listbox.selected_index : -1;
          FlintContext_Get()->prompt_callbacks.copy(sel, idx, type);
        } else {
          Platform_CopyToClipboard(sel);
        }
        return true;
      }
    }

    if (wParam == COMMAND_MODE_KEY) {
        WindowManager* wm = WindowManager_Get();
        Popup* popup = EditPopup_Create(wm,"", "Command Mode", 960, 540,command_confirm,state);
        WindowManager_PushPopup(wm, popup);
        return true;
      }

    PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
    return false;
}

bool Linux_HandleSimplePromptKey(const XEvent* ev,SimplePromptState *state) {
    PROFILE_START(timer);
    if (ev->type != KeyPress) { PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey"); return false; }

    int wParam = Linux_KeysymToWindowsVK(XLookupKeysym(&ev->xkey, 0));
    bool ctrl_pressed, shift_pressed, alt_pressed;
    ReadModifiers(ev, &ctrl_pressed, &shift_pressed, &alt_pressed);
    if (should_swallow_pending(wParam, (ctrl_pressed?HOTKEY_MOD_CTRL:0)|(shift_pressed?HOTKEY_MOD_SHIFT:0)|(alt_pressed?HOTKEY_MOD_ALT:0))) { PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey"); return true; }
    if (TryVimMotion(wParam, ctrl_pressed, shift_pressed, alt_pressed)) { PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey"); return true; }
    if (wParam == VK_BACK) {
        if (ctrl_pressed) {
            state->user_input[0] = '\0';
            state->cursor = 0;
        } else {
            EditBufferBackspace(state->user_input, &state->cursor);
        }
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
        return true;
    }
    if (wParam == VK_ESCAPE) {
      if (IsInsertMode()) {
        SetMode(MODE_NORMAL);
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
        return true;
      } else {
        FlintContext_Get()->window.selection_complete = true;
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
        return true;
      }
    }
    if (wParam == VK_RIGHT) {
      CursorForward(&state->cursor, strlen(state->user_input));
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
      return true;
    }
    if (wParam == VK_LEFT) {
       CursorBack(&state->cursor, strlen(state->user_input));
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
      return true;
    }
    if (wParam == VK_RETURN) {
      FlintContext_Get()->window.selection_complete = true;
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
      return true;
    }
    PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
    return false;
}

void DrawSimplePromptUserEditControlOnDraw(Display *dpy, XftDraw* xd, Visual* vis, Colormap cmap, SimplePromptState *state) {
    PROFILE_START(timer);
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    XftFont* xf = FH_TO_XFT(font);

    XGlyphInfo ext;
    if (xf && state->display_message)
        XftTextExtentsUtf8(dpy, xf, (FcChar8*)state->display_message, strlen(state->display_message), &ext);
    else ext.xOff = 0;
    int text_width = ext.xOff;

    XftColor text_color, vim_text_color, seperator_color;
    Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_TEXT), 0xFFFF, &text_color);
    Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_SEPERATOR), 0xFFFF, &seperator_color);
    Linux_XftAllocColor(dpy, vis, cmap, VimModeTextColor(), 0xFFFF, &vim_text_color);

    Platform_SetDrawContext(dpy, xd, vis, cmap);
    Platform_DrawTextRaw(5, state->height - 40, state->window_title, rgb_to_hex(COLOR_TEXT), font, "LEFT", 5);
    Platform_DrawTextRaw(5, state->height - 40, g_vim_mode, VimModeTextColor(), font, "RIGHT", 5);
    int centered_x = (state->width - text_width) / 2;
    Platform_DrawTextRaw(centered_x, 70, state->display_message, rgb_to_hex(COLOR_TEXT), font, "LEFT", centered_x);

    int cursor_x = 0;
    int text_len = strlen(state->user_input);
    int scroll = Linux_UpdateEditScrollOffset(dpy, xf, state->user_input, text_len, state->cursor, state->width, state->scroll_offset, &cursor_x);
    state->scroll_offset = scroll;
    int draw_x = 20 - scroll;
    int draw_y = 20 + (xf ? xf->ascent : FontSize);
    if (xf && text_len > 0) {
        XftDrawStringUtf8(xd, &text_color, xf, draw_x, draw_y, (FcChar8*)state->user_input, text_len);
    }
    Linux_DrawEditCursor(xd, &text_color, cursor_x, scroll, state->width);
    XftDrawRect(xd, &seperator_color,0,60,state->width,5);
    XftDrawRect(xd, &seperator_color,0,state->height - 60,state->width,5);

    XftColorFree(dpy, vis, cmap, &text_color);
    XftColorFree(dpy, vis, cmap, &seperator_color);
    XftColorFree(dpy, vis, cmap, &vim_text_color);
    Platform_DestroyFont(font);
    PROFILE_END_VAR(timer, "subclasses_x11:DrawSimplePromptUserEditControlOnDraw");
}

void DrawPromptUserEditControlOnDraw(Display *dpy, XftDraw* xd, Visual* vis, Colormap cmap, PromptState *state) {
    void Core_DrawEditBox(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap);
    Core_DrawEditBox(state, dpy, xd, vis, cmap);
}

void DrawPromptUserListBoxOnDraw(Display *dpy, XftDraw* xd, Visual* vis, Colormap cmap, PromptState *state) {
    void Core_DrawListBox(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap);
    Core_DrawListBox(state, dpy, xd, vis, cmap);
}

void DrawSimplePromptUserEditControl(Display *dpy, Window *win, SimplePromptState *state) {
    PROFILE_START(timer);
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
    if (!xd) { PROFILE_END_VAR(timer, "subclasses_x11:DrawSimplePromptUserEditControl"); return; }
    DrawSimplePromptUserEditControlOnDraw(dpy, xd, vis, cmap, state);
    XftDrawDestroy(xd);
    PROFILE_END_VAR(timer, "subclasses_x11:DrawSimplePromptUserEditControl");
}

void DrawPromptUserEditControl(Display *dpy,Window* win,PromptState *state) {
    PROFILE_START(timer);
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
    if (!xd) { PROFILE_END_VAR(timer, "subclasses_x11:DrawPromptUserEditControl"); return; }
    DrawPromptUserEditControlOnDraw(dpy, xd, vis, cmap, state);
    XftDrawDestroy(xd);
    PROFILE_END_VAR(timer, "subclasses_x11:DrawPromptUserEditControl");
}

void DrawPromptUserListBox(Display *dpy,Window* win,PromptState *state) {
      PROFILE_START(timer);
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
      if (!xd) { PROFILE_END_VAR(timer, "subclasses_x11:DrawPromptUserListBox"); return; }
      DrawPromptUserListBoxOnDraw(dpy, xd, vis, cmap, state);
      XftDrawDestroy(xd);
      PROFILE_END_VAR(timer, "subclasses_x11:DrawPromptUserListBox");
}

bool Linux_RenderPromptUser(Display *dpy, Window *win, PromptState *state) {
    Platform_SetPreRenderState(state);
    if (!fl_prerender()) return true;
    int w = state->width;
    int h = state->height;
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
    Platform_SetDrawContext(dpy, xdDbl, vis, cmap);
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
    Platform_SetDrawContext(NULL,NULL,NULL,0);
    Linux_ClearDblVisual();
    GC gc = XCreateGC(dpy, *win, 0, NULL);
    XCopyArea(dpy, dbl, *win, gc, 0, 0, w, h, 0, 0);
    XFreeGC(dpy, gc);
    XftDrawDestroy(xdDbl);
    XFreePixmap(dpy, dbl);
    XFlush(dpy);
    return true;
}
bool Linux_RenderSimplePromptUser(Display *dpy, Window *win, SimplePromptState *state) {
    int w = state->width;
    int h = state->height;
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(dpy, *win, &attrs)) {
        Linux_DrawBackground(dpy, *win, w, h);
        DrawSimplePromptUserEditControl(dpy, win,state);
        XFlush(dpy);
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
        return true;
    }
    XftDraw* xdDbl = XftDrawCreate(dpy, dbl, vis, cmap);
    if (!xdDbl) {
        XFreePixmap(dpy, dbl);
        Linux_DrawBackground(dpy, *win, w, h);
        DrawSimplePromptUserEditControl(dpy, win,state);
        XFlush(dpy);
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
    return true;
}
