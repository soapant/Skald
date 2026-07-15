#ifndef EXPLORER_H
#define EXPLORER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <shellapi.h>
#include "types.h"
#include "helpers.h"
#include "config.h"

extern HWND hPopupWnd;
extern FileEntry G_FILE_ENTRIES[];
#define FILE_EXPLORER_ZOXIDE 4
#define ExplorerZoxideFile G_FILE_ENTRIES[FILE_EXPLORER_ZOXIDE].mf

static char** g_cached_explorer_commands = NULL;
static int g_cached_explorer_count = 0;

// ============================================================
// REBUILD CACHE
// ============================================================
static void RebuildExplorerCache(void) {
    if (g_cached_explorer_commands) {
        for (int i = 0; i < g_cached_explorer_count; i++) {
            free(g_cached_explorer_commands[i]);
        }
        free(g_cached_explorer_commands);
        g_cached_explorer_commands = NULL;
        g_cached_explorer_count = 0;
    }
    
    if (!ExplorerZoxideFile || ExplorerZoxideFile->fileSize == 0) {
        return;
    }
    
    int line_count = 0;
    for (size_t i = 0; i < ExplorerZoxideFile->fileSize; i++) {
        if (ExplorerZoxideFile->pBuffer[i] == '\n') {
            line_count++;
        }
    }
    if (line_count == 0 && ExplorerZoxideFile->fileSize > 0) line_count = 1;
    if (line_count == 0) return;
    
    g_cached_explorer_commands = (char**)malloc(line_count * sizeof(char*));
    g_cached_explorer_count = 0;
    
    char* start = ExplorerZoxideFile->pBuffer;
    size_t totalBytes = ExplorerZoxideFile->fileSize;
    
    for (size_t i = 0; i < totalBytes; i++) {
        if (ExplorerZoxideFile->pBuffer[i] == '\n' || i == totalBytes - 1) {
            size_t len = (i == totalBytes - 1) ? (i - (start - ExplorerZoxideFile->pBuffer) + 1) : (i - (start - ExplorerZoxideFile->pBuffer));
            
            if (len > 0) {
                char* cmd = (char*)malloc(len + 1);
                strncpy(cmd, start, len);
                cmd[len] = '\0';
                
                if (len > 0 && cmd[len-1] == '\r') {
                    cmd[len-1] = '\0';
                }
                
                g_cached_explorer_commands[g_cached_explorer_count++] = cmd;
            }
            start = &ExplorerZoxideFile->pBuffer[i + 1];
        }
    }
}

// ============================================================
// REFRESH CALLBACK
// ============================================================
static void explorer_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    if (!g_cached_explorer_commands) {
        RebuildExplorerCache();
    }
    
    SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
    
    if (!filter || strlen(filter) == 0) {
        for (int i = 0; i < g_cached_explorer_count; i++) {
            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)g_cached_explorer_commands[i]);
        }
    } else {
        for (int i = 0; i < g_cached_explorer_count; i++) {
            if (StrStrIA(g_cached_explorer_commands[i], filter) != NULL) {
                SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)g_cached_explorer_commands[i]);
            }
        }
    }
    
    if (g_cached_explorer_count > 0) {
        SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
    }
}

// ============================================================
// ENTER CALLBACK
// ============================================================
static void explorer_enter_callback(const char* selection) {
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
                FileAppend(ExplorerZoxideFile, input);
                FileAppend(ExplorerZoxideFile, "\n");
                RebuildExplorerCache();
                printf("[EXPLORER] Saved query: %s\n", input);
                
                char* path = QueryZoxidePath(input);
                if (path) {
                    ShellExecuteA(NULL, "open", "explorer.exe", path, NULL, SW_SHOWNORMAL);
                    printf("[EXPLORER] Opened Explorer in: %s\n", path);
                }
            }
        }
    } else {
        // Selected from history
        char* path = QueryZoxidePath(selection);
        if (path) {
            ShellExecuteA(NULL, "open", "explorer.exe", path, NULL, SW_SHOWNORMAL);
            printf("[EXPLORER] Opened Explorer in: %s\n", path);
        }
    }
}

// ============================================================
// TRIGGER
// ============================================================
static void TriggerExplorer(HWND hPopupWnd) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    if (!g_cached_explorer_commands) {
        RebuildExplorerCache();
    }
    
    const char** list = (const char**)malloc(g_cached_explorer_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_explorer_count; i++) {
        list[i] = g_cached_explorer_commands[i];
    }
    
    PromptUser(list, "📁 Explorer", g_cached_explorer_count, 220, 650, 32, 
               explorer_refresh_callback, 
               explorer_enter_callback, 
               NULL, 
               NULL, 
               NULL);
    
    free(list);
}

#endif
