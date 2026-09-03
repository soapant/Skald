#ifndef TERMINAL_COMMAND_H
#define TERMINAL_COMMAND_H

#include "core/flapi.h"
#include "core/zoxide_helpers.h"

const char* TerminalHistoryFilePath = "data/TerminalHistory.txt";

char** g_cached_commands = NULL;
int g_cached_command_count = 0;


void RebuildCommandCache(void) {
    ZoxideHelper_RebuildCache(TerminalHistoryFilePath, &g_cached_commands, &g_cached_command_count);
}

void terminal_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    ZoxideHelper_Refresh(filter, hListBox, lb, TerminalHistoryFilePath, &g_cached_commands, &g_cached_command_count);
}

void terminal_enter_callback(const char* selection, ListBoxInfo* lb) {
      if (!UI_IsListBoxSelected(lb)) {
        fl_append_to_file(TerminalHistoryFilePath, selection);
        ZoxideHelper_RebuildCache(TerminalHistoryFilePath, &g_cached_commands, &g_cached_command_count);
      }
      fl_run_terminal_command((char*)selection,true);
}

void terminal_edit_callback(const char* selection, ListBoxInfo* lb) {
    ZoxideHelper_Edit(selection, lb, TerminalHistoryFilePath, &g_cached_commands, &g_cached_command_count, terminal_refresh_callback);
}

void TriggerTerminalCommand(void) {
    if (!g_cached_commands) RebuildCommandCache();
    const char** list = ZoxideHelper_PrepareTriggerList(TerminalHistoryFilePath, &g_cached_commands, &g_cached_command_count, NULL);

    PROMPT(.title = "⌨️ Terminal", .items = list, .count = g_cached_command_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = terminal_refresh_callback,
           .enter = terminal_enter_callback,
           .edit = terminal_edit_callback);

    ZoxideHelper_CleanupTriggerList(list);
}

void run_btop(void) {
    fl_run_terminal_command((char*)"ls",true);
}

static inline void terminal_command_init(void) {
    fl_register_hotkey("Alt+{", TriggerTerminalCommand, NULL, NULL);
    fl_register_hotkey("Ctrl+B", run_btop, NULL, NULL);
}

#endif
