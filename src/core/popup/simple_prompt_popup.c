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
    if (data->spec.initial_text) {
        strncpy(data->state->user_input, data->spec.initial_text, sizeof(data->state->user_input)-1);
        data->state->user_input[sizeof(data->state->user_input)-1] = '\0';
        data->state->cursor = strlen(data->state->user_input);
        strncpy(FlintContext_Get()->window.filter, data->spec.initial_text, sizeof(FlintContext_Get()->window.filter)-1);
        FlintContext_Get()->window.filter[sizeof(FlintContext_Get()->window.filter)-1]='\0';
        FlintContext_Get()->window.filter_cursor = data->state->cursor;
    } else {
        data->state->user_input[0] = '\0';
        data->state->cursor = 0;
        FlintContext_Get()->window.filter[0]='\0';
        FlintContext_Get()->window.filter_cursor=0;
    }
    data->state->width = data->w;
    data->state->height = data->h;
    self->user_data = data->state;
    
    SetMode(MODE_INSERT);
    PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_OnShow");
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
    .on_render = SimplePromptPopup_OnRender,
    .destroy = SimplePromptPopup_Destroy
};

Popup* SimplePromptPopup_Create(WindowManager* wm, const SimplePromptSpec* spec) {
    PROFILE_START(timer);
    if (!wm || !spec) { PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_Create"); return NULL; }
    int w,h;

    w = spec->w ? spec->w : 900;
    h = spec->h ? spec->h : 640;
    
    if (!spec->x || !spec->y) WindowManager_GetCenteredPos(w, h, &spec->x, &spec->y);
    
    SimplePromptPopupData* data = calloc(1, sizeof(SimplePromptPopupData));
    if (!data) { PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_Create"); return NULL; }
    
    data->base.vtable = &g_simple_prompt_popup_vtable;
    data->base.manager = wm;
    data->base.type = POPUP_SIMPLE;
    data->spec = *spec;
    data->x = spec->x; data->y = spec->y; data->w = w; data->h = h;
    
    PROFILE_END_VAR(timer, "window_manager:SimplePromptPopup_Create");
    return WindowManager_PushPopup(wm, (Popup*)data);
}


