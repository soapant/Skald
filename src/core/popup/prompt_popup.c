#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include "platform/platform.h"
#include "core/feature_data.h"
#include "config.h"
#include "core/history.h"
#include "platform/wm_platform.h"
#include "core/timer.h"
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Popup base;
    PromptSpec spec;
    PromptState* state;
    int x, y, w, h;
} PromptPopupData;

static const char* prompt_feature_name(const char* title) {
    if (!title || !title[0]) return title;
    const char* p = strchr(title, ' ');
    if (!p) return title;
    for (const char* q = title; q < p; q++) {
        if ((unsigned char)*q >= 0x80) return p + 1;
    }
    return title;
}

static void PromptPopup_OnShow(Popup* self) {
    PROFILE_START(timer);
    PromptPopupData* data = (PromptPopupData*)self;
    WindowManager* wm = (WindowManager*)self->manager;
    FlintContext* ctx = FlintContext_Get();
    ctx->window.selection_complete = false;
    ctx->window.output_buffer[0] = '\0';

    Platform_PromptPopupShowWindow(self, data->x, data->y, data->w, data->h);

    data->state = calloc(1, sizeof(PromptState));
    if (!data->state) { PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnShow"); return; }

    data->state->items = (char**)data->spec.items;
    data->state->item_count = 0;
    data->state->cursor = 0;
    data->state->width = data->w;
    data->state->height = data->h;
    const char* title = data->spec.title ? data->spec.title : "Flint";
    data->state->window_title = (char*)prompt_feature_name(title);
    data->state->filter[0] = '\0';
    FlintContext_Get()->window.filter[0] = '\0';
    FlintContext_Get()->window.filter_cursor = 0;
    if (data->spec.items || data->spec.refresh) {
      data->state->has_listbox = true;
    } else {
      data->state->has_listbox = false;
    }
    ListBoxInfo_Init(&data->state->listbox);
    data->state->listbox.selected_index = -1;
    extern EditorMode g_last_mode;
    if (ModePersistsThroughFeatures) {
        SetModeWithListBox(g_last_mode, &data->state->listbox);
        if (RememberLastIndex && last_index >= 0 && data->state->has_listbox) {
            data->state->listbox.selected_index = last_index;
        }
    } else {
        SetModeWithListBox(MODE_INSERT, &data->state->listbox);
    }
    self->user_data = data->state;

    FlintContext* ctx2 = FlintContext_Get();
    if (!wm->ctx) wm->ctx = ctx2;
    ctx2->prompt_callbacks.refresh = data->spec.refresh;
    ctx2->prompt_callbacks.enter = data->spec.enter;
    ctx2->prompt_callbacks.ctrl_enter = data->spec.ctrl_enter;
    ctx2->prompt_callbacks.tab = data->spec.tab;
    ctx2->prompt_callbacks.copy = data->spec.copy;
    ctx2->prompt_callbacks.render = data->spec.render;
    ctx2->prompt_callbacks.edit = data->spec.edit;
    ctx2->prompt_callbacks.filter = data->spec.filter;
    ctx2->prompt_callbacks.help = data->spec.help;
    ctx2->window.current_feature = (char*)prompt_feature_name(title);
    ctx2->window.title = title;
    ctx2->window.callbacks = ctx2->prompt_callbacks;
    SetCurrentFeatureName(ctx2->window.current_feature);
    History_ResetCursor(ctx2->window.current_feature);
    if (data->spec.refresh) data->spec.refresh(data->state->filter, FlintContext_Get()->window.hListBox, data->state->has_listbox ? &data->state->listbox : NULL);
    PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnShow");
}

static void PromptPopup_OnHide(Popup* self) {
    PROFILE_START(timer);
    PromptPopupData* data = (PromptPopupData*)self;
    Platform_PromptPopupHideWindow(self);
    extern EditorMode g_last_mode;
    extern void SavePerFeatureState(const char* feature, EditorMode mode, int last_index);
    if (data->state) {
        if (!KeepSelectionAfterClose) {
            last_index = 0;
            if (data->state->has_listbox) data->state->listbox.selected_index = 0;
        } else {
            last_index = data->state->listbox.selected_index;
        }
        g_last_mode = g_current_mode;
        FlintContext* _c3 = FlintContext_Get();
        const char* feat = _c3->window.current_feature;
        if (ModePersistsThroughFeatures && feat) {
            SavePerFeatureState(feat, g_current_mode, last_index);
        }
        _c3->window.current_feature = NULL;
    }
    PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnHide");
}

static void PromptPopup_OnRender(Popup* self) {
    PROFILE_START(timer);
    PromptPopupData* data = (PromptPopupData*)self;
    if (!data->state) { PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnRender"); return; }
    Platform_PromptPopupRender(self);
    PROFILE_END_VAR(timer, "window_manager:PromptPopup_OnRender");
}

static bool PromptPopup_OnKey(Popup* self, const PopupKeyEvent* key) {
    return Platform_PromptPopupOnKey(self, key);
}

static void PromptPopup_Destroy(Popup* self) {
    PROFILE_START(timer);
    PromptPopupData* data = (PromptPopupData*)self;
    Platform_PromptPopupDestroyWindow(self);
    free(data->state);
    free(data);
    PROFILE_END_VAR(timer, "window_manager:PromptPopup_Destroy");
}

static const PopupVTable g_prompt_popup_vtable = {
    .on_show = PromptPopup_OnShow,
    .on_hide = PromptPopup_OnHide,
    .on_render = PromptPopup_OnRender,
    .on_key = PromptPopup_OnKey,
    .destroy = PromptPopup_Destroy
};

Popup* PromptPopup_Create(WindowManager* wm, const PromptSpec* spec) {
    PROFILE_START(timer);
    if (!wm || !spec) { PROFILE_END_VAR(timer, "window_manager:PromptPopup_Create"); return NULL; }

    int layout_w = (int)(900 * Platform_GetUIScale());
    int layout_h = (int)(640 * Platform_GetUIScale());

    int x, y;
    WindowManager_GetCenteredPos(layout_w, layout_h, &x, &y);

    PromptPopupData* data = calloc(1, sizeof(PromptPopupData));
    if (!data) { PROFILE_END_VAR(timer, "window_manager:PromptPopup_Create"); return NULL; }

    data->base.vtable = &g_prompt_popup_vtable;
    data->base.manager = wm;
    data->base.type = POPUP_PROMPT;
    data->spec = *spec;
    data->x = x; data->y = y; data->w = layout_w; data->h = layout_h;

    PROFILE_END_VAR(timer, "window_manager:PromptPopup_Create");
    return WindowManager_PushPopup(wm, (Popup*)data);
}

bool WindowManager_TopPopupWantsTick(WindowManager* wm) {
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top || top->type != POPUP_PROMPT) return false;
    PromptPopupData* data = (PromptPopupData*)top;
    return data->spec.tick;
}
