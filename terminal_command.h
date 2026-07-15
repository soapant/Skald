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

// Cache for commands in memory
static char** g_cached_commands = NULL;
static int g_cached_command_count = 0;

// ============================================================
// REBUILD CACHE
// ============================================================
static void RebuildCommandCache(void) {
    // Free old cache
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
    
    // Count lines first
    int line_count = 0;
    for (size_t i = 0; i < TerminalHistoryFile->fileSize; i++) {
        if (TerminalHistoryFile->pBuffer[i] == '\n') {
            line_count++;
        }
    }
    if (line_count == 0 && TerminalHistoryFile->fileSize > 0) line_count = 1;
    
    if (line_count == 0) return;
    
    // Allocate for commands
    g_cached_commands = (char**)malloc(line_count * sizeof(char*));
    g_cached_command_count = 0;
    
    // Parse lines
    char* start = TerminalHistoryFile->pBuffer;
    size_t totalBytes = TerminalHistoryFile->fileSize;
    
    for (size_t i = 0; i < totalBytes; i++) {
        if (TerminalHistoryFile->pBuffer[i] == '\n' || i == totalBytes - 1) {
            size_t len = (i == totalBytes - 1) ? (i - (start - TerminalHistoryFile->pBuffer) + 1) : (i - (start - TerminalHistoryFile->pBuffer));
            
            if (len > 0) {
                char* cmd = (char*)malloc(len + 1);
                strncpy(cmd, start, len);
                cmd[len] = '\0';
                
                // Remove trailing \r if present
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
    
    // Rebuild cache if needed
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
// RUN COMMAND
// ============================================================
static void RunTerminalCommand(const char* Command) {
    if (Command != NULL && strlen(Command) > 0) {
        // Clean the command (remove newlines)
        char clean_cmd[512];
        strncpy(clean_cmd, Command, sizeof(clean_cmd) - 1);
        clean_cmd[sizeof(clean_cmd) - 1] = '\0';
        
        char* newline = strchr(clean_cmd, '\n');
        if (newline) *newline = '\0';
        newline = strchr(clean_cmd, '\r');
        if (newline) *newline = '\0';
        
        // Write to temp file
        char temp_file[MAX_PATH];
        GetTempPathA(MAX_PATH, temp_file);
        strcat(temp_file, "temp_cmd.sh");
        
        FILE* f = fopen(temp_file, "w");
        if (f) {
            fprintf(f, "#!/bin/bash\n");
            fprintf(f, "eval \"$(zoxide init bash)\"\n");
            fprintf(f, "%s\nexec bash\n", clean_cmd);
            fclose(f);
        }
        
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), 
                 "start alacritty -e \"C:\\Program Files\\Git\\bin\\bash.exe\" \"%s\"", temp_file);
        system(cmd);
        
        // Append to history with proper newline
        FileAppend(TerminalHistoryFile, clean_cmd);
        FileAppend(TerminalHistoryFile, "\n");
        
        // Update the cache
        RebuildCommandCache();
        
        printf("[TERMINAL COMMAND] Running: %s\n", clean_cmd);
    }
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
        RunTerminalCommand(selection);
    } else {
        char input[256] = {0};
        if (hEditControl != NULL) {
            GetWindowTextA(hEditControl, input, sizeof(input));
            if (strlen(input) > 0) {
                RunTerminalCommand(input);
            }
        }
    }
}

// ============================================================
// TRIGGER
// ============================================================
static void TriggerTerminalCommand(HWND hPopupWnd) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    // Initialize cache on first use
    if (!g_cached_commands) {
        RebuildCommandCache();
    }
    
    // Build the list for PromptUser - ALL the commands
    const char** list = (const char**)malloc(g_cached_command_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_command_count; i++) {
        list[i] = g_cached_commands[i];
    }
    
    // Pass the ACTUAL command count, not 1!
    PromptUser(list, "⌨️ Terminal", g_cached_command_count, 220, 650, 32, 
               terminal_refresh_callback, 
               terminal_enter_callback, 
               NULL, 
               NULL, 
               NULL);
    
    free(list);
}

#endif
