#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include "core/timer.h"
#include "platform/wm_platform.h"
#include "platform/ui.h"
#include <windows.h>
#include <stdlib.h>

bool Platform_PromptPopupShowWindow(Popup* self, int x, int y, int w, int h) {
    (void)self;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupShowWindow");
    return true;
}

void Platform_PromptPopupHideWindow(Popup* self) {
    (void)self;
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupHideWindow");
}

void Platform_PromptPopupRender(Popup* self) {
    (void)self;
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupRender");
}

void Platform_PromptPopupDestroyWindow(Popup* self) {
    (void)self;
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupDestroyWindow");
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
