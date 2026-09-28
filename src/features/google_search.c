#include "core/flapi.h"
#include "core/context.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include <stdlib.h>

const char* GoogleHistoryFile = "GoogleZoxide.txt";

void google_refresh_callback(const char* filter, ListBoxInfo* lb) {
    FlStringList files = fl_load_file_list(GoogleHistoryFile);
    fl_populate_listbox(lb, filter, (const char**)files.items, files.count);
    fl_free_file_list(&files);
}

void google_edit_callback(const char* selection, ListBoxInfo* lb) {
    int idx = -1;
    char* edited = fl_edit_text_popup(.initial_text = selection,.lb = lb, .out_index = &idx);
    if (!edited) return;
    if (idx >= 0) fl_replace_line_in_file(GoogleHistoryFile, idx, edited);
    free(edited);
    google_refresh_callback("", lb);
}

bool google_enter_callback(const char* selection, ListBoxInfo* lb) {
    if (!fl_is_selected(lb)) {
        fl_append_unique_line(GoogleHistoryFile, selection);
    }
    fl_query_browser(selection);
    return true;
}

bool google_ctrlenter_callback(const char* selection, ListBoxInfo* lb) {
    if (!fl_is_selected(lb)) {
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
