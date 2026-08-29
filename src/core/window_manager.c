#include "core/window_manager.h"
#include "platform/platform.h"
#include "core/common_ui.h"
#include "core/modal.h"
#include "core/feature_data.h"
#include "core/vim.h"
#include "bindings/vim_helpers.h"
#include "bindings/vim_motions.h"
#include "bindings/bindings.h"
#include "core/number_shortcut.h"
#include "helpers.h"
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <unistd.h>
#include "x11_display.h"
extern int WindowManager_RunLinux(WindowManager* wm);
extern void WindowManager_ProcessEventsLinux(WindowManager* wm);
extern void WindowManager_QuitLinux(WindowManager* wm);
extern void Linux_ProcessXEvent(Display* dpy, XEvent* ev, WindowManager* wm);
extern char output_buffer[256];
extern void EditBufferBackspace(char* buf, size_t* cursor);
extern bool EditBufferDeleteChar(char* buf, size_t* cursor);
#else
#include <windows.h>
extern int WindowManager_RunWin32(WindowManager* wm);
extern void WindowManager_ProcessEventsWin32(WindowManager* wm);
extern void WindowManager_QuitWin32(WindowManager* wm);
extern char output_buffer[256];
#endif

static WindowManager* g_window_manager = NULL;

WindowManager* WindowManager_Create(void) {
    WindowManager* wm = calloc(1, sizeof(WindowManager));
    if (!wm) return NULL;
    wm->popup_capacity = 8;
    wm->popups = calloc(wm->popup_capacity, sizeof(Popup*));
    if (!wm->popups) {
        free(wm);
        return NULL;
    }
    
    #if !defined(_WIN32)
    wm->display = get_display();
    if (!wm->display) {
        free(wm->popups);
        free(wm);
        return NULL;
    }
    #endif
    
    g_window_manager = wm;
    return wm;
}

void WindowManager_Destroy(WindowManager* wm) {
    if (!wm) return;
    
    // Destroy all popups
    for (int i = 0; i < wm->popup_count; i++) {
        if (wm->popups[i] && wm->popups[i]->vtable && wm->popups[i]->vtable->destroy) {
            wm->popups[i]->vtable->destroy(wm->popups[i]);
        }
    }
    
    free(wm->popups);
    
    #if !defined(_WIN32)
    // Don't close display here - it's shared
    #endif
    
    if (g_window_manager == wm) g_window_manager = NULL;
    free(wm);
}

WindowManager* WindowManager_Get(void) {
    return g_window_manager;
}

int WindowManager_Run(WindowManager* wm) {
    if (!wm) return -1;
    
    #if defined(_WIN32)
    return WindowManager_RunWin32(wm);
    #else
    return WindowManager_RunLinux(wm);
    #endif
}

void WindowManager_Quit(WindowManager* wm) {
    if (!wm) return;
    
    #if defined(_WIN32)
    WindowManager_QuitWin32(wm);
    #else
    WindowManager_QuitLinux(wm);
    #endif
}

Popup* WindowManager_PushPopup(WindowManager* wm, Popup* popup) {
    if (!wm || !popup) return NULL;
    
    if (wm->popup_count >= wm->popup_capacity) {
        int new_cap = wm->popup_capacity * 2;
        Popup** new_popups = realloc(wm->popups, new_cap * sizeof(Popup*));
        if (!new_popups) return NULL;
        wm->popups = new_popups;
        wm->popup_capacity = new_cap;
    }
    
    popup->manager = wm;
    popup->visible = true;
    wm->popups[wm->popup_count++] = popup;
    
    if (popup->vtable && popup->vtable->on_show) {
        popup->vtable->on_show(popup);
    }
    
    return popup;
}

void WindowManager_PopPopup(WindowManager* wm) {
    if (!wm || wm->popup_count == 0) return;
    
    Popup* popup = wm->popups[--wm->popup_count];
    if (popup->vtable && popup->vtable->on_hide) {
        popup->vtable->on_hide(popup);
    }
    popup->visible = false;
    popup->manager = NULL;
}

Popup* WindowManager_GetTopPopup(WindowManager* wm) {
    if (!wm || wm->popup_count == 0) return NULL;
    return wm->popups[wm->popup_count - 1];
}

void WindowManager_ProcessEvents(WindowManager* wm) {
    #if defined(_WIN32)
    WindowManager_ProcessEventsWin32(wm);
    #else
    WindowManager_ProcessEventsLinux(wm);
    #endif
}

char* WindowManager_ShowPrompt(WindowManager* wm, const PromptSpec* spec) {
    if (!wm || !spec) return NULL;
    
    Popup* popup = PromptPopup_Create(wm, spec);
    if (!popup) return NULL;
    
    #if defined(_WIN32)
    MSG msg;
    while (wm->running && wm->popup_count > 0 && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    #else
    Display* dpy = wm->display;
    XEvent ev;
    while (wm->running && wm->popup_count > 0) {
        if (XPending(dpy) > 0) {
            XNextEvent(dpy, &ev);
            Linux_ProcessXEvent(dpy, &ev, wm);
        } else {
            usleep(1000);
        }
    }
    #endif
    
    if (popup->manager == NULL) {
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    } else {
        WindowManager_PopPopup(wm);
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    }
    
    return output_buffer[0] ? output_buffer : NULL;
}

char* WindowManager_ShowSimplePrompt(WindowManager* wm, const SimplePromptSpec* spec) {
    if (!wm || !spec) return NULL;
    
    Popup* popup = SimplePromptPopup_Create(wm, spec);
    if (!popup) return NULL;
    
    #if defined(_WIN32)
    MSG msg;
    while (wm->running && wm->popup_count > 0 && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    #else
    Display* dpy = wm->display;
    XEvent ev;
    while (wm->running && wm->popup_count > 0) {
        if (XPending(dpy) > 0) {
            XNextEvent(dpy, &ev);
            Linux_ProcessXEvent(dpy, &ev, wm);
        } else {
            usleep(1000);
        }
    }
    #endif
    
    if (popup->manager == NULL) {
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    } else {
        WindowManager_PopPopup(wm);
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    }
    
    return output_buffer[0] ? output_buffer : NULL;
}

static bool prompt_should_swallow_pending(int wParam, int mods);
static bool prompt_try_vim_motion(int wParam, bool ctrl, bool shift, bool alt);

// ============ Edit Popup Implementation ============

typedef WMEditPopupData EditPopupDataInternal;

static void EditPopup_OnShow(Popup* self) {
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
    data->saved_mode = g_current_mode;
    SetMode(MODE_INSERT);
    extern bool g_Linux_CtrlHeld, g_Linux_AltHeld, g_Linux_ShiftHeld;
    extern bool g_linux_ctrl_held, g_linux_alt_held, g_linux_shift_held;
    g_Linux_CtrlHeld = g_Linux_AltHeld = g_Linux_ShiftHeld = false;
    g_linux_ctrl_held = g_linux_alt_held = g_linux_shift_held = false;
    #if !defined(_WIN32)
    Display* dpy = ((WindowManager*)self->manager)->display;
    XMapWindow(dpy, self->handle.xid);
    XSetInputFocus(dpy, self->handle.xid, RevertToParent, CurrentTime);
    XFlush(dpy);
    #endif
}

static void EditPopup_OnHide(Popup* self) {
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
    SetMode(data->saved_mode);
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) { XUnmapWindow(dpy, self->handle.xid); XFlush(dpy); }
    #endif
    // restore focus to new top popup
    WindowManager* wm = WindowManager_Get();
    if (wm) {
        Popup* top = WindowManager_GetTopPopup(wm);
        if (top) {
            Display* d2 = get_display();
            if (d2) { 
                XSetInputFocus(d2, top->handle.xid, RevertToParent, CurrentTime); 
                XFlush(d2); 
                if (top->vtable && top->vtable->on_render) top->vtable->on_render(top);
                XFlush(d2);
            }
        }
    }
}

static void EditPopup_OnKey(Popup* self, int vk_code, bool pressed, int modifiers) {
    if (!pressed) return;
    
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
    int vk_upper = (vk_code >= 'a' && vk_code <= 'z') ? vk_code - 32 : vk_code;
    bool ctrl = modifiers & HOTKEY_MOD_CTRL;
    bool shift = modifiers & HOTKEY_MOD_SHIFT;
    bool alt = modifiers & HOTKEY_MOD_ALT;
    if (prompt_should_swallow_pending(vk_upper, modifiers)) return;
    if (prompt_try_vim_motion(vk_upper, ctrl, shift, alt)) {
        if (selection_complete) { data->confirmed=false; WindowManager_PopPopup(self->manager); }
        return;
    }
    if (vk_code == VK_ESCAPE) {
        if (IsInsertMode()) {
            SetMode(MODE_NORMAL);
            if (self->vtable && self->vtable->on_render) self->vtable->on_render(self);
        } else {
            data->confirmed = false;
            WindowManager_PopPopup(self->manager);
        }
        return;
    }
    
    if (vk_code == VK_RETURN && !(modifiers & HOTKEY_MOD_SHIFT)) {
        data->confirmed = true;
        if (data->on_confirm) {
            data->on_confirm(data->text, data->user_data);
        }
        WindowManager_PopPopup(self->manager);
        return;
    }
    
    // Handle text input - only in INSERT
    if (vk_code == VK_BACK) {
        if (data->cursor > 0) {
            memmove(data->text + data->cursor - 1, data->text + data->cursor, 
                    strlen(data->text) - data->cursor + 1);
            data->cursor--;
        }
    } else if (vk_code >= 32 && vk_code <= 126) { // Printable
        if (!IsInsertMode()) return;
        if (data->cursor < data->text_capacity - 1) {
            memmove(data->text + data->cursor + 1, data->text + data->cursor,
                    strlen(data->text) - data->cursor + 1);
            data->text[data->cursor] = (char)vk_code;
            data->cursor++;
        }
    } else if (vk_code == VK_LEFT) {
        if (data->cursor > 0) data->cursor--;
    } else if (vk_code == VK_RIGHT) {
        if (data->cursor < (int)strlen(data->text)) data->cursor++;
    } else if (vk_code == VK_HOME) {
        data->cursor = 0;
    } else if (vk_code == VK_END) {
        data->cursor = strlen(data->text);
    }
    
    if (self->vtable && self->vtable->on_render) {
        self->vtable->on_render(self);
    }
}

static void EditPopup_OnRender(Popup* self) {
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
#if !defined(_WIN32)
    Display* dpy = get_display();
    if (!dpy) return;
    XftDraw* xd = Linux_CreateXftDraw(dpy, self->handle.xid);
    if (!xd) return;
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    XftFont* xf = FH_TO_XFT(font);
    int screen = DefaultScreen(dpy);
    Visual* visual = DefaultVisual(dpy, screen);
    Colormap cmap = DefaultColormap(dpy, screen);
    XftColor bg, fg, title_col, vim_col;
    Linux_XftAllocColor(dpy, visual, cmap, rgb_to_hex(COLOR_BG), 0xFFFF, &bg);
    Linux_XftAllocColor(dpy, visual, cmap, rgb_to_hex(COLOR_TEXT), 0xFFFF, &fg);
    Linux_XftAllocColor(dpy, visual, cmap, rgb_to_hex(COLOR_TEXT), 0xFFFF, &title_col);
    Linux_XftAllocColor(dpy, visual, cmap, VimModeTextColor(), 0xFFFF, &vim_col);
    // clear
    XftDrawRect(xd, &bg, 0, 0, data->w, data->h);
    // vim mode on left, title on right
    if (xf) {
        draw_wrapped_text(dpy, xd, &vim_col, font, g_vim_mode, 5, "LEFT", 5);
    }
    if (data->title && xf) {
        draw_wrapped_text(dpy, xd, &title_col, font, data->title, 5, "RIGHT", 5);
    }
    // text with scroll
    int cursor_x = 0;
    int text_len = data->text ? strlen(data->text) : 0;
    int scroll = 0;
    if (xf) {
        scroll = Linux_UpdateEditScrollOffset(dpy, xf, data->text ? data->text : "", text_len, data->cursor, data->w, 0, &cursor_x);
    }
    if (xf && text_len > 0) {
        XGlyphInfo ext;
        XftTextExtentsUtf8(dpy, xf, (FcChar8*)data->text, text_len, &ext);
        int draw_x = 10 - scroll;
        int draw_y = 40 + xf->ascent;
        XftDrawStringUtf8(xd, &fg, xf, draw_x, draw_y, (FcChar8*)data->text, text_len);
    }
    // cursor
    if (xf) {
        Linux_DrawEditCursor(xd, &fg, cursor_x, scroll, data->w);
    }
    XftColorFree(dpy, visual, cmap, &bg);
    XftColorFree(dpy, visual, cmap, &fg);
    XftColorFree(dpy, visual, cmap, &title_col);
    XftColorFree(dpy, visual, cmap, &vim_col);
    XftDrawDestroy(xd);
    Platform_DestroyFont(font);
    XFlush(dpy);
#endif
}

static void EditPopup_Destroy(Popup* self) {
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
    free(data->text);
    free(data->title);
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) XDestroyWindow(dpy, self->handle.xid);
    #endif
    free(data);
}

static const PopupVTable g_edit_popup_vtable = {
    .on_show = EditPopup_OnShow,
    .on_hide = EditPopup_OnHide,
    .on_key = EditPopup_OnKey,
    .on_render = EditPopup_OnRender,
    .destroy = EditPopup_Destroy
};

Popup* EditPopup_Create(WindowManager* wm, const char* initial_text,
                        const char* title, int x, int y, int w, int h,
                        EditPopupConfirmCallback on_confirm, void* user_data) {
    EditPopupDataInternal* data = calloc(1, sizeof(EditPopupDataInternal));
    if (!data) return NULL;
    
    data->base.vtable = &g_edit_popup_vtable;
    data->base.manager = wm;
    data->base.type = POPUP_EDIT;
    data->title = title ? strdup(title) : strdup("Edit");
    data->text_capacity = 1024;
    data->text = calloc(data->text_capacity, 1);
    if (initial_text) {
        strncpy(data->text, initial_text, data->text_capacity - 1);
        data->cursor = strlen(data->text);
    }
    data->on_confirm = on_confirm;
    data->user_data = user_data;
    data->x = x; data->y = y; data->w = w; data->h = h;
    data->confirmed = false;
    
    #if !defined(_WIN32)
    Display* dpy = wm->display;
    data->base.handle.xid = Linux_CreateEditPopupWindow(dpy, x, y, w, h);
    #endif
    
    return WindowManager_PushPopup(wm, (Popup*)data);
}

// ============ Message Popup ============

typedef struct {
    Popup base;
    char* message;
    int timeout_ms;
    uint64_t start_time;
} MessagePopupData;

static void MessagePopup_OnShow(Popup* self) {
    #if !defined(_WIN32)
    Display* dpy = ((WindowManager*)self->manager)->display;
    XMapWindow(dpy, self->handle.xid);
    XFlush(dpy);
    #endif
}

static void MessagePopup_Destroy(Popup* self) {
    MessagePopupData* data = (MessagePopupData*)self;
    free(data->message);
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) XDestroyWindow(dpy, self->handle.xid);
    #endif
    free(data);
}

static const PopupVTable g_message_popup_vtable = {
    .on_show = MessagePopup_OnShow,
    .on_hide = NULL,
    .on_key = NULL,
    .on_render = NULL,
    .destroy = MessagePopup_Destroy
};

Popup* MessagePopup_Create(WindowManager* wm, const char* message, int timeout_ms) {
    MessagePopupData* data = calloc(1, sizeof(MessagePopupData));
    if (!data) return NULL;
    
    data->base.vtable = &g_message_popup_vtable;
    data->base.manager = wm;
    data->message = strdup(message);
    data->timeout_ms = timeout_ms;
    
    #if !defined(_WIN32)
    Display* dpy = wm->display;
    // Create simple message window - reuse existing platform function
    // For now just create a basic window
    data->base.handle.xid = Linux_CreateDisplayMessageWindow(dpy, 0, 0, 400, 100);
    #endif
    
    return WindowManager_PushPopup(wm, (Popup*)data);
}

// ============ Prompt Popup (list-based) ============

typedef struct {
    Popup base;
    PromptSpec spec;
    PromptState* state;
    int x, y, w, h;
} PromptPopupData;

static void PromptPopup_OnShow(Popup* self) {
    PromptPopupData* data = (PromptPopupData*)self;
    WindowManager* wm = (WindowManager*)self->manager;
    selection_complete = false;
    output_buffer[0] = '\0';
    // clear stuck modifiers from Alt+T hotkey
    extern bool g_Linux_CtrlHeld, g_Linux_AltHeld, g_Linux_ShiftHeld;
    extern bool g_linux_ctrl_held, g_linux_alt_held, g_linux_shift_held;
    g_Linux_CtrlHeld = g_Linux_AltHeld = g_Linux_ShiftHeld = false;
    g_linux_ctrl_held = g_linux_alt_held = g_linux_shift_held = false;
    
    #if !defined(_WIN32)
    Display* dpy = wm->display;
    data->base.handle.xid = Linux_CreatePromptWindow(dpy, data->x, data->y, data->w, data->h);
    XSelectInput(dpy, data->base.handle.xid, KeyPressMask | ExposureMask | FocusChangeMask);
    XMapWindow(dpy, data->base.handle.xid);
    XSetInputFocus(dpy, data->base.handle.xid, RevertToParent, CurrentTime);
    XFlush(dpy);
    #endif
    
    data->state = calloc(1, sizeof(PromptState));
    if (!data->state) return;
    
    data->state->items = (char**)data->spec.items;
    data->state->item_count = data->spec.count;
    data->state->cursor = 0;
    data->state->width = data->w;
    data->state->height = data->h;
    const char* p = strchr(data->spec.title, ' ');
    data->state->window_title = (char*)(p ? p+1 : data->spec.title);
    data->state->filter[0] = '\0';
    data->state->has_listbox = (data->spec.count >= 0);
    ListBoxInfo_Init(&data->state->listbox);
    data->state->listbox.selected_index = -1;
    // Handle vim mode persistence per config
    extern EditorMode g_last_mode;
    extern int last_index;
    if (ModePersistsThroughFeatures) {
        SetModeWithListBox(g_last_mode, &data->state->listbox);
        if (RememberLastIndex && last_index >= 0 && data->state->has_listbox) {
            data->state->listbox.selected_index = last_index;
        }
    } else {
        SetModeWithListBox(MODE_INSERT, &data->state->listbox);
    }
    self->user_data = data->state;
    
    g_refresh_callback = data->spec.refresh;
    g_enter_callback = data->spec.enter;
    g_ctrl_enter_callback = data->spec.ctrl_enter;
    g_tab_callback = data->spec.tab;
    g_copy_callback = data->spec.copy;
    g_render_callback = data->spec.render;
    g_edit_callback = data->spec.edit;
    g_filter_callback = data->spec.filter;
    g_help_callback = data->spec.help;
    g_title = data->spec.title;
    g_current_feature = (char*)(p ? p+1 : data->spec.title);
    SetCurrentFeatureName(g_current_feature);
    if (g_refresh_callback) g_refresh_callback(data->state->filter, hListBox, data->state->has_listbox ? &data->state->listbox : NULL);
}

static bool prompt_should_swallow_pending(int wParam, int mods) {
    extern int pending_hotkey_vk;
    extern int pending_hotkey_mod;
    extern long pending_hotkey_timeMs;
    if (pending_hotkey_vk == 0) return false;
    struct timeval tv; gettimeofday(&tv,NULL);
    long now = tv.tv_sec*1000 + tv.tv_usec/1000;
    if (now - pending_hotkey_timeMs > 800) { pending_hotkey_vk=0; pending_hotkey_mod=0; return false; }
    if (wParam == pending_hotkey_vk) { pending_hotkey_vk=0; pending_hotkey_mod=0; return true; }
    return false;
}

static bool prompt_try_vim_motion(int wParam, bool ctrl, bool shift, bool alt) {
    int mod = HOTKEY_MOD_NONE;
    if (ctrl) mod |= HOTKEY_MOD_CTRL;
    if (shift) mod |= HOTKEY_MOD_SHIFT;
    if (alt) mod |= HOTKEY_MOD_ALT;
    MotionAction act; const char* seq; int typed;
    if (CheckVimMotion(wParam, mod, &act, &seq, &typed)) {
        ExecuteMotion(act, seq, typed);
        return true;
    }
    return false;
}

static void PromptPopup_OnKey(Popup* self, int vk_code, bool pressed, int modifiers) {
    if (!pressed) return;
    PromptPopupData* data = (PromptPopupData*)self;
    if (!data->state) return;
    
    #if !defined(_WIN32)
    bool ctrl_pressed = modifiers & HOTKEY_MOD_CTRL;
    bool shift_pressed = modifiers & HOTKEY_MOD_SHIFT;
    bool alt_pressed = modifiers & HOTKEY_MOD_ALT;
    int vk_upper = (vk_code >= 'a' && vk_code <= 'z') ? vk_code - 32 : vk_code;
    
    if (prompt_should_swallow_pending(vk_upper, modifiers)) return;
    if (prompt_try_vim_motion(vk_upper, ctrl_pressed, shift_pressed, alt_pressed)) {
        if (selection_complete) WindowManager_PopPopup(self->manager);
        return;
    }
    
    if (vk_code == VK_ESCAPE) {
        if (IsInsertMode()) {
            SetMode(MODE_NORMAL);
            if (self->vtable && self->vtable->on_render) self->vtable->on_render(self);
        } else {
            selection_complete = true;
            WindowManager_PopPopup(self->manager);
        }
        return;
    }
    
    if (vk_code == VK_BACK) {
        if (ctrl_pressed) {
            data->state->filter[0] = '\0';
            data->state->cursor = 0;
        } else {
            EditBufferBackspace(data->state->filter, &data->state->cursor);
        }
        if (g_refresh_callback) g_refresh_callback(data->state->filter, hListBox, data->state->has_listbox ? &data->state->listbox : NULL);
        return;
    }
    
    if (vk_code >= '0' && vk_code <= '9') {
        if (ctrl_pressed) {
            HandleNumberShortcut(vk_code, hListBox, g_ActiveEditControl, data->state->has_listbox ? &data->state->listbox : NULL);
            return;
        }
    }
    
    if (vk_code == VK_RIGHT) {
        CursorForward(&data->state->cursor, strlen(data->state->filter));
        return;
    }
    if (vk_code == VK_LEFT) {
        CursorBack(&data->state->cursor, strlen(data->state->filter));
        return;
    }
    if (vk_code == VK_UP) {
        if (ctrl_pressed) {
            VimContext ctx = Vim_FromState(data->state);
            SetCursorToStartOfText(&ctx);
        } else if (data->state->has_listbox) {
            UI_NavigateUp(&data->state->listbox, hListBox);
        }
        return;
    }
    if (vk_code == VK_DOWN) {
        if (ctrl_pressed) {
            VimContext ctx = Vim_FromState(data->state);
            SetCursorToEndOfText(&ctx);
        } else if (data->state->has_listbox) {
            UI_NavigateDown(&data->state->listbox, hListBox);
        }
        return;
    }
    if (vk_code == VK_DELETE) {
        if (EditBufferDeleteChar(data->state->filter, &data->state->cursor)) {
            if (g_refresh_callback) g_refresh_callback(data->state->filter, hListBox, data->state->has_listbox ? &data->state->listbox : NULL);
        }
        return;
    }
    if (vk_code == VK_HOME) {
        VimContext ctx = Vim_FromState(data->state);
        SetCursorToStartOfText(&ctx);
        return;
    }
    if (vk_code == VK_END) {
        VimContext ctx = Vim_FromState(data->state);
        SetCursorToEndOfText(&ctx);
        return;
    }
    if (vk_code == VK_TAB) {
        if (g_tab_callback) g_tab_callback(&data->state->listbox);
        return;
    }
    if (vk_code == VK_RETURN) {
        bool is_ctrl = ctrl_pressed;
        const char* sel = NULL;
        char filter_copy[sizeof(data->state->filter)];
        if (data->state->has_listbox && UI_IsListBoxSelected(&data->state->listbox)) {
            sel = ListBoxInfo_GetSelectedText(&data->state->listbox);
        } else {
            strncpy(filter_copy, data->state->filter, sizeof(filter_copy) - 1);
            filter_copy[sizeof(filter_copy) - 1] = '\0';
            sel = filter_copy;
        }
        if (is_ctrl) {
            if (g_ctrl_enter_callback) g_ctrl_enter_callback(sel, data->state->has_listbox ? &data->state->listbox : NULL);
        } else {
            if (g_enter_callback) g_enter_callback(sel, data->state->has_listbox ? &data->state->listbox : NULL) ; selection_complete = true;
        }
        if (selection_complete) WindowManager_PopPopup(self->manager);
        return;
    }
    
    if (vk_code >= 32 && vk_code <= 126) {
        if (IsInsertMode()) {
            int len = 1;
            char buf[2] = { (char)vk_code, '\0' };
            size_t n = strlen(data->state->filter);
            if (n + len < sizeof(data->state->filter) - 1) {
                if (data->state->cursor < 0) data->state->cursor = 0;
                if ((size_t)data->state->cursor > n) data->state->cursor = (int)n;
                memmove(data->state->filter + data->state->cursor + len,
                        data->state->filter + data->state->cursor,
                        n - data->state->cursor + 1);
                memcpy(data->state->filter + data->state->cursor, buf, len);
                data->state->cursor += len;
                if (g_refresh_callback) g_refresh_callback(data->state->filter, hListBox, data->state->has_listbox ? &data->state->listbox : NULL);
            }
        }
    }
    #endif
    
    if (selection_complete) {
        WindowManager_PopPopup(self->manager);
    }
}

static void PromptPopup_OnHide(Popup* self) {
    PromptPopupData* data = (PromptPopupData*)self;
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) {
        XUnmapWindow(dpy, self->handle.xid);
        XFlush(dpy);
    }
    #endif
    // handle config toggles
    extern EditorMode g_last_mode;
    extern int last_index;
    extern EditorMode g_current_mode;
    if (data->state) {
        if (!KeepSelectionAfterClose) {
            last_index = 0;
            if (data->state->has_listbox) data->state->listbox.selected_index = 0;
        } else {
            last_index = data->state->listbox.selected_index;
        }
        g_last_mode = g_current_mode;
        // per-feature save if needed
        if (ModePersistsThroughFeatures && g_current_feature) {
            extern void SavePerFeatureState(const char* feature, EditorMode mode,int last_index);
            SavePerFeatureState(g_current_feature, g_current_mode, last_index);
        }
    }
}

static void PromptPopup_OnRender(Popup* self) {
    PromptPopupData* data = (PromptPopupData*)self;
    if (!data->state) return;
    
    #if !defined(_WIN32)
    Display* dpy = ((WindowManager*)self->manager)->display;
    Linux_RenderPromptUser(dpy, &self->handle.xid, data->state);
    #endif
}

static void PromptPopup_Destroy(Popup* self) {
    PromptPopupData* data = (PromptPopupData*)self;
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) XDestroyWindow(dpy, self->handle.xid);
    #endif
    free(data->state);
    free(data);
}

static const PopupVTable g_prompt_popup_vtable = {
    .on_show = PromptPopup_OnShow,
    .on_hide = PromptPopup_OnHide,
    .on_key = PromptPopup_OnKey,
    .on_render = PromptPopup_OnRender,
    .destroy = PromptPopup_Destroy
};

Popup* PromptPopup_Create(WindowManager* wm, const PromptSpec* spec) {
    if (!wm || !spec) return NULL;
    
    int screen_width = 1920;
    int screen_height = 1080;
    int layout_w = 900;
    int layout_h = 95 + (spec->count > 0 ? (spec->count * spec->item_height) : 0);
    
    if (layout_h < spec->height) layout_h = spec->height;
    if (layout_h > spec->width) layout_h = spec->width;
    
    int x = (screen_width - layout_w) / 2;
    int y = (screen_height - layout_h) / 2;
    
    PromptPopupData* data = calloc(1, sizeof(PromptPopupData));
    if (!data) return NULL;
    
    data->base.vtable = &g_prompt_popup_vtable;
    data->base.manager = wm;
    data->base.type = POPUP_PROMPT;
    data->spec = *spec;
    data->x = x; data->y = y; data->w = layout_w; data->h = layout_h;
    
    return WindowManager_PushPopup(wm, (Popup*)data);
}

// ============ SimplePrompt Popup ============

typedef struct {
    Popup base;
    SimplePromptSpec spec;
    SimplePromptState* state;
    int x, y, w, h;
} SimplePromptPopupData;

static void SimplePromptPopup_OnShow(Popup* self) {
    SimplePromptPopupData* data = (SimplePromptPopupData*)self;
    WindowManager* wm = (WindowManager*)self->manager;
    selection_complete = false;
    output_buffer[0] = '\0';
    extern bool g_Linux_CtrlHeld, g_Linux_AltHeld, g_Linux_ShiftHeld;
    extern bool g_linux_ctrl_held, g_linux_alt_held, g_linux_shift_held;
    g_Linux_CtrlHeld = g_Linux_AltHeld = g_Linux_ShiftHeld = false;
    g_linux_ctrl_held = g_linux_alt_held = g_linux_shift_held = false;
    
    #if !defined(_WIN32)
    Display* dpy = wm->display;
    data->base.handle.xid = Linux_CreateSimplePromptWindow(dpy, data->x, data->y, data->w, data->h);
    XSelectInput(dpy, data->base.handle.xid, KeyPressMask | ExposureMask);
    XMapWindow(dpy, data->base.handle.xid);
    XSetInputFocus(dpy, data->base.handle.xid, RevertToParent, CurrentTime);
    XFlush(dpy);
    #endif
    
    data->state = calloc(1, sizeof(SimplePromptState));
    if (!data->state) return;
    
    data->state->display_message = (char*)data->spec.prompt;
    data->state->user_input[0] = '\0';
    data->state->cursor = 0;
    data->state->width = data->w;
    data->state->height = data->h;
    const char* p = strchr(data->spec.title, ' ');
    data->state->window_title = (char*)(p ? p+1 : data->spec.title);
    if (!data->state->window_title) data->state->window_title = (char*)data->spec.title;
    self->user_data = data->state;
    
    SetMode(MODE_INSERT);
}

static void SimplePromptPopup_OnKey(Popup* self, int vk_code, bool pressed, int modifiers) {
    if (!pressed) return;
    SimplePromptPopupData* data = (SimplePromptPopupData*)self;
    if (!data->state) return;
    
    #if !defined(_WIN32)
    if (vk_code == VK_ESCAPE) {
        if (IsInsertMode()) {
            SetMode(MODE_NORMAL);
            if (self->vtable && self->vtable->on_render) self->vtable->on_render(self);
        } else {
            selection_complete = true;
            WindowManager_PopPopup(self->manager);
        }
        return;
    }
    if (vk_code == VK_BACK) {
        if (data->state->cursor > 0) {
            size_t n = strlen(data->state->user_input);
            memmove(data->state->user_input + data->state->cursor - 1,
                    data->state->user_input + data->state->cursor,
                    n - data->state->cursor + 1);
            data->state->cursor--;
        }
        return;
    }
    if (vk_code == VK_LEFT) {
        if (data->state->cursor > 0) data->state->cursor--;
        return;
    }
    if (vk_code == VK_RIGHT) {
        if (data->state->cursor < (int)strlen(data->state->user_input)) data->state->cursor++;
        return;
    }
    if (vk_code == VK_HOME) {
        data->state->cursor = 0;
        return;
    }
    if (vk_code == VK_END) {
        data->state->cursor = strlen(data->state->user_input);
        return;
    }
    if (vk_code == VK_RETURN) {
        strncpy(output_buffer, data->state->user_input, sizeof(output_buffer)-1);
        output_buffer[sizeof(output_buffer)-1]='\0';
        selection_complete = true;
        WindowManager_PopPopup(self->manager);
        return;
    }
    if (vk_code >= 32 && vk_code <= 126) {
        if (IsInsertMode()) {
            size_t n = strlen(data->state->user_input);
            if (n < sizeof(data->state->user_input)-1) {
                if (data->state->cursor < 0) data->state->cursor=0;
                if ((size_t)data->state->cursor > n) data->state->cursor=n;
                memmove(data->state->user_input + data->state->cursor + 1,
                        data->state->user_input + data->state->cursor,
                        n - data->state->cursor + 1);
                data->state->user_input[data->state->cursor] = (char)vk_code;
                data->state->cursor++;
            }
        }
        return;
    }
    // fallback to original handler for any other keys
    {
        XEvent ev = {0};
        ev.type = KeyPress;
        ev.xkey.keycode = vk_code;
        ev.xkey.state = modifiers;
        Linux_HandleSimplePromptKey(&ev, data->state);
    }
    #endif
    
    if (selection_complete) {
        WindowManager_PopPopup(self->manager);
    }
}

static void SimplePromptPopup_OnHide(Popup* self) {
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) {
        XUnmapWindow(dpy, self->handle.xid);
        XFlush(dpy);
    }
    #endif
}

static void SimplePromptPopup_OnRender(Popup* self) {
    SimplePromptPopupData* data = (SimplePromptPopupData*)self;
    if (!data->state) return;
    
    #if !defined(_WIN32)
    Display* dpy = ((WindowManager*)self->manager)->display;
    Linux_RenderSimplePromptUser(dpy, &self->handle.xid, data->state);
    #endif
}

static void SimplePromptPopup_Destroy(Popup* self) {
    SimplePromptPopupData* data = (SimplePromptPopupData*)self;
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) XDestroyWindow(dpy, self->handle.xid);
    #endif
    free(data->state);
    free(data);
}

static const PopupVTable g_simple_prompt_popup_vtable = {
    .on_show = SimplePromptPopup_OnShow,
    .on_hide = SimplePromptPopup_OnHide,
    .on_key = SimplePromptPopup_OnKey,
    .on_render = SimplePromptPopup_OnRender,
    .destroy = SimplePromptPopup_Destroy
};

Popup* SimplePromptPopup_Create(WindowManager* wm, const SimplePromptSpec* spec) {
    if (!wm || !spec) return NULL;
    
    int screen_width = 1920;
    int screen_height = 1080;
    int layout_w = 900;
    int layout_h = 95 + (1 > 0 ? (1 * spec->item_height) : 0);
    
    if (layout_h < spec->height) layout_h = spec->height;
    if (layout_h > spec->width) layout_h = spec->width;
    
    int x = (screen_width - layout_w) / 2;
    int y = (screen_height - layout_h) / 2;
    
    SimplePromptPopupData* data = calloc(1, sizeof(SimplePromptPopupData));
    if (!data) return NULL;
    
    data->base.vtable = &g_simple_prompt_popup_vtable;
    data->base.manager = wm;
    data->base.type = POPUP_SIMPLE;
    data->spec = *spec;
    data->x = x; data->y = y; data->w = layout_w; data->h = layout_h;
    
    return WindowManager_PushPopup(wm, (Popup*)data);
}

// Search popup stub
Popup* SearchPopup_Create(WindowManager* wm, const char* title, int x, int y, int w, int h,
                          EditPopupConfirmCallback on_confirm, void* user_data) {
    (void)wm; (void)title; (void)x; (void)y; (void)w; (void)h;
    (void)on_confirm; (void)user_data;
    return NULL; // TODO
}
