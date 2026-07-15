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

extern HWND hPopupWnd;
extern MappedFile *TerminalHistoryFile;

static char** g_cached_commands = NULL;
static int g_cached_command_count = 0;

// ============================================================
// REBUILD CACHE
// ============================================================
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
    
    int line_count = 0;
    for (size_t i = 0; i < TerminalHistoryFile->fileSize; i++) {
        if (TerminalHistoryFile->pBuffer[i] == '\n') {
            line_count++;
        }
    }
    if (line_count == 0 && TerminalHistoryFile->fileSize > 0) line_count = 1;
    if (line_count == 0) return;
    
    g_cached_commands = (char**)malloc(line_count * sizeof(char*));
    g_cached_command_count = 0;
    
    char* start = TerminalHistoryFile->pBuffer;
    size_t totalBytes = TerminalHistoryFile->fileSize;
    
    for (size_t i = 0; i < totalBytes; i++) {
        if (TerminalHistoryFile->pBuffer[i] == '\n' || i == totalBytes - 1) {
            size_t len = (i == totalBytes - 1) ? (i - (start - TerminalHistoryFile->pBuffer) + 1) : (i - (start - TerminalHistoryFile->pBuffer));
            
            if (len > 0) {
                char* cmd = (char*)malloc(len + 1);
                strncpy(cmd, start, len);
                cmd[len] = '\0';
                
                if (len > 0 && cmd[len-1] == '\r') {
                    cmd[len-1] = '\0';
                }
                
                g_cached_commands[g_cached_command_count++] = cmd;
            }
            start = &TerminalHistoryFile->pBuffer[i + 1];
        }
    }
}

// ============================================================
// REFRESH CALLBACK
// ============================================================
static void terminal_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    if (!g_cached_commands) {
        RebuildCommandCache();
    }
    
    SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
    
    if (!filter || strlen(filter) == 0) {
        for (int i = 0; i < g_cached_command_count; i++) {
            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)g_cached_commands[i]);
        }
    } else {
        for (int i = 0; i < g_cached_command_count; i++) {
            if (StrStrIA(g_cached_commands[i], filter) != NULL) {
                SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)g_cached_commands[i]);
            }
        }
    }
    
    if (g_cached_command_count > 0) {
        SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
    }
}

// ============================================================
// RUN COMMAND - FIXED: Use CreateProcess directly
// ============================================================
static void RunTerminalCommand(const char* Command, bool save_to_history) {
    if (Command == NULL || strlen(Command) == 0) return;
    
    // Clean the command
    char clean_cmd[512];
    strncpy(clean_cmd, Command, sizeof(clean_cmd) - 1);
    clean_cmd[sizeof(clean_cmd) - 1] = '\0';
    
    char* newline = strchr(clean_cmd, '\n');
    if (newline) *newline = '\0';
    newline = strchr(clean_cmd, '\r');
    if (newline) *newline = '\0';
    
    // Save to history ONLY if it's a new command
    if (save_to_history && strlen(clean_cmd) > 0) {
        FileAppend(TerminalHistoryFile, clean_cmd);
        FileAppend(TerminalHistoryFile, "\n");
        RebuildCommandCache();
        printf("[TERMINAL] Saved to history: %s\n", clean_cmd);
    }
    
    // ============================================================
    // TEMP FILE APPROACH - FAST with system()
    // ============================================================
    char temp_file[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_file);
    strcat(temp_file, "temp_cmd.sh");
    
    FILE* f = fopen(temp_file, "w");
    if (f) {
        fprintf(f, "#!/bin/bash\n");
        fprintf(f, "eval \"$(zoxide init bash)\"\n");
        fprintf(f, "%s\n", clean_cmd);
        fclose(f);
    }
    
    // Use system() but with proper quoting - this is fast enough
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), 
             "start alacritty -e \"C:\\Program Files\\Git\\bin\\bash.exe\" \"%s\"", 
             temp_file);
    
    system(cmd);
    printf("[TERMINAL] Running: %s\n", clean_cmd);
}

// ============================================================
// ENTER CALLBACK
// ============================================================
static void terminal_enter_callback(const char* selection) {
    int selected_index = LB_ERR;
    if (hListBox != NULL) {
        selected_index = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
    }
    
    if (selected_index != LB_ERR) {
        // From history - DON'T save again
        RunTerminalCommand(selection, false);
    } else {
        // New command from input - SAVE to history
        char input[256] = {0};
        if (hEditControl != NULL) {
            GetWindowTextA(hEditControl, input, sizeof(input));
            if (strlen(input) > 0) {
                RunTerminalCommand(input, true);
            }
        }
    }
}

// ============================================================
// TRIGGER
// ============================================================
static void TriggerTerminalCommand(HWND hPopupWnd) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    if (!g_cached_commands) {
        RebuildCommandCache();
    }
    
    const char** list = (const char**)malloc(g_cached_command_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_command_count; i++) {
        list[i] = g_cached_commands[i];
    }
    
    PromptUser(list, "⌨️ Terminal", g_cached_command_count, 220, 650, 32, 
               terminal_refresh_callback, 
               terminal_enter_callback, 
               NULL, 
               NULL, 
               NULL);
    
    free(list);
}

#endif
