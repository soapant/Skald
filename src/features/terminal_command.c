#include "core/flapi.h"
#include "core/context.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include <stdlib.h>

const char* TerminalHistoryFilePath = "TerminalHistory.txt";

void terminal_refresh_callback(const char* filter, ListBoxInfo* lb) {
    FlStringList files = fl_load_file_list(TerminalHistoryFilePath);
    PopulateListBox(lb, filter, (const char**)files.items, files.count, NULL);
    fl_free_file_list(&files);
}

void terminal_edit_callback(const char* selection, ListBoxInfo* lb) {
    int idx = -1;
    char* edited = fl_edit_text_popup(.initial_text = selection,.lb = lb, .out_index = &idx);
    if (!edited) return;
    if (idx >= 0) fl_replace_line_in_file(TerminalHistoryFilePath, idx, edited);
    free(edited);
    terminal_refresh_callback("", lb);
    UI_Redraw(lb, FlintContext_Get()->window.hListBox);
}

bool terminal_enter_callback(const char* selection, ListBoxInfo* lb) {
    if (!UI_IsListBoxSelected(lb)) {
      fl_append_unique_line(TerminalHistoryFilePath, selection);
    }
    fl_run_terminal_command_and_open((char*)selection);
    return true;
}

bool terminal_ctrlenter_callback(const char* selection, ListBoxInfo* lb) {
    if (!UI_IsListBoxSelected(lb)) {
      fl_append_unique_line(TerminalHistoryFilePath, selection);
    }
    fl_launch_detached((char*)selection);
    return true;
}

void TriggerTerminalCommand(void) {
    PROMPT(.title = "⌨️ Terminal",
           .refresh = terminal_refresh_callback,
           .enter = terminal_enter_callback,
           .ctrl_enter = terminal_ctrlenter_callback,
           .edit = terminal_edit_callback);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+{", TriggerTerminalCommand);

    fl_register_feature("Terminal", TriggerTerminalCommand);
}
