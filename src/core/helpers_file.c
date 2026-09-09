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
#include "core/window_manager.h"

int ReplaceLineInFile(const char* filepath, int line_index, const char* new_text) {
    fprintf(stderr, "[REPLACE] file='%s' idx=%d new='%s'\n", filepath ? filepath : "(null)", line_index, new_text ? new_text : "(null)");
    if (!filepath || !new_text || line_index < 0) {
        fprintf(stderr, "[REPLACE] early return 0\n");
        return 0;
    }
    FILE* f = fopen(filepath, "r");
    if (!f) {
        return 0;
    }
    char (*lines)[MAX_FILE_LINE_LENGTH] = malloc(MAX_FILE_LINES * sizeof(*lines));
    if (!lines) { fclose(f); return 0; }
    int count = 0;
    while (count < MAX_FILE_LINES && fgets(lines[count], sizeof(lines[count]), f)) {
        lines[count][strcspn(lines[count], "\r\n")] = '\0';
        count++;
    }
    fclose(f);
    if (line_index >= count) {
        free(lines);
        return 0;
    }
    strncpy(lines[line_index], new_text, sizeof(lines[line_index]) - 1);
    lines[line_index][sizeof(lines[line_index]) - 1] = '\0';
    f = fopen(filepath, "w");
    if (!f) {
        free(lines);
        return 0;
    }
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s\n", lines[i]);
    }
    fclose(f);
    free(lines);
    return 1;
}

void WriteToCacheIndex(ListBoxInfo* lb, int index, const char* text) {
    if (!lb) return;
    if (index < 0 || index >= MAX_ITEMS || !text) return;
    
    strncpy(lb->populated_cache[index], text, sizeof(lb->populated_cache[index]) - 1);
    lb->populated_cache[index][sizeof(lb->populated_cache[index]) - 1] = '\0';
}

int EditCallback_Handle(EditCallbackData* data, RefreshCallback refresh) {
    if (!data || !data->new_text) return 0;
    
    if (data->selected_index >= 0 && data->lb) {
        WriteToCacheIndex(data->lb, data->selected_index, data->new_text);
    }
    
    int rc = 0;
    if (data->true_index >= 0 && data->filepath) {
        rc = ReplaceLineInFile(data->filepath, data->true_index, data->new_text);
    } else if (data->selected_index >= 0 && data->filepath) {
        rc = ReplaceLineInFile(data->filepath, data->selected_index, data->new_text);
    }
    
    if (refresh && data->lb) {
        refresh("", data->hListBox, data->lb);
    }
    if (data->lb) {
        UI_Redraw(data->lb, data->hListBox);
    }
    
    if (data->restore_selection && data->lb && data->selected_index >= 0) {
        UI_NavigateTo(data->lb, data->hListBox, data->selected_index);
    }
    
    return rc;
}

