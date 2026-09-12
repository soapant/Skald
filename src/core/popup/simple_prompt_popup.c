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
    SimplePromptSpec spec;
    SimplePromptState* state;
    int x, y, w, h;
} SimplePromptPopupData;

static void SimplePromptPopup_OnShow(Popup* self) {
    PROFILE_START(timer);
    SimplePromptPopupData* data = (SimplePromptPopupData*)self;
    WindowManager* wm = (WindowManager*)self->manager;
    FlintContext_Get()->window.selection_complete = false;
    FlintContext_Get()->window.output_buffer[0] = '\0';
    #if !defined(_WIN32)
    Display* dpy = wm->display;
    data->base.handle.xid = Linux_CreateSimplePromptWindow(dpy, data->x, data->y, data->w, data->h);
    XSelectInput(dpy, data->base.handle.xid, KeyPressMask | ExposureMask);
    XMapWindow(dpy, data->base.handle.xid);
    XFlush(dpy);
    XSync(dpy, False);
    {
        int (*old)(Display*, XErrorEvent*) = XSetErrorHandler(x11_error_handler);
        XSetInputFocus(dpy, data->base.handle.xid, RevertToParent, CurrentTime);
        XSync(dpy, False);
        XSetErrorHandler(old);
    }
    XFlush(dpy);
    #endif
    
    data->state = calloc(1, sizeof(SimplePromptState));
    if (!data->state) { PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnShow"); return; }
    
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
    PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnShow");
}

static void SimplePromptPopup_OnKey(Popup* self, int vk_code, bool pressed, int modifiers) {
    PROFILE_START(timer);
    if (!pressed) { PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnKey"); return; }
    SimplePromptPopupData* data = (SimplePromptPopupData*)self;
    if (!data->state) { PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnKey"); return; }
    
    #if !defined(_WIN32)
    int vk_upper = (vk_code >= 'a' && vk_code <= 'z') ? vk_code - 32 : vk_code;
    bool ctrl_pressed = modifiers & HOTKEY_MOD_CTRL;
    bool shift_pressed = modifiers & HOTKEY_MOD_SHIFT;
    bool alt_pressed = modifiers & HOTKEY_MOD_ALT;
    if (PromptController_ShouldSwallowPending(vk_upper, modifiers)) { PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnKey"); return; }
    if (PromptController_TryVimMotion(vk_upper, ctrl_pressed, shift_pressed, alt_pressed)) {
        FlintContext* _c = FlintContext_Get();
        if (_c->window.selection_complete) { WindowManager_PopPopup(self->manager); _c->window.selection_complete = false; }
        PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnKey");
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
        PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_BACK) {
        VimContext ctx = Vim_FromSimpleState(data->state);
        int pos = Vim_GetCursor(&ctx);
        if (ctrl_pressed) { Vim_SetText(&ctx, ""); }
        else if (pos > 0) {
            char buf[512]; Vim_GetText(&ctx, buf, sizeof(buf));
            int len = strlen(buf);
            memmove(buf + pos - 1, buf + pos, len - pos + 1);
            Vim_SetText(&ctx, buf);
            Vim_SetCursor(&ctx, pos - 1);
        }
        PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_LEFT) {
        VimContext ctx = Vim_FromSimpleState(data->state);
        int pos = Vim_GetCursor(&ctx);
        if (pos > 0) Vim_SetCursor(&ctx, pos - 1);
        PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_RIGHT) {
        VimContext ctx = Vim_FromSimpleState(data->state);
        int pos = Vim_GetCursor(&ctx);
        char buf[512]; Vim_GetText(&ctx, buf, sizeof(buf));
        if (pos < (int)strlen(buf)) Vim_SetCursor(&ctx, pos + 1);
        PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_HOME) {
        VimContext ctx = Vim_FromSimpleState(data->state);
        Vim_SetCursor(&ctx, 0);
        PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_END) {
        VimContext ctx = Vim_FromSimpleState(data->state);
        char buf[512]; Vim_GetText(&ctx, buf, sizeof(buf));
        Vim_SetCursor(&ctx, strlen(buf));
        PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnKey");
        return;
    }
    if (vk_code == VK_RETURN) {
        FlintContext* _c = FlintContext_Get();
        char buf[512]; VimContext ctx = Vim_FromSimpleState(data->state);
        Vim_GetText(&ctx, buf, sizeof(buf));
        strncpy(_c->window.output_buffer, buf, sizeof(_c->window.output_buffer)-1);
        _c->window.output_buffer[sizeof(_c->window.output_buffer)-1]='\0';
        _c->window.selection_complete = true;
        WindowManager_PopPopup(self->manager);
        PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnKey");
        return;
    }
    if (vk_code >= 32 && vk_code <= 126) {
        if (IsInsertMode()) {
            VimContext ctx = Vim_FromSimpleState(data->state);
            char buf[512]; Vim_GetText(&ctx, buf, sizeof(buf));
            int pos = Vim_GetCursor(&ctx);
            size_t n = strlen(buf);
            if (n < sizeof(buf)-1) {
                if (pos < 0) pos=0;
                if ((size_t)pos > n) pos=n;
                memmove(buf + pos + 1, buf + pos, n - pos + 1);
                buf[pos] = (char)vk_code;
                Vim_SetText(&ctx, buf);
                Vim_SetCursor(&ctx, pos + 1);
            }
        }
        PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnKey");
        return;
    }
    #endif
    FlintContext* _c4 = FlintContext_Get();
    if (_c4->window.selection_complete) {
        WindowManager_PopPopup(self->manager);
        _c4->window.selection_complete = false;
    }
    PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnKey");
}

static void SimplePromptPopup_OnHide(Popup* self) {
    PROFILE_START(timer);
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) {
        XUnmapWindow(dpy, self->handle.xid);
        XFlush(dpy);
    }
    #endif
    PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnHide");
}

static void SimplePromptPopup_OnRender(Popup* self) {
    PROFILE_START(timer);
    SimplePromptPopupData* data = (SimplePromptPopupData*)self;
    if (!data->state) { PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnRender"); return; }
    
    #if !defined(_WIN32)
    Display* dpy = ((WindowManager*)self->manager)->display;
    Linux_RenderSimplePromptUser(dpy, &self->handle.xid, data->state);
    #endif
    PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnRender");
}

static void SimplePromptPopup_Destroy(Popup* self) {
    PROFILE_START(timer);
    SimplePromptPopupData* data = (SimplePromptPopupData*)self;
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) XDestroyWindow(dpy, self->handle.xid);
    #endif
    free(data->state);
    free(data);
    PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_Destroy");
}

static const PopupVTable g_simple_prompt_popup_vtable = {
    .on_show = SimplePromptPopup_OnShow,
    .on_hide = SimplePromptPopup_OnHide,
    .on_key = SimplePromptPopup_OnKey,
    .on_render = SimplePromptPopup_OnRender,
    .destroy = SimplePromptPopup_Destroy
};

Popup* SimplePromptPopup_Create(WindowManager* wm, const SimplePromptSpec* spec) {
    PROFILE_START(timer);
    if (!wm || !spec) { PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_Create"); return NULL; }
    
    int layout_w = 900;
    int layout_h = 95 + (1 > 0 ? (1 * spec->item_height) : 0);
    
    if (layout_h < spec->height) layout_h = spec->height;
    if (layout_h > spec->width) layout_h = spec->width;
    
    int x, y;
    WindowManager_GetCenteredPos(layout_w, layout_h, &x, &y);
    
    SimplePromptPopupData* data = calloc(1, sizeof(SimplePromptPopupData));
    if (!data) { PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_Create"); return NULL; }
    
    data->base.vtable = &g_simple_prompt_popup_vtable;
    data->base.manager = wm;
    data->base.type = POPUP_SIMPLE;
    data->spec = *spec;
    data->x = x; data->y = y; data->w = layout_w; data->h = layout_h;
    
    PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_Create");
    return WindowManager_PushPopup(wm, (Popup*)data);
}


