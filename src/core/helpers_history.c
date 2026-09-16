#include "core/context.h"
#include "deps/log.h"
#include "core/modal.h"
#include "config.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "config.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_file.h"
#include "helpers/helpers_ui.h"
#include "core/timer.h"
#include "platform/ui.h"

bool IsWindowBlacklisted(const char* window_title, const char* blacklist[], int count) {
    for (int i = 0; i < count; i++) {
        if (Platform_StringContainsCaseInsensitive(window_title, blacklist[i])) {
            return true;
        }
    }
    return false;
}

int UI_LookupTrueIndex(const char* name) {
    if (!FlintContext_Get()->g_item_index_map || !name) return -1;
    size_t value = (size_t)HashMap_Get(FlintContext_Get()->g_item_index_map, name);
    log_info("UI_LookupTrueIndex name='%s' raw=%zu", name, value);
    return value > 0 ? (int)(value - 1) : -1;
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

const char* get_alias_for_item(const char* item) {
    for (size_t i = 0; i < ALIAS_ENTRY_COUNT; i++) {
        const char* expanded = Aliases[i].text;
        if (!expanded || expanded[0]=='\0') expanded = Aliases[i].text;
        if (!expanded) continue;
        if (Platform_StringContainsCaseInsensitive(item, expanded)) {
            return Aliases[i].alias;
        }
    }
    return NULL;
}
