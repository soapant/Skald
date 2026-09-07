#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include "core/zoxide_helpers.h"
#include "core/common_ui.h"
#include "helpers.h"
#include "core/window_manager.h"
#include "core/flapi.h"
#include <stdlib.h>
#include <string.h>

void ZoxideHelper_RebuildCache(const char* filepath, char*** cached_out, int* count_out) {
    WinTimer timer = TimerStart();
    if (!cached_out || !count_out) return;
    if (*cached_out) {
        for (int i = 0; i < *count_out; i++) {
            free((*cached_out)[i]);
        }
        free(*cached_out);
        *cached_out = NULL;
        *count_out = 0;
    }
    if (!filepath) return;
    const char* tmp[MAX_FILE_LINES];
    int n = fl_load_file_list(filepath, tmp, MAX_FILE_LINES);
    if (n <= 0) return;
    char** new_cache = (char**)malloc((size_t)n * sizeof(char*));
    if (!new_cache) {
        fl_free_file_list(tmp, n);
        return;
    }
    for (int i = 0; i < n; i++) {
        new_cache[i] = (char*)tmp[i];
    }
    *cached_out = new_cache;
    *count_out = n;
    TimerEnd(timer,"Load File");
}

void ZoxideHelper_Refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb, const char* filepath, char*** cached, int* count) {
    if (!cached || !count) return;
    if (!*cached) ZoxideHelper_RebuildCache(filepath, cached, count);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, (const char**)*cached, *count, NULL);
}

static void zoxide_edit_confirm_callback(const char* new_text, void* user_data) {
    EditCallbackData* data = (EditCallbackData*)user_data;
    if (new_text && data) {
        data->new_text = (char*)new_text;
        EditCallback_Handle(data, data->refresh_cb);
    }
    free(data);
}

void ZoxideHelper_Edit(const char* selection, ListBoxInfo* lb, const char* filepath, char*** cached, int* count, RefreshCallback refresh) {
    (void)cached; (void)count;
    int idx = lb ? lb->selected_index : -1;
    int true_idx = (lb && idx >= 0 && idx < lb->populated_count) ? lb->matching_indices[idx] : idx;

    WindowManager* wm = WindowManager_Get();

    EditCallbackData* data = calloc(1, sizeof(EditCallbackData));

    data->filepath = filepath;
    data->lb = lb;
    data->selected_index = idx;
    data->true_index = true_idx;
    data->new_text = NULL;
    data->restore_selection = true;
    data->hListBox = FlintContext_Get()->window.hListBox;
    data->hEditControl = FlintContext_Get()->window.hEditControl;
    data->refresh_cb = refresh;

    Popup* popup = EditPopup_Create(wm, selection, "Edit Zoxide", 960, 540,
                                    zoxide_edit_confirm_callback, data);
    WindowManager_PushPopup(wm, popup);
}

void ZoxideHelper_AppendNewEntry(const char* filepath, const char* selection, ListBoxInfo* lb, char*** cached, int* count) {
    if (!filepath || !selection) return;
    if (UI_IsListBoxSelected(lb)) return;
    if (fl_append_to_file(filepath, selection)) {
        ZoxideHelper_RebuildCache(filepath, cached, count);
    }
}

const char** ZoxideHelper_PrepareTriggerList(const char* filepath, char*** cached, int* count, void (*rebuild)(void)) {
    (void)filepath;
    if (!cached || !count) return NULL;
    if (!*cached && rebuild) rebuild();
    const char** list = NULL;
    if (*count > 0) {
        list = (const char**)malloc((size_t)*count * sizeof(char*));
        if (!list) return NULL;
        for (int i = 0; i < *count; i++) list[i] = (*cached)[i];
    } else {
        list = (const char**)malloc(1);
    }
    return list;
}

void ZoxideHelper_CleanupTriggerList(const char** list) {
    if (list) free((void*)list);
}
