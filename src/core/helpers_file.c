#include "common.h"
#include "config.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "helpers.h"
#include "core/timer.h"
#include "core/common_ui.h"

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
    
    char lines[100][512];
    int count = 0;
    while (fgets(lines[count], sizeof(lines[count]), f) && count < 100) {
        lines[count][strcspn(lines[count], "\r\n")] = '\0';
        count++;
    }
    fclose(f);
    
    if (line_index >= count) {
        return 0;
    }
    
    strncpy(lines[line_index], new_text, sizeof(lines[line_index]) - 1);
    lines[line_index][sizeof(lines[line_index]) - 1] = '\0';
    
    f = fopen(filepath, "w");
    if (!f) {
        return 0;
    }
    
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s\n", lines[i]);
    }
    fclose(f);
    
    return 1;
}

void WriteToCacheIndex(ListBoxInfo* lb, int index, const char* text) {
    if (!lb) return;
    if (index < 0 || index >= MAX_ITEMS || !text) return;
    
    strncpy(lb->populated_cache[index], text, sizeof(lb->populated_cache[index]) - 1);
    lb->populated_cache[index][sizeof(lb->populated_cache[index]) - 1] = '\0';
}

int ReadMdFile(char lines[100][512], const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        return 0;
    }
    
    int count = 0;
    char buffer[512];
    
    while (fgets(buffer, sizeof(buffer), f) && count < 100) {
        buffer[strcspn(buffer, "\r\n")] = '\0';
        
        if (strlen(buffer) > 0) {
            strncpy(lines[count], buffer, 511);
            lines[count][511] = '\0';
            
            count++;
        } 
    }
    
    fclose(f);
    return count;
}

void WriteMdFile(char lines[100][512], int count, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s\n", lines[i]);
    }
    fclose(f);
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
    
    // Restore selection if requested
    if (data->restore_selection && data->lb && data->selected_index >= 0) {
        UI_NavigateTo(data->lb, data->hListBox, data->selected_index);
    }
    
    return rc;
}
