#include "core/flapi.h"

const char* TerminalHistoryFilePath = "TerminalHistory.txt";

void terminal_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(TerminalHistoryFilePath, items, MAX_FILE_LINES);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, n, NULL);
    fl_free_file_list(items, n);
}

void terminal_edit_callback(const char* selection, ListBoxInfo* lb) {
    int idx = -1;
    char* edited = fl_edit_text_popup(.initial_text = selection, .title = "Edit Terminal", .x = 960, .y = 540, .lb = lb, .out_index = &idx);
    if (!edited) return;
    if (idx >= 0) fl_replace_line_in_file(TerminalHistoryFilePath, idx, edited);
    free(edited);
    terminal_refresh_callback("", FlintContext_Get()->window.hListBox, lb);
    UI_Redraw(lb, FlintContext_Get()->window.hListBox);
}

bool terminal_enter_callback(const char* selection, ListBoxInfo* lb) {
      if (!UI_IsListBoxSelected(lb)) {
        fl_append_unique_line(TerminalHistoryFilePath, selection);
      }
      fl_run_terminal_command_and_open((char*)selection);
    return true;
}

void TriggerTerminalCommand(void) {
    PROMPT(.title = "⌨️ Terminal",
           .refresh = terminal_refresh_callback,
           .enter = terminal_enter_callback,
           .edit = terminal_edit_callback);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+{", TriggerTerminalCommand);

    fl_register_feature("Terminal", TriggerTerminalCommand);
}
