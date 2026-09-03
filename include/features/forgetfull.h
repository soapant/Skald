#ifndef FORGETFULL_H
#define FORGETFULL_H

#include "core/flapi.h"

void AddTextToFile(const char* text) {
    if (!text || strlen(text) == 0) return;
    
    const char* lines[100];
    int count = fl_load_file_list(FORGETFULL_FILE_PATH, lines, 100);
    
    char new_entry[512];
    new_entry[0] = '\0';
    strncpy(new_entry, text, sizeof(new_entry) - 1);
    
    const char* to_write[101];
    for (int i = 0; i < count; i++) to_write[i] = lines[i];
    to_write[count] = new_entry;
    fl_write_to_file(FORGETFULL_FILE_PATH, to_write, count + 1);
    fl_free_file_list(lines, count);
}

void RemoveTextFromFile(const char* item_to_remove) {
    if (!item_to_remove) return;
    const char* lines[100];
    int count = fl_load_file_list(FORGETFULL_FILE_PATH, lines, 100);
    if (count == 0) return;
    const char* kept[100];
    int kept_count = 0;
    char clean_item[512];
    strncpy(clean_item, item_to_remove, sizeof(clean_item) - 1);
    clean_item[sizeof(clean_item) - 1] = '\0';
    clean_item[strcspn(clean_item, "\r\n")] = '\0';
    for (int i = 0; i < count; i++) {
        char clean_line[512];
        strncpy(clean_line, lines[i], sizeof(clean_line) - 1);
        clean_line[sizeof(clean_line) - 1] = '\0';
        clean_line[strcspn(clean_line, "\r\n")] = '\0';
        if (strcmp(clean_line, clean_item) == 0) {
            printf("[FORGETFULL-MGR] Dropped file line item: \"%s\"\n", item_to_remove);
            continue;
        }
        kept[kept_count++] = lines[i];
    }
    fl_write_to_file(FORGETFULL_FILE_PATH, kept, kept_count);
    fl_free_file_list(lines, count);
}

void forgetfull_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[100];
    int forgetfull_count = fl_load_file_list(FORGETFULL_FILE_PATH, items, 100);

    PopulateListBox(lb, hListBox, filter, items, forgetfull_count, NULL);
    fl_free_file_list(items, forgetfull_count);
}

void forgetfull_enter_callback(const char* selection, ListBoxInfo* lb) {
    if (UI_IsListBoxSelected(lb)) {
        RemoveTextFromFile(selection);
    } 
    else {
        AddTextToFile(selection);
        UI_SetText(hEditControl, "");
    }
    UI_Redraw(lb, hListBox);
}
void forgetfull_edit_confirm_callback(const char* new_text, void* user_data) {
    EditCallbackData* data = (EditCallbackData*)user_data;
    if (new_text && data) {
        data->new_text = (char*)new_text;
        EditCallback_Handle(data, forgetfull_refresh_callback);
    }
    free(data);
}

void forgetfull_edit_callback(const char* selection, ListBoxInfo* lb) {
    int idx = lb ? lb->selected_index : -1;
    int true_idx = (lb && idx >= 0 && idx < lb->populated_count) ? lb->matching_indices[idx] : idx;
    
    WindowManager* wm = WindowManager_Get();
    if (!wm) return;
    
    
    EditCallbackData* data = calloc(1, sizeof(EditCallbackData));
    if (!data) return;
    
    data->filepath = FORGETFULL_FILE_PATH;
    data->lb = lb;
    data->selected_index = idx;
    data->true_index = true_idx;
    data->new_text = NULL;
    data->restore_selection = true;
    data->hListBox = hListBox;
    data->hEditControl = hEditControl;
    
    Popup* popup = EditPopup_Create(wm, selection, "Edit ForgetFull", 960, 540,
                                    forgetfull_edit_confirm_callback, data);
    if (popup) {
        WindowManager_PushPopup(wm, popup);
    } else {
        free(data);
    }
}

void forgetfull_ctrlenter_callback(const char* selection, ListBoxInfo* lb) {
    if (UI_IsListBoxSelected(lb)) {
        RemoveTextFromFile(selection);
    } else {
        AddTextToFile(selection);
        UI_SetText(hEditControl, "");
    }
    forgetfull_refresh_callback(NULL, hListBox, lb);
    selection_complete = false;
}

void TriggerForgetFull(void) {
    const char* ui_list[100];
    int count = fl_load_file_list(FORGETFULL_FILE_PATH, ui_list, 100);

    PROMPT(.title = "📁 ForgetFull", .items = ui_list, .count = count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = forgetfull_refresh_callback,
           .enter = forgetfull_enter_callback,
           .ctrl_enter = forgetfull_ctrlenter_callback,
           .edit = forgetfull_edit_callback);
    fl_free_file_list(ui_list, count);
}

#endif
