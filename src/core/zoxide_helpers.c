#include "common.h"
#include "core/zoxide_helpers.h"
#include "core/common_ui.h"
#include "helpers.h"
#include "core/window_manager.h"
#include <stdlib.h>
#include <string.h>

void ZoxideHelper_RebuildCache(MappedFile* file, char*** cached_out, int* count_out) {
    if (!cached_out || !count_out) return;
    if (*cached_out) {
        for (int i = 0; i < *count_out; i++) {
            free((*cached_out)[i]);
        }
        free(*cached_out);
        *cached_out = NULL;
        *count_out = 0;
    }
    if (!file || file->fileSize == 0) return;
    size_t lineCount = 0;
    Line* lines = Platform_FileGetLines(file, &lineCount);
    if (!lines) return;
    char** new_cache = (char**)malloc(lineCount * sizeof(char*));
    if (!new_cache) {
#if !defined(_WIN32)
        for (size_t i = 0; i < lineCount; i++) {
            free(lines[i].ptr);
        }
#endif
        free(lines);
        return;
    }
    *cached_out = new_cache;
    *count_out = 0;
    for (size_t i = 0; i < lineCount; i++) {
        size_t len = lines[i].length;
        while (len > 0 && lines[i].ptr[len-1] == '\r') len--;
        if (len > 0) {
            char* cmd = (char*)malloc(len + 1);
            if (cmd) {
                memcpy(cmd, lines[i].ptr, len);
                cmd[len] = '\0';
                (*cached_out)[(*count_out)++] = cmd;
            }
        }
#if !defined(_WIN32)
        free(lines[i].ptr);
#endif
    }
    free(lines);
}

void ZoxideHelper_Refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb, MappedFile* file, char*** cached, int* count) {
    if (!*cached) ZoxideHelper_RebuildCache(file, cached, count);
    PopulateListBox(lb, hListBox, filter, (const char**)*cached, *count, NULL);
}

static void zoxide_edit_confirm_callback(const char* new_text, void* user_data) {
    EditCallbackData* data = (EditCallbackData*)user_data;
    if (new_text && data) {
        data->new_text = (char*)new_text;
        EditCallback_Handle(data, data->refresh_cb);
        Platform_ReplaceLineInMappedFile(data->file, (data->true_index >= 0) ? data->true_index : data->selected_index, new_text);
    }
    free(data);
}

void ZoxideHelper_Edit(const char* selection, ListBoxInfo* lb, MappedFile* file, char*** cached, int* count, RefreshCallback refresh) {
    int idx = lb ? lb->selected_index : -1;
    int true_idx = (lb && idx >= 0 && idx < lb->populated_count) ? lb->matching_indices[idx] : idx;
    
    WindowManager* wm = WindowManager_Get();
    if (!wm) return;
    
    extern WindowHandle hListBox;
    extern WindowHandle hEditControl;
    
    EditCallbackData* data = calloc(1, sizeof(EditCallbackData));
    if (!data) return;
    
    data->filepath = NULL;
    data->file = file;
    data->lb = lb;
    data->selected_index = idx;
    data->true_index = true_idx;
    data->new_text = NULL;
    data->restore_selection = true;
    data->hListBox = hListBox;
    data->hEditControl = hEditControl;
    data->refresh_cb = refresh;
    
    Popup* popup = EditPopup_Create(wm, selection, "Edit Zoxide", 960, 540, 1000, 300,
                                    zoxide_edit_confirm_callback, data);
    if (popup) {
        WindowManager_PushPopup(wm, popup);
    } else {
        free(data);
    }
    
    ZoxideHelper_RebuildCache(file, cached, count);
    if (refresh) refresh("", hListBox, lb);
    else if (lb) UI_Redraw(lb, hListBox);
}

void ZoxideHelper_AppendNewEntry(MappedFile* file, const char* selection, ListBoxInfo* lb, char*** cached, int* count) {
    if (!file || !selection) return;
    if (file->fileSize > 0) {
        char lastChar = Platform_GetLastChar(file);
        if (lastChar != '\n') {
            Platform_FileAppend(file, "\n");
        }
    }
    if (!UI_IsListBoxSelected(lb)) {
        Platform_FileAppend(file, selection);
        Platform_FileAppend(file, "\n");
        ZoxideHelper_RebuildCache(file, cached, count);
    }
}

const char** ZoxideHelper_PrepareTriggerList(MappedFile* file, char*** cached, int* count, void (*rebuild)(void)) {
    if (!cached || !count) return NULL;
    if (!*cached && rebuild) rebuild();
    const char** list = NULL;
    if (*count > 0) {
        list = (const char**)malloc(*count * sizeof(char*));
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
