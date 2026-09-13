#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include "core/platform.h"
#include "core/common_ui.h"
#include "core/modal.h"
#include "core/feature_data.h"
#include "core/vim.h"
#include "bindings/vim_helpers.h"
#include "bindings/vim_motions.h"
#include "bindings/bindings.h"
#include "core/number_shortcut.h"
#include "core/prompt_controller.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include "config.h"
#include "core/feature_loader.h"
#include "core/history.h"
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if !defined(_WIN32)
#include <pthread.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <unistd.h>
#include <stdatomic.h>
#include "platform/x11/display.h"
#include "platform/x11/drawing.h"
#include "platform/x11/window_factory.h"
extern int WindowManager_RunLinux(WindowManager* wm);
extern void WindowManager_ProcessEventsLinux(WindowManager* wm);
extern void WindowManager_QuitLinux(WindowManager* wm);
extern void Linux_ProcessXEvent(Display* dpy, XEvent* ev, WindowManager* wm);
extern void EditBufferBackspace(char* buf, size_t* cursor);
extern bool EditBufferDeleteChar(char* buf, size_t* cursor);
#else
#include <windows.h>
#include "core/timer.h"
extern int WindowManager_RunWin32(WindowManager* wm);
extern void WindowManager_ProcessEventsWin32(WindowManager* wm);
extern void WindowManager_QuitWin32(WindowManager* wm);
#endif
typedef struct {
    Popup base;
    PromptSpec spec;
    PromptState* state;
    int x, y, w, h;
} PromptPopupData;

static void PromptPopup_OnShow(Popup* self) {
    PROFILE_START(timer);
    PromptPopupData* data = (PromptPopupData*)self;
    WindowManager* wm = (WindowManager*)self->manager;
    FlintContext* ctx = FlintContext_Get();
    ctx->window.selection_complete = false;
    ctx->window.output_buffer[0] = '\0';
    
    #if !defined(_WIN32)
    Display* dpy = wm->display;
    data->base.handle.xid = Linux_CreatePromptWindow(dpy, data->x, data->y, data->w, data->h);
    XSelectInput(dpy, data->base.handle.xid, KeyPressMask | ExposureMask | FocusChangeMask);
    XMapWindow(dpy, data->base.handle.xid);
    XFlush(dpy);
    XSync(dpy, False);
    for (int _r=0; _r<20; _r++) {
        XWindowAttributes _a;
        if (XGetWindowAttributes(dpy, data->base.handle.xid, &_a) && _a.map_state == IsViewable) break;
        XSync(dpy, False);
    }
    {
        int (*old)(Display*, XErrorEvent*) = XSetErrorHandler(x11_error_handler);
        XSetInputFocus(dpy, data->base.handle.xid, RevertToParent, CurrentTime);
        XSync(dpy, False);
        XSetErrorHandler(old);
    }
    XFlush(dpy);
    #endif
    
    data->state = calloc(1, sizeof(PromptState));
    if (!data->state) { PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnShow"); return; }
    
    data->state->items = (char**)data->spec.items;
    data->state->item_count = 0;
    data->state->cursor = 0;
    data->state->width = data->w;
    data->state->height = data->h;
    const char* title = data->spec.title ? data->spec.title : "Flint";
    const char* p = strchr(title, ' ');
    data->state->window_title = (char*)(p ? p+1 : title);
    data->state->filter[0] = '\0';
    if (data->spec.items || data->spec.refresh) {
      data->state->has_listbox = true;
    } else {
      data->state->has_listbox = false;
    }
    ListBoxInfo_Init(&data->state->listbox);
    data->state->listbox.selected_index = -1;
    extern EditorMode g_last_mode;
    if (ModePersistsThroughFeatures) {
        SetModeWithListBox(g_last_mode, &data->state->listbox);
        if (RememberLastIndex && last_index >= 0 && data->state->has_listbox) {
            data->state->listbox.selected_index = last_index;
        }
    } else {
        SetModeWithListBox(MODE_INSERT, &data->state->listbox);
    }
    self->user_data = data->state;
    
    FlintContext* ctx2 = FlintContext_Get();
    if (!wm->ctx) wm->ctx = ctx2;
    ctx2->prompt_callbacks.refresh = data->spec.refresh;
    ctx2->prompt_callbacks.enter = data->spec.enter;
    ctx2->prompt_callbacks.ctrl_enter = data->spec.ctrl_enter;
    ctx2->prompt_callbacks.tab = data->spec.tab;
    ctx2->prompt_callbacks.copy = data->spec.copy;
    ctx2->prompt_callbacks.render = data->spec.render;
    ctx2->prompt_callbacks.edit = data->spec.edit;
    ctx2->prompt_callbacks.filter = data->spec.filter;
    ctx2->prompt_callbacks.help = data->spec.help;
    ctx2->window.current_feature = (char*)(p ? p+1 : title);
    ctx2->window.title = title;
    ctx2->window.callbacks = ctx2->prompt_callbacks;
    SetCurrentFeatureName(ctx2->window.current_feature);
    History_ResetCursor(ctx2->window.current_feature);
    if (data->spec.refresh) data->spec.refresh(data->state->filter, FlintContext_Get()->window.hListBox, data->state->has_listbox ? &data->state->listbox : NULL);
    PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnShow");
}

static void PromptPopup_OnKey(Popup* self, int vk_code, bool pressed, int modifiers) {
    PROFILE_START(timer);
    if (!pressed) { PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey"); return; }
    PromptPopupData* data = (PromptPopupData*)self;
    if (!data->state) { PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey"); return; }
    
    #if !defined(_WIN32)
    bool ctrl_pressed = modifiers & HOTKEY_MOD_CTRL;
    bool shift_pressed = modifiers & HOTKEY_MOD_SHIFT;
    bool alt_pressed = modifiers & HOTKEY_MOD_ALT;
    int vk_upper = (vk_code >= 'a' && vk_code <= 'z') ? vk_code - 32 : vk_code;
    
    if (PromptController_ShouldSwallowPending(vk_upper, modifiers)) { PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey"); return; }
    if (PromptController_TryVimMotion(vk_upper, ctrl_pressed, shift_pressed, alt_pressed)) {
        FlintContext* _c = FlintContext_Get();
        if (_c->window.selection_complete) { WindowManager_PopPopup(self->manager); _c->window.selection_complete = false; }
        PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    
    if (vk_code == VK_ESCAPE) {
        if (IsInsertMode()) {
            SetMode(MODE_NORMAL);
            if (self->vtable && self->vtable->on_render) self->vtable->on_render(self);
        } else {
            FlintContext_Get()->window.selection_complete = true;
            WindowManager_PopPopup(self->manager);
        }
        PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    
    if (vk_code == VK_BACK) {
        if (ctrl_pressed) {
            data->state->filter[0] = '\0';
            data->state->cursor = 0;
        } else {
            EditBufferBackspace(data->state->filter, &data->state->cursor);
        }
        History_ResetCursor(FlintContext_Get()->window.current_feature);
        if (data->spec.refresh) data->spec.refresh(data->state->filter, FlintContext_Get()->window.hListBox, data->state->has_listbox ? &data->state->listbox : NULL);
        PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    
    if (vk_code >= '0' && vk_code <= '9') {
        if (ctrl_pressed) {
            HandleNumberShortcut(vk_code, FlintContext_Get()->window.hListBox, FlintContext_Get()->window.g_ActiveEditControl, data->state->has_listbox ? &data->state->listbox : NULL);
            PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey");
            return;
        }
    }
    
    if (vk_code == VK_RIGHT) {
        CursorForward(&data->state->cursor, strlen(data->state->filter));
        PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_LEFT) {
        CursorBack(&data->state->cursor, strlen(data->state->filter));
        PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_UP) {
        if (ctrl_pressed) {
            VimContext ctx = Vim_FromState(data->state);
            SetCursorToStartOfText(&ctx);
        } else if (data->state->has_listbox) {
            if (data->state->listbox.selected_index == -1) {
                char hist[512];
                const char* feat = FlintContext_Get()->window.current_feature;
                if (feat && History_CycleUp(feat, hist, sizeof(hist))) {
                    strncpy(data->state->filter, hist, sizeof(data->state->filter)-1);
                    data->state->filter[sizeof(data->state->filter)-1]='\0';
                    data->state->cursor = (int)strlen(data->state->filter);
                    if (data->spec.refresh) data->spec.refresh(data->state->filter, FlintContext_Get()->window.hListBox, data->state->has_listbox ? &data->state->listbox : NULL);
                    if (self->vtable && self->vtable->on_render) self->vtable->on_render(self);
                } else {
                    UI_NavigateUp(&data->state->listbox, FlintContext_Get()->window.hListBox);
                }
            } else {
                UI_NavigateUp(&data->state->listbox, FlintContext_Get()->window.hListBox);
            }
        }
        PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_DOWN) {
        if (ctrl_pressed) {
            VimContext ctx = Vim_FromState(data->state);
            SetCursorToEndOfText(&ctx);
        } else if (data->state->has_listbox) {
            UI_NavigateDown(&data->state->listbox, FlintContext_Get()->window.hListBox);
        }
        PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_DELETE) {
        if (EditBufferDeleteChar(data->state->filter, &data->state->cursor)) {
            History_ResetCursor(FlintContext_Get()->window.current_feature);
            if (data->spec.refresh) data->spec.refresh(data->state->filter, FlintContext_Get()->window.hListBox, data->state->has_listbox ? &data->state->listbox : NULL);
        }
        PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_HOME) {
        VimContext ctx = Vim_FromState(data->state);
        SetCursorToStartOfText(&ctx);
        PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_END) {
        VimContext ctx = Vim_FromState(data->state);
        SetCursorToEndOfText(&ctx);
        PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_TAB) {
        if (data->spec.tab) data->spec.tab(&data->state->listbox);
        PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey");
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
        const char* feat = FlintContext_Get()->window.current_feature;
        if (feat && sel && sel[0]) History_Add(feat, sel);
        bool should_close = false;
        if (is_ctrl) {
            if (data->spec.ctrl_enter) should_close = data->spec.ctrl_enter(sel, data->state->has_listbox ? &data->state->listbox : NULL);
            else should_close = false;
        } else {
            if (data->spec.enter) should_close = data->spec.enter(sel, data->state->has_listbox ? &data->state->listbox : NULL);
            else should_close = true;
        }
        FlintContext_Get()->window.selection_complete = should_close;
        if (should_close) WindowManager_PopPopup(self->manager);
        PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey");
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
                History_ResetCursor(FlintContext_Get()->window.current_feature);
                if (data->spec.refresh) data->spec.refresh(data->state->filter, FlintContext_Get()->window.hListBox, data->state->has_listbox ? &data->state->listbox : NULL);
            }
        }
    }
    #endif
    FlintContext* _c2 = FlintContext_Get();
    if (_c2->window.selection_complete) {
        WindowManager_PopPopup(self->manager);
        _c2->window.selection_complete = false;
    }
    PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnKey");
}

static void PromptPopup_OnHide(Popup* self) {
    PROFILE_START(timer);
    PromptPopupData* data = (PromptPopupData*)self;
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) {
        XUnmapWindow(dpy, self->handle.xid);
        XFlush(dpy);
    }
    #endif
    extern EditorMode g_last_mode;
    extern void SavePerFeatureState(const char* feature, EditorMode mode, int last_index);
    if (data->state) {
        if (!KeepSelectionAfterClose) {
            last_index = 0;
            if (data->state->has_listbox) data->state->listbox.selected_index = 0;
        } else {
            last_index = data->state->listbox.selected_index;
        }
        g_last_mode = g_current_mode;
        FlintContext* _c3 = FlintContext_Get();
        const char* feat = _c3->window.current_feature;
        if (ModePersistsThroughFeatures && feat) {
            SavePerFeatureState(feat, g_current_mode, last_index);
        }
    }
    PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnHide");
}

static void PromptPopup_OnRender(Popup* self) {
    PROFILE_START(timer);
    PromptPopupData* data = (PromptPopupData*)self;
    if (!data->state) { PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnRender"); return; }
    
    #if !defined(_WIN32)
    Display* dpy = ((WindowManager*)self->manager)->display;
    Linux_RenderPromptUser(dpy, &self->handle.xid, data->state);
    #endif
    PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnRender");
}

static void PromptPopup_Destroy(Popup* self) {
    PROFILE_START(timer);
    PromptPopupData* data = (PromptPopupData*)self;
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) XDestroyWindow(dpy, self->handle.xid);
    #endif
    free(data->state);
    free(data);
    PROFILE_END_VAR(timer, "window_manager:PromptPopup_Destroy");
}

static const PopupVTable g_prompt_popup_vtable = {
    .on_show = PromptPopup_OnShow,
    .on_hide = PromptPopup_OnHide,
    .on_key = PromptPopup_OnKey,
    .on_render = PromptPopup_OnRender,
    .destroy = PromptPopup_Destroy
};

Popup* PromptPopup_Create(WindowManager* wm, const PromptSpec* spec) {
    PROFILE_START(timer);
    if (!wm || !spec) { PROFILE_END_VAR(timer, "window_manager:PromptPopup_Create"); return NULL; }
    
    int layout_w = 900;
    int layout_h = 640;
    
    int x, y;
    WindowManager_GetCenteredPos(layout_w, layout_h, &x, &y);
    
    PromptPopupData* data = calloc(1, sizeof(PromptPopupData));
    if (!data) { PROFILE_END_VAR(timer, "window_manager:PromptPopup_Create"); return NULL; }
    
    data->base.vtable = &g_prompt_popup_vtable;
    data->base.manager = wm;
    data->base.type = POPUP_PROMPT;
    data->spec = *spec;
    data->x = x; data->y = y; data->w = layout_w; data->h = layout_h;
    
    PROFILE_END_VAR(timer, "window_manager:PromptPopup_Create");
    return WindowManager_PushPopup(wm, (Popup*)data);
}

