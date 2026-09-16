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
#include "helpers/helpers_file.h"
#include "helpers/helpers_ui.h"
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
    char* message;
    int timeout_ms;
    uint64_t start_time;
} MessagePopupData;

static void MessagePopup_OnShow(Popup* self) {
    PROFILE_START(timer);
    #if !defined(_WIN32)
    Display* dpy = ((WindowManager*)self->manager)->display;
    XMapWindow(dpy, self->handle.xid);
    XFlush(dpy);
    #endif
    PROFILE_END_VAR(timer, "window_manager:MessagePopup_OnShow");
}

static void MessagePopup_Destroy(Popup* self) {
    PROFILE_START(timer);
    MessagePopupData* data = (MessagePopupData*)self;
    free(data->message);
    #if !defined(_WIN32)
    Display* dpy = get_display();
    if (dpy) XDestroyWindow(dpy, self->handle.xid);
    #endif
    free(data);
    PROFILE_END_VAR(timer, "window_manager:MessagePopup_Destroy");
}

static const PopupVTable g_message_popup_vtable = {
    .on_show = MessagePopup_OnShow,
    .on_hide = NULL,
    .on_key = NULL,
    .on_render = NULL,
    .destroy = MessagePopup_Destroy
};

Popup* MessagePopup_Create(WindowManager* wm, const char* message, int timeout_ms) {
    PROFILE_START(timer);
    MessagePopupData* data = calloc(1, sizeof(MessagePopupData));
    if (!data) { PROFILE_END_VAR(timer, "window_manager:MessagePopup_Create"); return NULL; }
    
    data->base.vtable = &g_message_popup_vtable;
    data->base.manager = wm;
    data->message = strdup(message);
    data->timeout_ms = timeout_ms;
    
    #if !defined(_WIN32)
    Display* dpy = wm->display;
    data->base.handle.xid = Linux_CreateDisplayMessageWindow(dpy, 0, 0, 400, 100);
    #endif
    
    PROFILE_END_VAR(timer, "window_manager:MessagePopup_Create");
    return WindowManager_PushPopup(wm, (Popup*)data);
}



