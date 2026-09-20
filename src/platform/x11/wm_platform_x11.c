#include "platform/wm_platform.h"
#include "core/window_manager.h"
#include "core/context.h"
#include "core/platform.h"
#include "core/prompt_controller.h"
#include "deps/log.h"
#include "platform/x11/display_x11.h"
#include "platform/x11/input_x11.h"
#include "platform/input_state.h"
#include "core/remap_manager.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <string.h>
#include <stdio.h>

extern int WindowManager_RunLinux(WindowManager* wm);
extern void WindowManager_QuitLinux(WindowManager* wm);
extern void WindowManager_ProcessEventsLinux(WindowManager* wm);

bool Platform_WM_InitDisplay(WindowManager* wm) {
    if (!wm) return false;
    wm->display = get_display();
    return wm->display != NULL;
}

int Platform_WM_Run(WindowManager* wm) {
    return WindowManager_RunLinux(wm);
}

void Platform_WM_Quit(WindowManager* wm) {
    WindowManager_QuitLinux(wm);
}

void Platform_WM_ProcessEvents(WindowManager* wm) {
    WindowManager_ProcessEventsLinux(wm);
}

void Platform_WM_OnPopEmpty(WindowManager* wm) {
    (void)wm;
    Platform_DismissMessage();
    x11_clear_suppress();
    remap_reset();
    x11_sync_held_from_phys();
    Platform_InputState_Clear();
}

void Platform_WM_GetMonitorGeometry(int *ox, int *oy, int *ow, int *oh) {
    int x = 3840, y = 0, w = 1920, h = 1080;
    Display *dpy = NULL;
    WindowManager *wm = WindowManager_Get();
    if (wm) dpy = wm->display;
    if (!dpy) dpy = get_display();
    if (dpy) {
        Window root = DefaultRootWindow(dpy);
        XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
        if (res) {
            long best = -1;
            int bx = 3840, by = 0, bw = 1920, bh = 1080;
            bool found_main = false;
            for (int c = 0; c < res->ncrtc; c++) {
                XRRCrtcInfo *ci = XRRGetCrtcInfo(dpy, res, res->crtcs[c]);
                if (!ci || ci->noutput == 0 || ci->width == 0) { if (ci) XRRFreeCrtcInfo(ci); continue; }
                if (ci->x == 3840 && ci->y == 0 && ci->width == 1920) {
                    bx = ci->x; by = ci->y; bw = ci->width; bh = ci->height;
                    found_main = true;
                    XRRFreeCrtcInfo(ci);
                    break;
                }
                long area = (long)ci->width * ci->height;
                if (area > best) { best = area; bx = ci->x; by = ci->y; bw = ci->width; bh = ci->height; }
                XRRFreeCrtcInfo(ci);
            }
            XRRFreeScreenResources(res);
            if (found_main || best >= 0) { x = bx; y = by; w = bw; h = bh; }
        }
    }
    if (ox) *ox = x;
    if (oy) *oy = y;
    if (ow) *ow = w;
    if (oh) *oh = h;
}

char* Platform_WM_ShowPrompt(WindowManager* wm, const PromptSpec* spec) {
    if (!wm || !spec) return NULL;
    Popup* popup = PromptPopup_Create(wm, spec);
    if (!popup) return NULL;
    Display* dpy = wm->display;
    while (wm->running && popup->manager) {
        Platform_ProcessNestedEvents(wm, popup);
        if (PromptController_PollSequenceTimeout()) {
            Popup* top = WindowManager_GetTopPopup(wm);
            if (top && top->vtable && top->vtable->on_render) top->vtable->on_render(top);
            if (dpy) XFlush(dpy);
        }
    }
    if (popup->manager == NULL) {
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    } else {
        WindowManager_PopPopup(wm);
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    }
    if (dpy) { XFlush(dpy); XSync(dpy, False); }
    FlintContext* _c = FlintContext_Get();
    return _c->window.output_buffer[0] ? _c->window.output_buffer : NULL;
}

