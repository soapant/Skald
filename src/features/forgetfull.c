#include "core/flapi.h"

#define FORGETFULL_FILE_PATH "forgetfull.txt"

void forgetfull_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[100];
    int forgetfull_count = fl_load_file_list(FORGETFULL_FILE_PATH, items, 100);

    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, forgetfull_count, NULL);
    fl_free_file_list(items, forgetfull_count);
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
void forgetfull_edit_callback(const char* selection, ListBoxInfo* lb) {
    int idx = -1;
    char* edited = fl_edit_text_popup(.initial_text = selection,.lb = lb, .out_index = &idx);
    if (!edited) return;
    if (idx >= 0) fl_replace_line_in_file(FORGETFULL_FILE_PATH, idx, edited);
    free(edited);
    forgetfull_refresh_callback("", FlintContext_Get()->window.hListBox, lb);
    UI_Redraw(lb, FlintContext_Get()->window.hListBox);
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
    PROMPT(.title = "📁 ForgetFull",
           .refresh = forgetfull_refresh_callback,
           .enter = forgetfull_enter_callback,
           .ctrl_enter = forgetfull_ctrlenter_callback,
           .edit = forgetfull_edit_callback);
}

FLINT_FEATURE_INIT {
    fl_register_feature("ForgetFull", TriggerForgetFull);
}
