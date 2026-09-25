#include "core/context.h"
#include "deps/log.h"
#include "core/window_manager.h"
#include "platform/platform.h"
#include "core/timer.h"
#include "platform/wm_platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static WindowManager* g_window_manager = NULL;

void SavePerFeatureState(const char* feature, EditorMode mode, int last_index) {
    PROFILE_START(timer);
    (void)feature; (void)mode; (void)last_index;
    PROFILE_END_VAR(timer, "window_manager:SavePerFeatureState");
}

WindowManager* WindowManager_Create(void) {
    PROFILE_START(timer);
    WindowManager* wm = calloc(1, sizeof(WindowManager));
    if (!wm) { PROFILE_END_VAR(timer, "window_manager:WindowManager_Create"); return NULL; }
    wm->popup_capacity = 8;
    wm->popups = calloc(wm->popup_capacity, sizeof(Popup*));
    if (!wm->popups) {
        free(wm);
        PROFILE_END_VAR(timer, "window_manager:WindowManager_Create");
        return NULL;
    }
    if (!Platform_WM_InitDisplay(wm)) {
        free(wm->popups);
        free(wm);
        PROFILE_END_VAR(timer, "window_manager:WindowManager_Create");
        return NULL;
    }
    Platform_ApplyUIScale();
    wm->ctx = FlintContext_Get();
    wm->watcher_stop = false;
    g_window_manager = wm;
    Platform_WatcherStart(wm);
    PROFILE_END_VAR(timer, "window_manager:WindowManager_Create");
    return wm;
}

void WindowManager_Destroy(WindowManager* wm) {
    PROFILE_START(timer);
    if (!wm) { PROFILE_END_VAR(timer, "window_manager:WindowManager_Destroy"); return; }
    wm->watcher_stop = true;
    Platform_WatcherStop(wm);
    for (int i = 0; i < wm->popup_count; i++) {
        if (wm->popups[i] && wm->popups[i]->vtable && wm->popups[i]->vtable->destroy) {
            wm->popups[i]->vtable->destroy(wm->popups[i]);
        }
    }
    free(wm->popups);
    if (g_window_manager == wm) g_window_manager = NULL;
    free(wm);
    log_info("WindowManager_Destroy completed");
    PROFILE_END_VAR(timer, "window_manager:WindowManager_Destroy");
}

WindowManager* WindowManager_Get(void) {
    PROFILE_START(timer);
    PROFILE_END_VAR(timer, "window_manager:WindowManager_Get");
    return g_window_manager;
}

int WindowManager_Run(WindowManager* wm) {
    PROFILE_START(timer);
    if (!wm) { PROFILE_END_VAR(timer, "window_manager:WindowManager_Run"); return -1; }
    int rc = Platform_WM_Run(wm);
    PROFILE_END_VAR(timer, "window_manager:WindowManager_Run");
    return rc;
}

void WindowManager_Quit(WindowManager* wm) {
    PROFILE_START(timer);
    if (!wm) { PROFILE_END_VAR(timer, "window_manager:WindowManager_Quit"); return; }
    Platform_WM_Quit(wm);
    PROFILE_END_VAR(timer, "window_manager:WindowManager_Quit");
}

Popup* WindowManager_PushPopup(WindowManager* wm, Popup* popup) {
    PROFILE_START(timer);
    if (!wm || !popup) { PROFILE_END_VAR(timer, "window_manager:WindowManager_PushPopup"); return NULL; }
    if (wm->popup_count >= wm->popup_capacity) {
        int new_cap = wm->popup_capacity * 2;
        Popup** new_popups = realloc(wm->popups, new_cap * sizeof(Popup*));
        if (!new_popups) { PROFILE_END_VAR(timer, "window_manager:WindowManager_PushPopup"); return NULL; }
        wm->popups = new_popups;
        wm->popup_capacity = new_cap;
    }
    popup->manager = wm;
    popup->visible = true;
    wm->popups[wm->popup_count++] = popup;
    if (popup->vtable && popup->vtable->on_show) {
        popup->vtable->on_show(popup);
    }
    PROFILE_END_VAR(timer, "window_manager:WindowManager_PushPopup");
    return popup;
}

void WindowManager_PopPopup(WindowManager* wm) {
    if (!wm || wm->popup_count == 0) return;
    Popup* popup = wm->popups[--wm->popup_count];
    if (popup->vtable && popup->vtable->on_hide) {
        popup->vtable->on_hide(popup);
    }
    popup->visible = false;
    popup->manager = NULL;
    if (wm->popup_count == 0) {
        Platform_WM_OnPopEmpty(wm);
    }
}

Popup* WindowManager_GetTopPopup(WindowManager* wm) {
    if (!wm || wm->popup_count == 0) return NULL;
    return wm->popups[wm->popup_count - 1];
}

void WindowManager_GetMainMonitorGeometry(int *ox, int *oy, int *ow, int *oh) {
    Platform_WM_GetMonitorGeometry(ox, oy, ow, oh);
}

void WindowManager_GetCenteredPos(int win_w, int win_h, int *out_x, int *out_y) {
    int mox, moy, mow, moh;
    WindowManager_GetMainMonitorGeometry(&mox, &moy, &mow, &moh);
    if (out_x) *out_x = mox + (mow - win_w) / 2;
    if (out_y) *out_y = moy + (moh - win_h) / 2;
}

char* WindowManager_ShowPrompt(WindowManager* wm, const PromptSpec* spec) {
    PROFILE_START(timer);
    if (!wm || !spec) { PROFILE_END_VAR(timer, "window_manager:WindowManager_ShowPrompt"); return NULL; }
    CloseCallback on_close = spec->on_close;
    if (FlintContext_Get()->item_index_map) {
        shfree(FlintContext_Get()->item_index_map);
        FlintContext_Get()->item_index_map = NULL;
    }
    if (spec->items) {
        int count = 0;
        for (count = 0; spec->items[count]; count++) {}
        if (count > 0) {
            FlintContext_Get()->item_index_map = NULL;
            sh_new_strdup(FlintContext_Get()->item_index_map);
            for (int i = 0; i < count; i++) {
                if (spec->items[i]) shput(FlintContext_Get()->item_index_map, (char*)spec->items[i], (void*)(size_t)(i + 1));
            }
        }
    }
    char* res = Platform_PromptUser(wm, spec);
    if (on_close) on_close();
    PROFILE_END_VAR(timer, "window_manager:WindowManager_ShowPrompt");
    return res;
}

