#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include "core/platform.h"
#include "platform/ui.h"
#include "config.h"
#include "platform/wm_platform.h"
#include "platform/x11/subclasses_x11.h"
#include "core/timer.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <unistd.h>
#include "platform/x11/display_x11.h"
#include "platform/x11/drawing_x11.h"
#include "platform/x11/window_factory_x11.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

bool Platform_PromptPopupShowWindow(Popup* self, int x, int y, int w, int h) {
    PROFILE_START(timer);
    (void)x;
    (void)y;
    WindowManager* wm = (WindowManager*)self->manager;
    Display* dpy = wm ? wm->display : NULL;
    if (!dpy) { PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupShowWindow"); return false; }
    self->handle.xid = Linux_CreatePromptWindow(dpy, x, y, w, h);
    XSelectInput(dpy, self->handle.xid, KeyPressMask | ExposureMask | FocusChangeMask);
    XMapWindow(dpy, self->handle.xid);
    XFlush(dpy);
    XSync(dpy, False);
    for (int _r = 0; _r < 20; _r++) {
        XWindowAttributes _a;
        if (XGetWindowAttributes(dpy, self->handle.xid, &_a) && _a.map_state == IsViewable) break;
        XSync(dpy, False);
    }
    {
        int (*old)(Display*, XErrorEvent*) = XSetErrorHandler(x11_error_handler);
        XSetInputFocus(dpy, self->handle.xid, RevertToParent, CurrentTime);
        XSync(dpy, False);
        XSetErrorHandler(old);
    }
    XFlush(dpy);
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupShowWindow");
    return true;
}

void Platform_PromptPopupHideWindow(Popup* self) {
    PROFILE_START(timer);
    Display* dpy = get_display();
    if (dpy) {
        XUnmapWindow(dpy, self->handle.xid);
        XFlush(dpy);
    }
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupHideWindow");
}

void Platform_PromptPopupRender(Popup* self) {
    PROFILE_START(timer);
    WindowManager* wm = (WindowManager*)self->manager;
    Display* dpy = wm ? wm->display : NULL;
    if (dpy) Linux_RenderPromptUser(dpy, &self->handle.xid, (PromptState*)self->user_data);
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupRender");
}

void Platform_PromptPopupDestroyWindow(Popup* self) {
    PROFILE_START(timer);
    Display* dpy = get_display();
    if (dpy) XDestroyWindow(dpy, self->handle.xid);
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupDestroyWindow");
}
