#ifndef YAZI_ZOXIDE_H
#define YAZI_ZOXIDE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "helpers.h"
#include "config.h"

extern HWND hPopupWnd;
extern FileEntry G_FILE_ENTRIES[];
#define FILE_YAZI_ZOXIDE 1
#define YaziZoxideFile G_FILE_ENTRIES[FILE_YAZI_ZOXIDE].mf

static char** g_cached_yazi_commands = NULL;
static int g_cached_yazi_count = 0;

// ============================================================
// REBUILD CACHE
// ============================================================
static void RebuildYaziCache(void) {
    if (g_cached_yazi_commands) {
        for (int i = 0; i < g_cached_yazi_count; i++) {
            free(g_cached_yazi_commands[i]);
        }
        free(g_cached_yazi_commands);
        g_cached_yazi_commands = NULL;
        g_cached_yazi_count = 0;
    }
    
    if (!YaziZoxideFile || YaziZoxideFile->fileSize == 0) {
        return;
    }
    
    size_t lineCount = 0;
    Line* lines = FileGetLines(YaziZoxideFile, &lineCount);
    if (!lines) return;
    
    g_cached_yazi_commands = (char**)malloc(lineCount * sizeof(char*));
    g_cached_yazi_count = 0;
    
    for (size_t i = 0; i < lineCount; i++) {
        size_t len = lines[i].length;
        while (len > 0 && lines[i].ptr[len-1] == '\r') len--;
        
        if (len > 0) {
            char* cmd = (char*)malloc(len + 1);
            memcpy(cmd, lines[i].ptr, len);
            cmd[len] = '\0';
            g_cached_yazi_commands[g_cached_yazi_count++] = cmd;
        }
    }
    
    free(lines);
}

// ============================================================
// REFRESH CALLBACK
// ============================================================
static void yazi_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    if (!g_cached_yazi_commands) {
        RebuildYaziCache();
    }
    
    UI_ResetContent(hListBox);
    
    if (!filter || strlen(filter) == 0) {
        for (int i = 0; i < g_cached_yazi_count; i++) {
            UI_AddString(hListBox, g_cached_yazi_commands[i]);
        }
    } else {
        for (int i = 0; i < g_cached_yazi_count; i++) {
            if (StrStrIA(g_cached_yazi_commands[i], filter) != NULL) {
                UI_AddString(hListBox, g_cached_yazi_commands[i]);
            }
        }
    }
    
    if (g_cached_yazi_count > 0) {
        UI_SetSelection(hListBox, 0);
    }
}

// ============================================================
// ENTER CALLBACK
// ============================================================
static void yazi_enter_callback(const char* selection) {
    int selected_index = LB_ERR;
    if (hListBox != NULL) {
        selected_index = UI_GetSelection(hListBox);
    }
    
    if (selected_index == LB_ERR) {
        // New query from input - save it
        char input[256] = {0};
        if (hEditControl != NULL) {
            GetWindowTextA(hEditControl, input, sizeof(input));
            if (strlen(input) > 0) {
                FileAppend(YaziZoxideFile, input);
                FileAppend(YaziZoxideFile, "\n");
                RebuildYaziCache();
                char* path = QueryZoxidePath(input);
                if (path) {
                    // Launch asynchronously, setting the directory and invoking -e yazi
                    ShellExecuteA(NULL, "open", "alacritty.exe", "-e yazi", path, SW_SHOWNORMAL);
                }
            }
        }
    } else {
        // Selected from history
        char* path = QueryZoxidePath(selection);
        if (path) {
            ShellExecuteA(NULL, "open", "alacritty.exe", "-e yazi", path, SW_SHOWNORMAL);
        }
    }
}

// ============================================================
// TRIGGER
// ============================================================
static void TriggerYaziZoxide() {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    if (!g_cached_yazi_commands) {
        RebuildYaziCache();
    }
    
    const char** list = (const char**)malloc(g_cached_yazi_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_yazi_count; i++) {
        list[i] = g_cached_yazi_commands[i];
    }
    
    PromptUser(list, "📂 Yazi", g_cached_yazi_count, 520, 650, 32, 
               yazi_refresh_callback, 
               yazi_enter_callback, 
               NULL, 
               NULL, 
               NULL);
    
    free(list);
}

#endif
