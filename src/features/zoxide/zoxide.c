#include "core/flapi.h"
#include <stdio.h>
#include <stdlib.h>


const char* TerminalZoxideFilePath = "TerminalZoxide.txt";
static void zoxide_refresh(const char* filter, ListBoxInfo* lb) {
    FlStringList files = fl_load_file_list(TerminalZoxideFilePath);
    fl_populate_listbox(lb, filter, (const char**)files.items, files.count);
    fl_free_file_list(&files);
}
static void zoxide_edit(const char* selection, ListBoxInfo* lb) {
    int idx = -1;
    char* edited = fl_edit_text_popup(.initial_text = selection,.lb = lb, .out_index = &idx);
    if (!edited) return;
    if (idx >= 0) fl_replace_line_in_file(TerminalZoxideFilePath, idx, edited);
    free(edited);
    zoxide_refresh("", lb);
}
static bool zoxide_enter(const char* selection, ListBoxInfo* lb) {
    char* path = fl_get_zoxide_match((char*)selection);
    if (!path) return false;
    fl_launch_terminal_at_path(path, NULL);
    if (!fl_is_selected(lb)) fl_append_unique_line(TerminalZoxideFilePath, selection);
    free(path);
    return true;
}
static void TriggerZoxide(void) {
    FlStringList files = fl_load_file_list(TerminalZoxideFilePath);
    fl_window(.title = "⌨️ Zoxide", .items = (const char**)files.items,
           .refresh = zoxide_refresh, .enter = zoxide_enter, .edit = zoxide_edit);
    fl_free_file_list(&files);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey(.hotkey="Alt+W", .cb=TriggerZoxide, .blacklist={"\1"});

    fl_register_feature(.name="Zoxide",.cb=TriggerZoxide,.icon="/folder.png");
}
