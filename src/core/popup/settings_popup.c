#include "core/context.h"
#include "core/window_manager.h"
#include "platform/platform.h"
#include "platform/wm_platform.h"
#include "core/timer.h"
#include <stdlib.h>

typedef struct {
    Popup base;
    int x, y, w, h;
} SettingsPopupData;

static void SettingsPopup_OnShow(Popup* self) {
    PROFILE_START(timer);
    SettingsPopupData* data = (SettingsPopupData*)self;
    FlintContext_Get()->window.selection_complete = false;
    Platform_SettingsPopupShowWindow(self, data->x, data->y, data->w, data->h);
    PROFILE_END_VAR(timer, "window_manager:SettingsPopup_OnShow");
}

static void SettingsPopup_OnHide(Popup* self) {
    PROFILE_START(timer);
    Platform_SettingsPopupHideWindow(self);
    PROFILE_END_VAR(timer, "window_manager:SettingsPopup_OnHide");
}

static void SettingsPopup_OnRender(Popup* self) {
    PROFILE_START(timer);
    Platform_SettingsPopupRender(self);
    PROFILE_END_VAR(timer, "window_manager:SettingsPopup_OnRender");
}

static bool SettingsPopup_OnKey(Popup* self, const PopupKeyEvent* key) {
    return Platform_SettingsPopupOnKey(self, key);
}

static void SettingsPopup_Destroy(Popup* self) {
    PROFILE_START(timer);
    Platform_SettingsPopupDestroyWindow(self);
    free(self);
    PROFILE_END_VAR(timer, "window_manager:SettingsPopup_Destroy");
}

static const PopupVTable g_settings_popup_vtable = {
    .on_show = SettingsPopup_OnShow,
    .on_hide = SettingsPopup_OnHide,
    .on_render = SettingsPopup_OnRender,
    .on_key = SettingsPopup_OnKey,
    .destroy = SettingsPopup_Destroy
};

Popup* SettingsPopup_Create(WindowManager* wm) {
    PROFILE_START(timer);
    if (!wm) { PROFILE_END_VAR(timer, "window_manager:SettingsPopup_Create"); return NULL; }
    int w = (int)(900 * Platform_GetUIScale());
    int h = (int)(700 * Platform_GetUIScale());
    int x, y;
    WindowManager_GetCenteredPos(w, h, &x, &y);
    SettingsPopupData* data = calloc(1, sizeof(SettingsPopupData));
    if (!data) { PROFILE_END_VAR(timer, "window_manager:SettingsPopup_Create"); return NULL; }
    data->base.vtable = &g_settings_popup_vtable;
    data->base.manager = wm;
    data->base.type = POPUP_SETTINGS;
    data->x = x; data->y = y; data->w = w; data->h = h;
    PROFILE_END_VAR(timer, "window_manager:SettingsPopup_Create");
    return WindowManager_PushPopup(wm, (Popup*)data);
}
