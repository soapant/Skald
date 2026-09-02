#ifndef TERMINAL_COMMAND_H
#define TERMINAL_COMMAND_H

#include "core/flapi.h"
#include "core/zoxide_helpers.h"
#include "core/platform.h"

const char* TerminalHistoryFilePath = "data/TerminalHistory.txt";
MappedFile TerminalHistoryFileData = {0};
bool TerminalHistoryFileInit = false;
MappedFile* GetTerminalHistoryFile(void) {
    if (!TerminalHistoryFileInit) {
#if !defined(_WIN32)
        TerminalHistoryFileData.fd = -1;
#endif
        Platform_FileInit(TerminalHistoryFilePath, &TerminalHistoryFileData);
        TerminalHistoryFileInit = true;
    }
    return &TerminalHistoryFileData;
}
#define TerminalHistoryFile GetTerminalHistoryFile()


char** g_cached_commands = NULL;
int g_cached_command_count = 0;


void RebuildCommandCache(void) {
    ZoxideHelper_RebuildCache(TerminalHistoryFile, &g_cached_commands, &g_cached_command_count);
}

void terminal_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    ZoxideHelper_Refresh(filter, hListBox, lb, TerminalHistoryFile, &g_cached_commands, &g_cached_command_count);
}

void terminal_enter_callback(const char* selection, ListBoxInfo* lb) {
      if (!UI_IsListBoxSelected(lb)) {
        Platform_FileAppend(TerminalHistoryFile, selection);
        Platform_FileAppend(TerminalHistoryFile, "\n");
      }
      fl_run_terminal_command(selection,true);
}

void terminal_edit_callback(const char* selection, ListBoxInfo* lb) {
    ZoxideHelper_Edit(selection, lb, TerminalHistoryFile, &g_cached_commands, &g_cached_command_count, terminal_refresh_callback);
}

void TriggerTerminalCommand(void) {
    if (!g_cached_commands) RebuildCommandCache();
    const char** list = ZoxideHelper_PrepareTriggerList(NULL, &g_cached_commands, &g_cached_command_count, NULL);

    PROMPT(.title = "⌨️ Terminal", .items = list, .count = g_cached_command_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = terminal_refresh_callback,
           .enter = terminal_enter_callback,
           .edit = terminal_edit_callback);

    ZoxideHelper_CleanupTriggerList(list);
}

void run_btop(void) {
    fl_run_terminal_command("ls",true);
}

static inline void terminal_command_init(void) {
    fl_register_hotkey("Alt+{", TriggerTerminalCommand, NULL, NULL);
    fl_register_hotkey("Ctrl+B", run_btop, NULL, NULL);
}

#endif
