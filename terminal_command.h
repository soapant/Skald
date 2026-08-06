// terminal_command.h
#ifndef TERMINAL_COMMAND_H
#define TERMINAL_COMMAND_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "types.h"
#include "helpers.h"

#define FILE_TERMINAL_HISTORY 0
#define TerminalHistoryFile G_FILE_ENTRIES[FILE_TERMINAL_HISTORY].mf
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
    Line* lines = FileGetLines(TerminalHistoryFile, &lineCount);
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

static void terminal_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    if (!g_cached_commands) {
        RebuildCommandCache();
    }
    
    PopulateListBox(hListBox, filter, (const char**)g_cached_commands, g_cached_command_count, NULL);
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
        FileAppend(TerminalHistoryFile, clean_cmd);
        FileAppend(TerminalHistoryFile, "\n");
        RebuildCommandCache();
        printf("[TERMINAL] Saved to history: %s\n", clean_cmd);
    }
    
    const char* bash = NULL;
    if (GetFileAttributesA("C:\\Program Files\\Git\\bin\\bash.exe") != INVALID_FILE_ATTRIBUTES) {
        bash = "C:\\Program Files\\Git\\bin\\bash.exe";
    } else if (GetFileAttributesA("C:\\Program Files (x86)\\Git\\bin\\bash.exe") != INVALID_FILE_ATTRIBUTES) {
        bash = "C:\\Program Files (x86)\\Git\\bin\\bash.exe";
    } else {
        bash = "bash";
    }
    
    char temp_file[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_file);
    strcat(temp_file, "temp_cmd.sh");
    
    FILE* f = fopen(temp_file, "w");
    if (f) {
        fprintf(f, "#!/bin/bash\n");
        fprintf(f, "source ~/.bashrc 2>/dev/null\n");
        fprintf(f, "eval \"$(zoxide init bash)\" 2>/dev/null\n");
        fprintf(f, "%s\n", clean_cmd);
        fprintf(f, "\necho\nread -p \"Press Enter to close...\"\n");
        fclose(f);
    }
    
    char terminal_exe[512];
    snprintf(terminal_exe, sizeof(terminal_exe), "%s.exe", UsedTerminal);
    
    char args[2048];
    snprintf(args, sizeof(args), "-e \"%s\" \"%s\"", bash, temp_file);
    
    ShellExecuteA(NULL, "open", terminal_exe, args, NULL, SW_SHOWNORMAL);
    
    printf("[TERMINAL] Running: %s\n", clean_cmd);
}

static void terminal_enter_callback(const char* selection) {
      if (TerminalHistoryFile->fileSize > 0) {
          char lastChar = TerminalHistoryFile->pBuffer[TerminalHistoryFile->fileSize - 1];
          if (lastChar != '\n') {
              FileAppend(TerminalHistoryFile, "\n");
          }
      }
      if (!UI_IsListBoxSelected()) {
        FileAppend(TerminalHistoryFile, selection);
        FileAppend(TerminalHistoryFile, "\n");
      }
      RunTerminalCommand(selection, false);
}

static void terminal_edit_callback(const char* selection) {
    char* NewterminalQuery = EditTextPopup(selection,960,540,1000,300);
    
    if (NewterminalQuery != NULL) {
        WriteToCacheIndex(g_SelectedIndex,NewterminalQuery);
    }
    
    ReplaceLineInMappedFile(TerminalHistoryFile,g_SelectedIndex,NewterminalQuery);
    
    RebuildCommandCache();
    
    terminal_refresh_callback("",hListBox);
}

static void TriggerTerminalCommand() {
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

#endif
