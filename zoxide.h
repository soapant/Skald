#ifndef ZOXIDE_H
#define ZOXIDE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "helpers.h"
#include "config.h"

extern HWND hPopupWnd;
extern FileEntry G_FILE_ENTRIES[];
#define FILE_TERMINAL_ZOXIDE 3
#define TerminalZoxideFile G_FILE_ENTRIES[FILE_TERMINAL_ZOXIDE].mf

// Cache for zoxide commands
static char** g_cached_zoxide_commands = NULL;
static int g_cached_zoxide_count = 0;

// ============================================================
// REBUILD CACHE
// ============================================================
static void RebuildZoxideCache(void) {
    if (g_cached_zoxide_commands) {
        for (int i = 0; i < g_cached_zoxide_count; i++) {
            free(g_cached_zoxide_commands[i]);
        }
        free(g_cached_zoxide_commands);
        g_cached_zoxide_commands = NULL;
        g_cached_zoxide_count = 0;
    }
    
    if (!TerminalZoxideFile || TerminalZoxideFile->fileSize == 0) {
        return;
    }
    
    int line_count = 0;
    for (size_t i = 0; i < TerminalZoxideFile->fileSize; i++) {
        if (TerminalZoxideFile->pBuffer[i] == '\n') {
            line_count++;
        }
    }
    if (line_count == 0 && TerminalZoxideFile->fileSize > 0) line_count = 1;
    if (line_count == 0) return;
    
    g_cached_zoxide_commands = (char**)malloc(line_count * sizeof(char*));
    g_cached_zoxide_count = 0;
    
    char* start = TerminalZoxideFile->pBuffer;
    size_t totalBytes = TerminalZoxideFile->fileSize;
    
    for (size_t i = 0; i < totalBytes; i++) {
        if (TerminalZoxideFile->pBuffer[i] == '\n' || i == totalBytes - 1) {
            size_t len = (i == totalBytes - 1) ? (i - (start - TerminalZoxideFile->pBuffer) + 1) : (i - (start - TerminalZoxideFile->pBuffer));
            
            if (len > 0) {
                char* cmd = (char*)malloc(len + 1);
                strncpy(cmd, start, len);
                cmd[len] = '\0';
                
                if (len > 0 && cmd[len-1] == '\r') {
                    cmd[len-1] = '\0';
                }
                
                g_cached_zoxide_commands[g_cached_zoxide_count++] = cmd;
            }
            start = &TerminalZoxideFile->pBuffer[i + 1];
        }
    }
}

// ============================================================
// REFRESH CALLBACK
// ============================================================
static void zoxide_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    if (!g_cached_zoxide_commands) {
        RebuildZoxideCache();
    }
    
    SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
    
    if (!filter || strlen(filter) == 0) {
        for (int i = 0; i < g_cached_zoxide_count; i++) {
            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)g_cached_zoxide_commands[i]);
        }
    } else {
        for (int i = 0; i < g_cached_zoxide_count; i++) {
            if (StrStrIA(g_cached_zoxide_commands[i], filter) != NULL) {
                SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)g_cached_zoxide_commands[i]);
            }
        }
    }
    
    if (g_cached_zoxide_count > 0) {
        SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
    }
}

// ============================================================
// ENTER CALLBACK
// ============================================================
static void zoxide_enter_callback(const char* selection) {
    int selected_index = LB_ERR;
    if (hListBox != NULL) {
        selected_index = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
    }
    
    if (selected_index == LB_ERR) {
        // New query from input - save it
        char input[256] = {0};
        if (hEditControl != NULL) {
            GetWindowTextA(hEditControl, input, sizeof(input));
            if (strlen(input) > 0) {
                // Prevent line collapsing: Add missing newline if needed
                if (TerminalZoxideFile->fileSize > 0) {
                    char lastChar = TerminalZoxideFile->pBuffer[TerminalZoxideFile->fileSize - 1];
                    if (lastChar != '\n') {
                        FileAppend(TerminalZoxideFile, "\n");
                    }
                }

                FileAppend(TerminalZoxideFile, input);
                FileAppend(TerminalZoxideFile, "\n");
                RebuildZoxideCache();
                
                // Run the query
                char* path = QueryZoxidePath(input);
                if (path) {
                    // Non-blocking launch: Start alacritty inside target path as working dir
                    ShellExecuteA(NULL, "open", "alacritty.exe", NULL, path, SW_SHOWNORMAL);
                }
            }
        }
    } else {
        // Selected from history
        char* path = QueryZoxidePath(selection);
        if (path) {
            // Non-blocking launch: Start alacritty inside target path as working dir
            ShellExecuteA(NULL, "open", "alacritty.exe", NULL, path, SW_SHOWNORMAL);
        }
    }
}

// ============================================================
// TRIGGER
// ============================================================
static void TriggerZoxide(HWND hPopupWnd) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    if (!g_cached_zoxide_commands) {
        RebuildZoxideCache();
    }
    
    const char** list = (const char**)malloc(g_cached_zoxide_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_zoxide_count; i++) {
        list[i] = g_cached_zoxide_commands[i];
    }
    
    PromptUser(list, "⌨️ Zoxide", g_cached_zoxide_count, 220, 650, 32, 
               zoxide_refresh_callback, 
               zoxide_enter_callback, 
               NULL, 
               NULL, 
               NULL);
    
    free(list);
}

#endif
