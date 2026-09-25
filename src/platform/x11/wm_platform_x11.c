#include "platform/wm_platform.h"
#include "core/window_manager.h"
#include "core/context.h"
#include "core/platform.h"
#include "core/prompt_controller.h"
#include "deps/log.h"
#include "platform/x11/input_x11.h"
#include "platform/input_state.h"
#include "core/remap_manager.h"
#include "platform/x11/display_x11.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>

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
    x11_clear_suppress();
    remap_reset();
    x11_sync_held_from_phys();
    Platform_InputState_Clear();
}

void Platform_WM_GetMonitorGeometry(int *ox, int *oy, int *ow, int *oh) {
    int x = 0, y = 0, w = 1920, h = 1080;
    Display *dpy = NULL;
    WindowManager *wm = WindowManager_Get();
    if (wm) dpy = wm->display;
    if (!dpy) dpy = get_display();
    if (dpy) {
        Window root = DefaultRootWindow(dpy);
        w = DisplayWidth(dpy, DefaultScreen(dpy));
        h = DisplayHeight(dpy, DefaultScreen(dpy));
        int px = 0, py = 0;
        Window rw = None, cw = None;
        int rx = 0, ry = 0, wx = 0, wy = 0;
        unsigned int m = 0;
        if (XQueryPointer(dpy, root, &rw, &cw, &rx, &ry, &wx, &wy, &m)) { px = rx; py = ry; }
        XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
        if (res) {
            int bx = x, by = y, bw = w, bh = h;
            bool found = false;
            for (int c = 0; c < res->ncrtc && !found; c++) {
                XRRCrtcInfo *ci = XRRGetCrtcInfo(dpy, res, res->crtcs[c]);
                if (!ci || ci->noutput == 0 || ci->width == 0 || ci->height == 0) { if (ci) XRRFreeCrtcInfo(ci); continue; }
                if (px >= ci->x && px < ci->x + ci->width && py >= ci->y && py < ci->y + ci->height) {
                    bx = ci->x; by = ci->y; bw = ci->width; bh = ci->height;
                    found = true;
                }
                XRRFreeCrtcInfo(ci);
            }
            if (!found) {
                RROutput primary = XRRGetOutputPrimary(dpy, root);
                for (int c = 0; c < res->ncrtc && !found; c++) {
                    XRRCrtcInfo *ci = XRRGetCrtcInfo(dpy, res, res->crtcs[c]);
                    if (!ci || ci->noutput == 0 || ci->width == 0 || ci->height == 0) { if (ci) XRRFreeCrtcInfo(ci); continue; }
                    for (int o = 0; o < ci->noutput; o++) {
                        if (ci->outputs[o] == primary) {
                            bx = ci->x; by = ci->y; bw = ci->width; bh = ci->height;
                            found = true;
                            break;
                        }
                    }
                    XRRFreeCrtcInfo(ci);
                }
            }
            if (!found) {
                long best = -1;
                for (int c = 0; c < res->ncrtc; c++) {
                    XRRCrtcInfo *ci = XRRGetCrtcInfo(dpy, res, res->crtcs[c]);
                    if (!ci || ci->noutput == 0 || ci->width == 0 || ci->height == 0) { if (ci) XRRFreeCrtcInfo(ci); continue; }
                    long area = (long)ci->width * ci->height;
                    if (area > best) { best = area; bx = ci->x; by = ci->y; bw = ci->width; bh = ci->height; }
                    XRRFreeCrtcInfo(ci);
                }
                if (best >= 0) found = true;
            }
            XRRFreeScreenResources(res);
            if (found) { x = bx; y = by; w = bw; h = bh; }
            log_info("monitor ptr=(%d,%d) geom=(%d,%d %dx%d)", px, py, x, y, w, h);
        }
    }
    if (ox) *ox = x;
    if (oy) *oy = y;
    if (ow) *ow = w;
    if (oh) *oh = h;
}

char* Platform_PromptUser(WindowManager* wm, const PromptSpec* spec) {
    if (!wm || !spec) return NULL;
    Popup* popup = PromptPopup_Create(wm, spec);
    if (!popup) return NULL;
    Display* dpy = wm->display;
    long long last_tick = 0;
    while (wm->running && popup->manager) {
        Platform_WM_ProcessEvents(wm);
        long long now = Platform_GetWallClockMs();
        if (now - last_tick >= 1000 && WindowManager_TopPopupWantsTick(wm)) {
            last_tick = now;
            Popup* top = WindowManager_GetTopPopup(wm);
            if (top && top->vtable && top->vtable->on_render) top->vtable->on_render(top);
            if (dpy) XFlush(dpy);
        }
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

