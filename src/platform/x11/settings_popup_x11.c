#include "core/platform.h"
#include "platform/wm_platform.h"
#include "core/timer.h"

void Platform_SettingsPopUp(void) {
}

bool Platform_SettingsPopupShowWindow(Popup* self, int x, int y, int w, int h) {
    (void)self;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "window_manager:Platform_SettingsPopupShowWindow");
    return true;
}

void Platform_SettingsPopupHideWindow(Popup* self) {
    (void)self;
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "window_manager:Platform_SettingsPopupHideWindow");
}

void Platform_SettingsPopupRender(Popup* self) {
    (void)self;
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "window_manager:Platform_SettingsPopupRender");
}

void Platform_SettingsPopupDestroyWindow(Popup* self) {
    (void)self;
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "window_manager:Platform_SettingsPopupDestroyWindow");
}

bool Platform_SettingsPopupOnKey(Popup* self, const PopupKeyEvent* key) {
    (void)self;
    (void)key;
    return false;
}
