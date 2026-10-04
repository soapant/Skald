#include "core/context.h"
#include "deps/log.h"
#include "core/modal.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "config.h"
#include "platform/platform.h"
#include "helpers/helpers_alias.h"
#include "platform/ui.h"

int UI_LookupTrueIndex(const char* name) {
    if (!FlintContext_Get()->item_index_map || !name) return -1;
    ptrdiff_t idx = shgeti(FlintContext_Get()->item_index_map, (char*)name);
    size_t value = idx >= 0 ? (size_t)FlintContext_Get()->item_index_map[idx].value : 0;
    log_info("UI_LookupTrueIndex name='%s' raw=%zu", name, value);
    return value > 0 ? (int)(value - 1) : -1;
}


bool GetFromHistory(ListBoxInfo* lb) {
    ptrdiff_t hidx = shgeti(FlintContext_Get()->feature_history, FlintContext_Get()->window.current_feature);
    char** HistoryList = hidx >= 0 ? (char**)FlintContext_Get()->feature_history[hidx].value : NULL;
    if (HistoryList) {
        ptrdiff_t pidx = shgeti(FlintContext_Get()->history_indices, FlintContext_Get()->window.current_feature);
        int* idx_ptr = pidx >= 0 ? (int*)FlintContext_Get()->history_indices[pidx].value : NULL;
        int idx = idx_ptr ? *idx_ptr : 0;
        
        int count = 0;
        while (HistoryList[count]) count++;
        
        if (count > 0) {
            idx = (idx - 1 + count) % count;
            
            UI_SetText(HistoryList[idx]);
            UI_SetCursorPosition(strlen(HistoryList[idx]));

            if (!IsNormalMode()) {
              UI_Redraw(lb, FlintContext_Get()->window.hListBox);
            }
            if (!idx_ptr) {
                idx_ptr = malloc(sizeof(int));
                if (!idx_ptr) return true;
                if (!FlintContext_Get()->history_indices) { FlintContext_Get()->history_indices = NULL; sh_new_strdup(FlintContext_Get()->history_indices); }
                if (!FlintContext_Get()->history_indices) { free(idx_ptr); return true; }
                shput(FlintContext_Get()->history_indices, FlintContext_Get()->window.current_feature, idx_ptr);
                if (shgeti(FlintContext_Get()->history_indices, FlintContext_Get()->window.current_feature) < 0) { free(idx_ptr); return true; }
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
