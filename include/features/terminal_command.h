#ifndef TERMINAL_COMMAND_H
#define TERMINAL_COMMAND_H

#include "core/flapi.h"
#include "core/zoxide_helpers.h"

typedef struct {
    int vk_code;
    int modifiers;
    const char* terminal_command;
    bool open_in_terminal;
} TerminalCommandHotkey;

static char** g_cached_commands = NULL;
static int g_cached_command_count = 0;

#define FILE_TERMINAL_HISTORY 0
#define TerminalHistoryFile G_FILE_ENTRIES[FILE_TERMINAL_HISTORY].mf

static void RebuildCommandCache(void) {
    ZoxideHelper_RebuildCache(TerminalHistoryFile, &g_cached_commands, &g_cached_command_count);
}

static void terminal_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    ZoxideHelper_Refresh(filter, hListBox, lb, TerminalHistoryFile, &g_cached_commands, &g_cached_command_count);
}

static void terminal_enter_callback(const char* selection, ListBoxInfo* lb) {
      if (TerminalHistoryFile->fileSize > 0) {
          char lastChar = Platform_GetLastChar(TerminalHistoryFile);
          if (lastChar != '\n') {
              Platform_FileAppend(TerminalHistoryFile, "\n");
          }
      }
      if (!UI_IsListBoxSelected(lb)) {
        Platform_FileAppend(TerminalHistoryFile, selection);
        Platform_FileAppend(TerminalHistoryFile, "\n");
      }
      // RunTerminalCommand(selection, false,true);
}

static void terminal_edit_callback(const char* selection, ListBoxInfo* lb) {
    ZoxideHelper_Edit(selection, lb, TerminalHistoryFile, &g_cached_commands, &g_cached_command_count, terminal_refresh_callback);
}

static void TriggerTerminalCommand(void) {
    if (!g_cached_commands) RebuildCommandCache();
    const char** list = ZoxideHelper_PrepareTriggerList(NULL, &g_cached_commands, &g_cached_command_count, NULL);

    PROMPT(.title = "⌨️ Terminal", .items = list, .count = g_cached_command_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = terminal_refresh_callback,
           .enter = terminal_enter_callback,
           .edit = terminal_edit_callback);

    ZoxideHelper_CleanupTriggerList(list);
}

static void run_btop(void) {
    // RunTerminalCommand("btop", false, false);
}

static inline void terminal_command_init(void) {
    fl_register_hotkey("Alt+{", TriggerTerminalCommand, NULL, NULL);
    fl_register_hotkey("Alt+Ctrl+B", run_btop, NULL, NULL);
}

#endif
