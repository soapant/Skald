#include "core/flapi.h"

const char* GoogleHistoryFile = "GoogleZoxide.txt";

void google_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(GoogleHistoryFile, items, MAX_FILE_LINES);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, n, NULL);
    fl_free_file_list(items, n);
}

void google_edit_callback(const char* selection, ListBoxInfo* lb) {
    int idx = -1;
    char* edited = fl_edit_text_popup(.initial_text = selection, .title = "Edit Google", .x = 960, .y = 540, .lb = lb, .out_index = &idx);
    if (!edited) return;
    if (idx >= 0) fl_replace_line_in_file(GoogleHistoryFile, idx, edited);
    free(edited);
    google_refresh_callback("", FlintContext_Get()->window.hListBox, lb);
    UI_Redraw(lb, FlintContext_Get()->window.hListBox);
}

bool google_enter_callback(const char* selection, ListBoxInfo* lb) {
    if (!UI_IsListBoxSelected(lb)) {
        fl_append_unique_line(GoogleHistoryFile, selection);
    }
    fl_query_browser(selection);
    return true;
}

bool google_ctrlenter_callback(const char* selection, ListBoxInfo* lb) {
    if (!UI_IsListBoxSelected(lb)) {
        fl_append_unique_line(GoogleHistoryFile, selection);
    }
    fl_launch_browser(selection);
    return true;
}

void TriggerGoogleSearch(void) {
    PROMPT(.title = "🔍 Google",
           .refresh = google_refresh_callback,
           .enter = google_enter_callback,
           .ctrl_enter = google_ctrlenter_callback,
           .edit = google_edit_callback);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+G", TriggerGoogleSearch,"Fortnite");
    fl_register_feature("Google Search", TriggerGoogleSearch);
}
