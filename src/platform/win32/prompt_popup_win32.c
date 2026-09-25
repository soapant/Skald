#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include "core/timer.h"
#include "deps/log.h"
#include "platform/wm_platform.h"
#include "platform/ui.h"
#include "platform/win32/common_ui_win32.h"
#include <windows.h>
#include <stdlib.h>

bool Platform_PromptPopupShowWindow(Popup* self, int x, int y, int w, int h) {
    (void)self;
    PROFILE_START(timer);
    HINSTANCE hInstance = GetModuleHandle(NULL);
    FlintContext_Get()->window.hPopupWnd = WH_FROM_HWND(CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_COMPOSITED,
        "QuickSwitcherModalClass", NULL, WS_POPUP | WS_CLIPCHILDREN,
        x, y, w, h, NULL, NULL, hInstance, NULL
        ));
    if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) == NULL) {
        log_error("CreateWindowExA failed: %lu", GetLastError());
        PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupShowWindow");
        return false;
    }
    win32_apply_rounded_corners(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), w, h);
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupShowWindow");
    return true;
}

void Platform_PromptPopupHideWindow(Popup* self) {
    (void)self;
    PROFILE_START(timer);
    if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) {
        KillTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), 999);
        KillTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), UI_TICK_TIMER_ID);
        DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
        FlintContext_Get()->window.hPopupWnd = WH_NULL;
    }
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupHideWindow");
}

void Platform_PromptPopupRender(Popup* self) {
    (void)self;
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupRender");
}

void Platform_PromptPopupDestroyWindow(Popup* self) {
    Platform_PromptPopupHideWindow(self);
}

bool Platform_PromptPopupOnKey(Popup* self, const PopupKeyEvent* key) {
    (void)self;
    (void)key;
    return false;
}

void HandleCommandExecution(ListBoxInfo* lb, char* string) {
    if (!lb) return;
    int num = atoi(string);
    UI_SetSelection(lb, FlintContext_Get()->window.hListBox, num - 1);
    UI_LB_SetCaretIndex(FlintContext_Get()->window.hListBox, num, TRUE);
}
