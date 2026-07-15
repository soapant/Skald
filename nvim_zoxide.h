#ifndef NVIM_ZOXIDE_H
#define NVIM_ZOXIDE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "helpers.h"
#include "config.h"

extern HWND hPopupWnd;
extern FileEntry G_FILE_ENTRIES[];
#define FILE_NVIM_ZOXIDE 2
#define NvimZoxideFile G_FILE_ENTRIES[FILE_NVIM_ZOXIDE].mf

static char** g_cached_nvim_commands = NULL;
static int g_cached_nvim_count = 0;

extern void WriteDebugLog(const char* format, ...);

// ============================================================
// REBUILD CACHE
// ============================================================
static void RebuildNvimCache(void) {
    if (g_cached_nvim_commands) {
        for (int i = 0; i < g_cached_nvim_count; i++) {
            free(g_cached_nvim_commands[i]);
        }
        free(g_cached_nvim_commands);
        g_cached_nvim_commands = NULL;
        g_cached_nvim_count = 0;
    }
    
    if (!NvimZoxideFile || NvimZoxideFile->fileSize == 0) {
        return;
    }
    
    int line_count = 0;
    for (size_t i = 0; i < NvimZoxideFile->fileSize; i++) {
        if (NvimZoxideFile->pBuffer[i] == '\n') {
            line_count++;
        }
    }
    if (line_count == 0 && NvimZoxideFile->fileSize > 0) line_count = 1;
    if (line_count == 0) return;
    
    g_cached_nvim_commands = (char**)malloc(line_count * sizeof(char*));
    g_cached_nvim_count = 0;
    
    char* start_ptr = NvimZoxideFile->pBuffer;
    size_t totalBytes = NvimZoxideFile->fileSize;
    
    for (size_t i = 0; i < totalBytes; i++) {
        if (NvimZoxideFile->pBuffer[i] == '\n' || i == totalBytes - 1) {
            size_t len = (i == totalBytes - 1) ? (i - (start_ptr - NvimZoxideFile->pBuffer) + 1) : (i - (start_ptr - NvimZoxideFile->pBuffer));
            
            if (len > 0) {
                char* cmd = (char*)malloc(len + 1);
                strncpy(cmd, start_ptr, len);
                cmd[len] = '\0';
                
                if (len > 0 && cmd[len-1] == '\r') {
                    cmd[len-1] = '\0';
                }
                
                g_cached_nvim_commands[g_cached_nvim_count++] = cmd;
            }
            start_ptr = &NvimZoxideFile->pBuffer[i + 1];
        }
    }
}

// ============================================================
// REFRESH CALLBACK
// ============================================================
static void nvim_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    if (!g_cached_nvim_commands) {
        RebuildNvimCache();
    }
    
    SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
    
    if (!filter || strlen(filter) == 0) {
        for (int i = 0; i < g_cached_nvim_count; i++) {
            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)g_cached_nvim_commands[i]);
        }
    } else {
        for (int i = 0; i < g_cached_nvim_count; i++) {
            if (StrStrIA(g_cached_nvim_commands[i], filter) != NULL) {
                SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)g_cached_nvim_commands[i]);
            }
        }
    }
    
    if (g_cached_nvim_count > 0) {
        SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
    }
}

// ============================================================
// ENTER CALLBACK WITH WORKING DIRECTORY AND SAFE NEWLINE FIX
// ============================================================
static void nvim_enter_callback(const char* selection) {
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
                // BUG FIX: Check if the file doesn't end with a newline before appending
                if (NvimZoxideFile->fileSize > 0) {
                    char lastChar = NvimZoxideFile->pBuffer[NvimZoxideFile->fileSize - 1];
                    if (lastChar != '\n') {
                        FileAppend(NvimZoxideFile, "\n");
                    }
                }
                
                FileAppend(NvimZoxideFile, input);
                FileAppend(NvimZoxideFile, "\n");
                RebuildNvimCache();
                
                char* path = QueryZoxidePath(input);
                if (path) {
                    // BUG FIX: Set the 5th parameter (working directory) so Neovide launches in that folder!
                    ShellExecuteA(NULL, "open", "neovide.exe", NULL, path, SW_SHOWNORMAL);
                    WriteDebugLog("[NVIM] Launched Neovide in: %s\n", path);
                }
            }
        }
    } else {
        // Selected from history
        char* path = QueryZoxidePath(selection);
        if (path) {
            // BUG FIX: Set working directory parameter here too!
            ShellExecuteA(NULL, "open", "neovide.exe", NULL, path, SW_SHOWNORMAL);
            WriteDebugLog("[NVIM] Launched Neovide from history in: %s\n", path);
        }
    }
}

// ============================================================
// TRIGGER
// ============================================================
static void TriggerNvimZoxide(int version, HWND hPopupWnd) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    if (!g_cached_nvim_commands) {
        RebuildNvimCache();
    }
    
    const char** list = (const char**)malloc(g_cached_nvim_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_nvim_count; i++) {
        list[i] = g_cached_nvim_commands[i];
    }
    
    PromptUser(list, "📝 Neovide", g_cached_nvim_count, 220, 650, 32, 
               nvim_refresh_callback, 
               nvim_enter_callback, 
               NULL, 
               NULL, 
               NULL);
    
    free(list);
}

#endif
