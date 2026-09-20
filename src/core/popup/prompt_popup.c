#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include "core/platform.h"
#include "platform/ui.h"
#include "core/modal.h"
#include "core/feature_data.h"
#include "core/vim.h"
#include "bindings/vim_helpers.h"
#include "bindings/vim_motions.h"
#include "bindings/bindings.h"
#include "core/prompt_controller.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_ui.h"
#include "config.h"
#include "core/feature_loader.h"
#include "core/history.h"
#include "deps/log.h"
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
#include "platform/x11/display_x11.h"
#include "platform/x11/drawing_x11.h"
#include "platform/x11/window_factory_x11.h"
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
    FlintContext_Get()->window.filter[0] = '\0';
    FlintContext_Get()->window.filter_cursor = 0;
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

