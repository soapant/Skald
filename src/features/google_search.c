#include "core/flapi.h"

const char* GoogleHistoryFile = "data/GoogleZoxide.txt";

void google_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(GoogleHistoryFile, items, MAX_FILE_LINES);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, n, NULL);
    fl_free_file_list(items, n);
}

static void edit_confirm(const char* new_text, void* user_data) {
    EditCallbackData* data = (EditCallbackData*)user_data;
    if (new_text && data) {
        data->new_text = (char*)new_text;
        EditCallback_Handle(data, google_refresh_callback);
    }
    free(data);
}
void google_edit_callback(const char* selection, ListBoxInfo* lb) {
    int idx = lb ? lb->selected_index : -1;
    int true_idx = (lb && idx >= 0 && idx < lb->populated_count) ? lb->matching_indices[idx] : idx;
    WindowManager* wm = WindowManager_Get();
    EditCallbackData* data = calloc(1, sizeof(EditCallbackData));
    if (!data) return;
    data->filepath = GoogleHistoryFile;
    data->lb = lb;
    data->selected_index = idx;
    data->true_index = true_idx;
    data->new_text = NULL;
    data->restore_selection = true;
    data->hListBox = FlintContext_Get()->window.hListBox;
    data->hEditControl = FlintContext_Get()->window.hEditControl;
    data->refresh_cb = google_refresh_callback;
    Popup* popup = EditPopup_Create(wm, selection, "Edit Google", 960, 540, edit_confirm, data);
    WindowManager_PushPopup(wm, popup);
}

bool google_enter_callback(const char* selection, ListBoxInfo* lb) {
    if (!UI_IsListBoxSelected(lb)) {
        fl_append_unique_line(GoogleHistoryFile, selection);
    }
    fl_query_browser(selection);
    return true;
}

void TriggerGoogleSearch(void) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(GoogleHistoryFile, items, MAX_FILE_LINES);
    PROMPT(.title = "🔍 Google", .items = items,
           .refresh = google_refresh_callback,
           .enter = google_enter_callback,
           .edit = google_edit_callback);
    fl_free_file_list(items, n);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+G", TriggerGoogleSearch);
    fl_register_feature("Google Search", TriggerGoogleSearch);
}
