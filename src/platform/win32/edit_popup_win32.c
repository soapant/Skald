#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include "core/timer.h"
#include "platform/wm_platform.h"
#include <windows.h>

bool Platform_EditPopupCreateWindow(Popup* self, int x, int y, int w, int h, const char* title) {
    PROFILE_START(timer);
    WMEditPopupData* data = (WMEditPopupData*)self;
    HINSTANCE hInst = GetModuleHandle(NULL);
    HWND hwnd = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "EditPopupClass", title, WS_POPUP, x, y, w, h, NULL, NULL, hInst, data);
    if (!hwnd) { PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupCreateWindow"); return false; }
    data->base.handle = WH_FROM_HWND(hwnd);
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupCreateWindow");
    return true;
}

void Platform_EditPopupShowWindow(Popup* self) {
    PROFILE_START(timer);
    HWND hwnd = WH_TO_HWND(self->handle);
    if (hwnd && IsWindow(hwnd)) {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
    }
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupShowWindow");
}

void Platform_EditPopupHideWindow(Popup* self) {
    PROFILE_START(timer);
    HWND hwnd = WH_TO_HWND(self->handle);
    if (hwnd && IsWindow(hwnd)) ShowWindow(hwnd, SW_HIDE);
    WindowManager* wm = WindowManager_Get();
    if (wm) {
        Popup* top = WindowManager_GetTopPopup(wm);
        if (top && WH_TO_HWND(top->handle) && IsWindow(WH_TO_HWND(top->handle))) {
            SetForegroundWindow(WH_TO_HWND(top->handle));
            SetFocus(WH_TO_HWND(top->handle));
            if (top->vtable && top->vtable->on_render) top->vtable->on_render(top);
        }
    }
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupHideWindow");
}

void Platform_EditPopupRender(Popup* self) {
    (void)self;
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupRender");
}

void Platform_EditPopupDestroyWindow(Popup* self) {
    PROFILE_START(timer);
    HWND hwnd = WH_TO_HWND(self->handle);
    if (hwnd && IsWindow(hwnd)) DestroyWindow(hwnd);
    PROFILE_END_VAR(timer, "window_manager:Platform_EditPopupDestroyWindow");
}

bool Platform_EditPopupOnKey(Popup* self, const PopupKeyEvent* key) {
    (void)self;
    (void)key;
    return false;
}
