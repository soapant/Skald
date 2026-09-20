#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include "core/platform.h"
#include "platform/ui.h"
#include "core/feature_data.h"
#include "core/vim.h"
#include "core/prompt_controller.h"
#include "config.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_ui.h"
#include "core/feature_loader.h"
#include "platform/wm_platform.h"
#include "core/timer.h"
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef WMEditPopupData EditPopupDataInternal;

static void EditPopup_OnShow(Popup* self) {
    PROFILE_START(timer);
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
    data->saved_mode = g_current_mode;
    SetMode(MODE_INSERT);
    Platform_EditPopupShowWindow(self);
    PROFILE_END_VAR(timer, "window_manager:EditPopup_OnShow");
}

static void EditPopup_OnHide(Popup* self) {
    PROFILE_START(timer);
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
    SetMode(data->saved_mode);
    Platform_EditPopupHideWindow(self);
    PROFILE_END_VAR(timer, "window_manager:EditPopup_OnHide");
}

static void EditPopup_OnRender(Popup* self) {
    PROFILE_START(timer);
    Platform_EditPopupRender(self);
    PROFILE_END_VAR(timer, "window_manager:EditPopup_OnRender");
}

static void EditPopup_Destroy(Popup* self) {
    PROFILE_START(timer);
    EditPopupDataInternal* data = (EditPopupDataInternal*)self;
    free(data->text);
    free(data->title);
    free(data->align);
    Platform_EditPopupDestroyWindow(self);
    free(data);
    PROFILE_END_VAR(timer, "window_manager:EditPopup_Destroy");
}

static const PopupVTable g_edit_popup_vtable = {
    .on_show = EditPopup_OnShow,
    .on_hide = EditPopup_OnHide,
    .on_render = EditPopup_OnRender,
    .destroy = EditPopup_Destroy
};

Popup* EditPopup_Create(WindowManager* wm, const char* initial_text,
                        const char* title, int x, int y, int w, int h, const char* align,
                        EditPopupConfirmCallback on_confirm, void* user_data) {
    PROFILE_START(timer);
    EditPopupDataInternal* data = calloc(1, sizeof(EditPopupDataInternal));
    if (!data) { PROFILE_END_VAR(timer, "window_manager:EditPopup_Create"); return NULL; }
    WindowManager_GetCenteredPos(w, h, &x, &y);

    data->base.vtable = &g_edit_popup_vtable;
    data->base.manager = wm;
    data->base.type = POPUP_EDIT;
    data->title = title ? strdup(title) : strdup("");
    data->align = align ? strdup(align) : strdup("LEFT");
    if (!data->title || !data->align) {
        free(data->title);
        free(data->align);
        free(data);
        PROFILE_END_VAR(timer, "window_manager:EditPopup_Create"); return NULL;
    }
    data->text_capacity = 1024;
    data->text = calloc(data->text_capacity, 1);
    if (!data->text) {
        free(data->title);
        free(data->align);
        free(data);
        PROFILE_END_VAR(timer, "window_manager:EditPopup_Create"); return NULL;
    }
    if (initial_text) {
        strncpy(data->text, initial_text, data->text_capacity - 1);
        data->cursor = strlen(data->text);
    }
    data->on_confirm = on_confirm;
    data->user_data = user_data;
    data->x = x; data->y = y; data->w = w; data->h = h;
    data->confirmed = false;

    if (!Platform_EditPopupCreateWindow((Popup*)data, x, y, w, h, title)) {
        free(data->text);
        free(data->title);
        free(data->align);
        free(data);
        PROFILE_END_VAR(timer, "window_manager:EditPopup_Create");
        return NULL;
    }

    PROFILE_END_VAR(timer, "window_manager:EditPopup_Create");
    return (Popup*)data;
}
