#include "common.h"
#include "core/zoxide_helpers.h"
#include "core/common_ui.h"
#include "platform/platform.h"
#include "helpers.h"
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
    *cached_out = (char**)malloc(lineCount * sizeof(char*));
    *count_out = 0;
    for (size_t i = 0; i < lineCount; i++) {
        size_t len = lines[i].length;
        while (len > 0 && lines[i].ptr[len-1] == '\r') len--;
        if (len > 0) {
            char* cmd = (char*)malloc(len + 1);
            memcpy(cmd, lines[i].ptr, len);
            cmd[len] = '\0';
            (*cached_out)[(*count_out)++] = cmd;
        }
    }
    free(lines);
}

void ZoxideHelper_Refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb, MappedFile* file, char*** cached, int* count) {
    if (!*cached) ZoxideHelper_RebuildCache(file, cached, count);
    PopulateListBox(lb, hListBox, filter, (const char**)*cached, *count, NULL);
}

void ZoxideHelper_Edit(const char* selection, ListBoxInfo* lb, MappedFile* file, char*** cached, int* count, RefreshCallback refresh) {
    char* newQuery = Platform_EditTextPopup(selection, 960, 540, 1000, 300);
    if (newQuery != NULL) {
        WriteToCacheIndex(lb, lb->selected_index, newQuery);
    }
    Platform_ReplaceLineInMappedFile(file, lb->selected_index, newQuery);
    ZoxideHelper_RebuildCache(file, cached, count);
    if (refresh) refresh("", hListBox, lb);
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
