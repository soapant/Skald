#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "helpers.h"
#include "core/timer.h"
#include "core/common_ui.h"

#if defined(_WIN32)
#include <shlwapi.h>
#endif

bool IsWindowBlacklisted(const char* window_title, const char* blacklist[], int count) {
    for (int i = 0; i < count; i++) {
#if defined(_WIN32)
        if (StrStrIA(window_title, blacklist[i]) != NULL) {
#else
        if (strstr(window_title, blacklist[i]) != NULL) {
#endif
            return true;
        }
    }
    return false;
}

int UI_LookupTrueIndex(const char* name) {
    if (!FlintContext_Get()->g_item_index_map || !name) return -1;
    size_t value = (size_t)HashMap_Get(FlintContext_Get()->g_item_index_map, name);
    return value > 0 ? (int)(value - 1) : -1;
}

void SaveFeatureHistory(char* string) {
    char** HistoryList = NULL;
    int count = 0;
    
    HistoryList = (char**)HashMap_Get(FlintContext_Get()->feature_history, FlintContext_Get()->window.current_feature);
    while (HistoryList && HistoryList[count]) count++;
    
    // temporary i think
    // HistoryList = StringList_Append(HistoryList, &count, string);
    
    HashMap_Put(FlintContext_Get()->feature_history, FlintContext_Get()->window.current_feature, HistoryList);
}

bool GetFromHistory(ListBoxInfo* lb) {
    char** HistoryList = (char**)HashMap_Get(FlintContext_Get()->feature_history, FlintContext_Get()->window.current_feature);
    if (HistoryList) {
        int* idx_ptr = (int*)HashMap_Get(FlintContext_Get()->history_indices, FlintContext_Get()->window.current_feature);
        int idx = idx_ptr ? *idx_ptr : 0;
        
        int count = 0;
        while (HistoryList[count]) count++;
        
        if (count > 0) {
            idx = (idx - 1 + count) % count;
            
            UI_SetText(FlintContext_Get()->window.hEditControl, HistoryList[idx]);
            UI_SetCursorPosition(FlintContext_Get()->window.hEditControl,strlen(HistoryList[idx]));

            if (!IsNormalMode()) {
              UI_Redraw(lb, FlintContext_Get()->window.hListBox);
            }
            if (!idx_ptr) {
                idx_ptr = malloc(sizeof(int));
                if (!idx_ptr) return true;
                if (!FlintContext_Get()->history_indices) FlintContext_Get()->history_indices = HashMap_Create(32);
                if (!FlintContext_Get()->history_indices) { free(idx_ptr); return true; }
                HashMap_Put(FlintContext_Get()->history_indices, FlintContext_Get()->window.current_feature, idx_ptr);
                if (!HashMap_Get(FlintContext_Get()->history_indices, FlintContext_Get()->window.current_feature)) { free(idx_ptr); return true; }
            }
            *idx_ptr = idx;
            return true;
        } 
    }
    return false;
}

void HandleCommandExecution(ListBoxInfo* lb, char* string) {
    if (!lb) return;
    int num = atoi(string);
    UI_SetSelection(lb, FlintContext_Get()->window.hListBox, num - 1);
    UI_LB_SetCaretIndex(FlintContext_Get()->window.hListBox, num, TRUE);
}

const char* get_alias_for_item(const char* item) {
    if (!g_add_alias_to_string) return NULL;
    if (!FlintContext_Get()->window.current_feature) return NULL;
    if (!item) return NULL;

    for (size_t i = 0; i < ALIAS_ENTRY_COUNT; i++) {
        if (strcmp(Aliases[i].feature, FlintContext_Get()->window.current_feature) == 0) continue;
        const char* expanded = Aliases[i].text;
        if (!expanded || expanded[0]=='\0') expanded = Aliases[i].text;
        if (!expanded) continue;
        if (strcasecmp(item, expanded) == 0) {
            return Aliases[i].alias;
        }
    }
    return NULL;
}
