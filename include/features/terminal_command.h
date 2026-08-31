#ifndef TERMINAL_COMMAND_H
#define TERMINAL_COMMAND_H

#include "core/flapi.h"
#include "core/zoxide_helpers.h"

static const TerminalCommandHotkey G_TERMINAL_COMMAND_HOTKEYS[] = {
    { 'B', HOTKEY_MOD_ALT_CTRL,"btop",false},
};
static const unsigned int TERMINAL_COMMAND_HOTKEY_COUNT = sizeof(G_TERMINAL_COMMAND_HOTKEYS) / sizeof(G_TERMINAL_COMMAND_HOTKEYS[0]);

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

static void RunTerminalCommand(const char* Command, bool save_to_history,bool open_in_terminal) {
    if (Command == NULL || strlen(Command) == 0) return;
    
    char clean_cmd[512];
    strncpy(clean_cmd, Command, sizeof(clean_cmd) - 1);
    clean_cmd[sizeof(clean_cmd) - 1] = '\0';
    
    char* newline = strchr(clean_cmd, '\n');
    if (newline) *newline = '\0';
    newline = strchr(clean_cmd, '\r');
    if (newline) *newline = '\0';
    
    if (save_to_history && strlen(clean_cmd) > 0) {
        Platform_FileAppend(TerminalHistoryFile, clean_cmd);
        Platform_FileAppend(TerminalHistoryFile, "\n");
        RebuildCommandCache();
        printf("[TERMINAL] Saved to history: %s\n", clean_cmd);
    }
    
    char temp_file[MAX_PATH];
#if defined(_WIN32)
    GetTempPathA(MAX_PATH, temp_file);
    strcat(temp_file, "temp_cmd.sh");
#else
    const char* real_user = Platform_GetRealUser();
    const char* real_home = Platform_GetRealHome();
    char cache_dir[MAX_PATH];
    snprintf(cache_dir, sizeof(cache_dir), "%s/.cache", real_home);
    snprintf(temp_file, sizeof(temp_file), "%s/.cache/temp_cmd.sh", real_home);
    char mkdir_cmd[MAX_PATH + 64];
    if (Platform_IsRunningAsRoot())
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\" && chown %s:%s \"%s\"", cache_dir, real_user, real_user, cache_dir);
    else
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", cache_dir);
    system(mkdir_cmd);
#endif

    FILE* f = fopen(temp_file, "w");
    if (f) {
        fprintf(f, "#!/bin/bash\n");
        fprintf(f, "%s\n", clean_cmd);
        fclose(f);
#if !defined (_WIN32)
        if (Platform_IsRunningAsRoot()){
            char chown_file[MAX_PATH + 64];
            snprintf(chown_file, sizeof(chown_file), "chown %s:%s \"%s\" && chmod 755 \"%s\"", real_user, real_user, temp_file, temp_file);
            system(chown_file);
        } else {
            chmod(temp_file, 0755);
        }
#endif
    }
    
#if defined(_WIN32)
    char terminal_exe[512];
    snprintf(terminal_exe, sizeof(terminal_exe), "%s.exe", UsedTerminal);
    
    char args[2048];
    snprintf(args, sizeof(args), "-e bash \"%s\"", temp_file);
    ShellExecuteA(NULL, "open", terminal_exe, args, NULL, SW_SHOWNORMAL);
#else
    const char* display = Platform_GetSessionDisplay();
    const char* xauthority = Platform_GetSessionXAuthority();
    char runtime_dir[256];
    snprintf(runtime_dir,sizeof(runtime_dir),"%s",Platform_GetRuntimeDir());
    char extra[1024];
    snprintf(extra,sizeof(extra),"-e bash \"%s\"", temp_file);
    if (open_in_terminal) {
    Platform_LaunchTerminal(real_home, extra);
    }
#endif
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
      RunTerminalCommand(selection, false,true);
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

static inline bool CheckTerminalCommandHotkeyBindings(int pressed_vk, int modifiers) {
    int idx = fl_find_terminal_idx(pressed_vk, modifiers, G_TERMINAL_COMMAND_HOTKEYS, TERMINAL_COMMAND_HOTKEY_COUNT);
    if (idx < 0) return false;
    RunTerminalCommand(G_TERMINAL_COMMAND_HOTKEYS[idx].terminal_command, false, G_TERMINAL_COMMAND_HOTKEYS[idx].open_in_terminal);
    return true;
}

static inline void terminal_command_init(void) {
    fl_register_hotkey("Alt+{", TriggerTerminalCommand, NULL, NULL);
}

#endif
