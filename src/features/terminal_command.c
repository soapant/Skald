#include "common.h"
#include "features/terminal_command.h"
#include "helpers.h"
#include "core/common_ui.h"
#if defined(_WIN32)
#include <shellapi.h>
#endif

static char** g_cached_commands = NULL;
static int g_cached_command_count = 0;

static void RebuildCommandCache(void) {
    if (g_cached_commands) {
        for (int i = 0; i < g_cached_command_count; i++) {
            free(g_cached_commands[i]);
        }
        free(g_cached_commands);
        g_cached_commands = NULL;
        g_cached_command_count = 0;
    }
    
    if (!TerminalHistoryFile || TerminalHistoryFile->fileSize == 0) {
        return;
    }
    
    size_t lineCount = 0;
    Line* lines = Platform_FileGetLines(TerminalHistoryFile, &lineCount);
    if (!lines) return;
    
    g_cached_commands = (char**)malloc(lineCount * sizeof(char*));
    g_cached_command_count = 0;
    
    for (size_t i = 0; i < lineCount; i++) {
        size_t len = lines[i].length;
        while (len > 0 && lines[i].ptr[len-1] == '\r') len--;
        
        if (len > 0) {
            char* cmd = (char*)malloc(len + 1);
            memcpy(cmd, lines[i].ptr, len);
            cmd[len] = '\0';
            g_cached_commands[g_cached_command_count++] = cmd;
        }
    }
    
    free(lines);
}

static void terminal_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    if (!lb) return;
    
    if (!g_cached_commands) {
        RebuildCommandCache();
    }
    
    PopulateListBox(lb, hListBox, filter, (const char**)g_cached_commands, g_cached_command_count, NULL);
}


static void RunTerminalCommand(const char* Command, bool save_to_history) {
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
    const char* real_user = getenv("SUDO_USER");
    if (!real_user) real_user = "diggy";
    
    char cache_dir[MAX_PATH];
    if (strcmp(real_user, "root") == 0) {
        snprintf(cache_dir, sizeof(cache_dir), "/home/diggy/.cache");
        snprintf(temp_file, sizeof(temp_file), "/home/diggy/.cache/temp_cmd.sh");
    } else {
        snprintf(cache_dir, sizeof(cache_dir), "/home/%s/.cache", real_user);
        snprintf(temp_file, sizeof(temp_file), "/home/%s/.cache/temp_cmd.sh", real_user);
    }
    char mkdir_cmd[MAX_PATH + 64];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\" && chown %s:%s \"%s\"", cache_dir, real_user, real_user, cache_dir);
    system(mkdir_cmd);
#endif

    FILE* f = fopen(temp_file, "w");
    if (f) {
        fprintf(f, "#!/bin/bash\n");
        fprintf(f, "%s\n", clean_cmd);
        fclose(f);
        
#ifndef _WIN32
        char chown_file[MAX_PATH + 64];
        snprintf(chown_file, sizeof(chown_file), "chown %s:%s \"%s\" && chmod 755 \"%s\"", real_user, real_user, temp_file, temp_file);
        system(chown_file);
#else
        chmod(temp_file, 0755);
#endif
    }
    
#if defined(_WIN32)
    char terminal_exe[512];
    snprintf(terminal_exe, sizeof(terminal_exe), "%s.exe", UsedTerminal);
    
    char args[2048];
    snprintf(args, sizeof(args), "-e bash \"%s\"", temp_file);
    ShellExecuteA(NULL, "open", terminal_exe, args, NULL, SW_SHOWNORMAL);
#else
    const char* display = getenv("DISPLAY");
    if (!display) display = ":0";
    
    const char* xauthority = getenv("XAUTHORITY");
    if (!xauthority) xauthority = "";

    char runtime_dir[256];
    const char* env_runtime = getenv("XDG_RUNTIME_DIR");
    if (env_runtime) {
        snprintf(runtime_dir, sizeof(runtime_dir), "%s", env_runtime);
    } else {
        snprintf(runtime_dir, sizeof(runtime_dir), "/run/user/1000");
    }

    const char* term = (UsedTerminal && strlen(UsedTerminal) > 0) ? UsedTerminal : "alacritty";

    char cmd_open[1024];
    snprintf(cmd_open, sizeof(cmd_open), 
             "sudo -u %s DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s %s -e bash \"%s\" &", 
             real_user, display, xauthority, runtime_dir, term, temp_file);
    
    system(cmd_open);
#endif
    
    printf("[TERMINAL] Running: %s\n", clean_cmd);
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
      RunTerminalCommand(selection, false);
}

static void terminal_edit_callback(const char* selection, ListBoxInfo* lb) {
    char* NewterminalQuery = Platform_EditTextPopup(selection,960,540,1000,300);
    
    if (NewterminalQuery != NULL) {
        WriteToCacheIndex(lb, lb->selected_index,NewterminalQuery);
    }
    
    Platform_ReplaceLineInMappedFile(TerminalHistoryFile,lb->selected_index,NewterminalQuery);
    
    RebuildCommandCache();
    
    terminal_refresh_callback("", hListBox, lb);
}

void TriggerTerminalCommand(void) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    if (!g_cached_commands) {
        RebuildCommandCache();
    }
    
    const char** list = (const char**)malloc(g_cached_command_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_command_count; i++) {
        list[i] = g_cached_commands[i];
    }
    
    ImportantPromptUser(list, "⌨️ Terminal", g_cached_command_count, 920, 650, 32, 
               terminal_refresh_callback, 
               terminal_enter_callback, 
               terminal_edit_callback,NULL);
    
    free(list);
}
