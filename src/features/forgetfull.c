#include "core/flapi.h"

void forgetfull_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[100];
    int forgetfull_count = fl_load_file_list(FORGETFULL_FILE_PATH, items, 100);

    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, forgetfull_count, NULL);
}

bool forgetfull_enter_callback(const char* selection, ListBoxInfo* lb) {
    if (UI_IsListBoxSelected(lb)) {
        fl_remove_line(FORGETFULL_FILE_PATH,selection);
    } 
    else {
        fl_append_unique_line(FORGETFULL_FILE_PATH,selection);
        UI_SetText(FlintContext_Get()->window.hEditControl, "");
    }
    UI_Redraw(lb, FlintContext_Get()->window.hListBox);
    return true;
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
    data->hListBox = FlintContext_Get()->window.hListBox;
    data->hEditControl = FlintContext_Get()->window.hEditControl;
    
    Popup* popup = EditPopup_Create(wm, selection, "Edit ForgetFull", 960, 540,
                                    forgetfull_edit_confirm_callback, data);
    if (popup) {
        WindowManager_PushPopup(wm, popup);
    } else {
        free(data);
    }
}

bool forgetfull_ctrlenter_callback(const char* selection, ListBoxInfo* lb) {
    if (UI_IsListBoxSelected(lb)) {
        fl_remove_line(FORGETFULL_FILE_PATH,selection);
    } else {
        fl_append_unique_line(FORGETFULL_FILE_PATH,selection);
        UI_SetText(FlintContext_Get()->window.hEditControl, "");
    }
    forgetfull_refresh_callback(NULL, FlintContext_Get()->window.hListBox, lb);
    return false;
}

void TriggerForgetFull(void) {
    const char* ui_list[100];
    int count = fl_load_file_list(FORGETFULL_FILE_PATH, ui_list, 100);

    PROMPT(.title = "📁 ForgetFull", .items = ui_list,
           .refresh = forgetfull_refresh_callback,
           .enter = forgetfull_enter_callback,
           .ctrl_enter = forgetfull_ctrlenter_callback,
           .edit = forgetfull_edit_callback);
}

FLINT_FEATURE_INIT {
    fl_register_feature("ForgetFull", TriggerForgetFull);
}
