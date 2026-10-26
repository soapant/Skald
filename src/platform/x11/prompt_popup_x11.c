#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include "wm/wm.h"
#include "platform/platform.h"
#include "platform/draw.h"
#include "platform/ui.h"
#include "config.h"
#include "platform/wm_platform.h"
#include "platform/x11/subclasses_x11.h"
#include "platform/x11/drawing_x11.h"
#include "platform/x11/display_x11.h"
#include "core/timer.h"
#include "core/history.h"
#include "core/prompt_controller.h"
#include "core/listbox_hover.h"
#include "core/prompt_keys.h"
#include "core/hotkey_text.h"
#include "platform/x11/input_x11.h"
#include "platform/x11/window_factory_x11.h"
#include "platform/input.h"
#include "bindings/vim_helpers.h"
#include "bindings/vim_state.h"
#include "helpers/helpers_alias.h"
#include "platform/draw.h"
#include "helpers/helpers_ui.h"
#include "deps/log.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

bool Platform_PromptPopupShowWindow(Popup* self, int x, int y, int w, int h) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    (void)x;
    (void)y;
    WindowManager* wm = (WindowManager*)self->manager;
    Display* dpy = wm ? wm->display : NULL;
    if (!dpy) { PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupShowWindow"); return false; }
    struct timespec _t0, _t1, _t2, _t3;
    clock_gettime(CLOCK_MONOTONIC, &_t0);
    ListBox_InvalidateMousePos();
    self->handle.xid = x11_create_prompt_window(dpy, x, y, w, h);
    XSelectInput(dpy, self->handle.xid, KeyPressMask | ExposureMask | FocusChangeMask | ButtonPressMask);
    clock_gettime(CLOCK_MONOTONIC, &_t1);
    Popup* popup = self;
    (void)popup;
    clock_gettime(CLOCK_MONOTONIC, &_t2);
    if (self->vtable && self->vtable->on_render) self->vtable->on_render(self);
    clock_gettime(CLOCK_MONOTONIC, &_t3);
    log_info("prompt show: create=%.2fms bg=%.2fms first_render=%.2fms rounded=%d radius=%d opacity=%d",
             (_t1.tv_sec-_t0.tv_sec)*1000.0+(_t1.tv_nsec-_t0.tv_nsec)/1e6,
             (_t2.tv_sec-_t1.tv_sec)*1000.0+(_t2.tv_nsec-_t1.tv_nsec)/1e6,
             (_t3.tv_sec-_t2.tv_sec)*1000.0+(_t3.tv_nsec-_t2.tv_nsec)/1e6,
             (int)RoundedCorners, CornerRadius, (int)WindowOpacity);
    XMapWindow(dpy, self->handle.xid);
    XSync(dpy, False);
    {
        XWindowAttributes _a;
        if (XGetWindowAttributes(dpy, self->handle.xid, &_a)) {
            log_info("prompt mapped req=(%d,%d %dx%d) actual=(%d,%d %dx%d) state=%d", x, y, w, h, _a.x, _a.y, _a.width, _a.height, (int)_a.map_state);
        } else {
            log_info("prompt mapped req=(%d,%d %dx%d) actual=unknown", x, y, w, h);
        }
    }
    {
        int (*old)(Display*, XErrorEvent*) = XSetErrorHandler(x11_error_handler);
        XSetInputFocus(dpy, self->handle.xid, RevertToParent, CurrentTime);
        XSync(dpy, False);
        XSetErrorHandler(old);
        Window _fw = None;
        int _rev = 0;
        XGetInputFocus(dpy, &_fw, &_rev);
        if (_fw != self->handle.xid) {
            old = XSetErrorHandler(x11_error_handler);
            XSetInputFocus(dpy, self->handle.xid, RevertToParent, CurrentTime);
            XSync(dpy, False);
            XSetErrorHandler(old);
            XGetInputFocus(dpy, &_fw, &_rev);
        }
        log_info("prompt focus win=%lu want=%lu ok=%d", (unsigned long)_fw, (unsigned long)self->handle.xid, _fw == self->handle.xid);
        char* _nm = NULL;
        if (XFetchName(dpy, self->handle.xid, &_nm) && _nm) {
            if (!x11_is_own_window_name(_nm)) {
                Wm_FocusByTitle(_nm);
            }
            XFree(_nm);
        }
    }
    XFlush(dpy);
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupShowWindow");
    { PROFILE_END_VAR(_pt, "Platform_PromptPopupShowWindow"); return true; }
}

void Platform_PromptPopupHideWindow(Popup* self) {
    PROFILE_START(_pt);
    PROFILE_START(timer);
    Display* dpy = get_display();
    if (dpy) {
        XUnmapWindow(dpy, self->handle.xid);
        XFlush(dpy);
    }
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupHideWindow");
}

void Platform_PromptPopupRender(Popup* self) {
        PROFILE_START(_pt);
WindowManager* wm = (WindowManager*)self->manager;
    Display* dpy = wm ? wm->display : NULL;
    if (dpy) {
        PromptState* psr = (PromptState*)self->user_data;
        if (psr && psr->no_bg) {
            Platform_SetTransparentWindow(true);
            x11_render_prompt_user(dpy, &self->handle.xid, (PromptState*)self->user_data);
            Platform_SetTransparentWindow(false);
            { PROFILE_END_VAR(_pt, "Platform_PromptPopupRender"); return; }
        }
        x11_render_prompt_user(dpy, &self->handle.xid, (PromptState*)self->user_data);
    }
}

void Platform_PromptPopupDestroyWindow(Popup* self) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    Display* dpy = get_display();
    if (dpy) {
        x11_release_prompt_buffer(dpy, self->handle.xid);
        XDestroyWindow(dpy, self->handle.xid);
    }
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupDestroyWindow");
}


static void command_confirm(const char* new_text, void* user_data) {
        PROFILE_START(_pt);
PromptState* state = (PromptState*)user_data;
    ListBox_CommandExecute(&state->listbox, new_text);
}

bool Platform_PromptPopupOnKey(Popup* self, const PopupKeyEvent* key) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    PromptState* state = self ? (PromptState*)self->user_data : NULL;
    if (!state) { PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey"); return true; }
    if (Prompt_HandleKey(state, key)) { PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey"); return true; }

    int wParam = key->vk;
    bool shift_pressed = key->shift;

    if (wParam == COMMAND_MODE_KEY && shift_pressed) {
        WindowManager* wm = WindowManager_Get();
        Popup* popup = EditPopup_Create(wm,"Command Mode", 960, 540, 1000, 100, "LEFT", command_confirm,state);
        WindowManager_PushPopup(wm, popup);
        return true;
    }

    if (IsInsertMode() && key->text_len > 0) {
        bool swallow = HotkeyText_ShouldSwallow(key);
        if (!swallow && HotkeyText_IsPrintable(key)) {
            Prompt_InsertText(state, key->text, key->text_len);
            PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
            return true;
        }
    }

    PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
    { PROFILE_END_VAR(_pt, "Platform_PromptPopupOnKey"); return false; }
}

