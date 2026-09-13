#include "core/flapi.h"

const char* TerminalHistoryFilePath = "data/TerminalHistory.txt";

void terminal_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(TerminalHistoryFilePath, items, MAX_FILE_LINES);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, n, NULL);
}

static void edit_confirm(const char* new_text, void* user_data) {
    EditCallbackData* data = (EditCallbackData*)user_data;
    if (new_text && data) {
        data->new_text = (char*)new_text;
        EditCallback_Handle(data, terminal_refresh_callback);
    }
    free(data);
}

void terminal_edit_callback(const char* selection, ListBoxInfo* lb) {
    int idx = lb ? lb->selected_index : -1;
    int true_idx = (lb && idx >= 0 && idx < lb->populated_count) ? lb->matching_indices[idx] : idx;
    WindowManager* wm = WindowManager_Get();
    EditCallbackData* data = calloc(1, sizeof(EditCallbackData));
    if (!data) return;
    data->filepath = TerminalHistoryFilePath;
    data->lb = lb;
    data->selected_index = idx;
    data->true_index = true_idx;
    data->new_text = NULL;
    data->restore_selection = true;
    data->hListBox = FlintContext_Get()->window.hListBox;
    data->hEditControl = FlintContext_Get()->window.hEditControl;
    data->refresh_cb = terminal_refresh_callback;
    Popup* popup = EditPopup_Create(wm, selection, "Edit Terminal", 960, 540, edit_confirm, data);
    WindowManager_PushPopup(wm, popup);
}

bool terminal_enter_callback(const char* selection, ListBoxInfo* lb) {
      if (!UI_IsListBoxSelected(lb)) {
        fl_append_unique_line(TerminalHistoryFilePath, selection);
      }
      fl_run_terminal_command_and_open((char*)selection);
    return true;
}

void TriggerTerminalCommand(void) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(TerminalHistoryFilePath, items, MAX_FILE_LINES);
    PROMPT(.title = "⌨️ Terminal", .items = items,
           .refresh = terminal_refresh_callback,
           .enter = terminal_enter_callback,
           .edit = terminal_edit_callback);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+{", TriggerTerminalCommand);

    fl_register_feature("Terminal", TriggerTerminalCommand);
}
