// nvim_zoxide.h
#ifndef NVIM_ZOXIDE_H
#define NVIM_ZOXIDE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "helpers.h"
#include "config.h"

#define FILE_NVIM_ZOXIDE 2
#define NvimZoxideFile G_FILE_ENTRIES[FILE_NVIM_ZOXIDE].mf

static char** g_cached_nvim_commands = NULL;
static int g_cached_nvim_count = 0;
int BigVersion;

static void LaunchNvim(const char* path, int version) {
    if (!path) return;
    
    char cmd[2048];
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    
    if (version == 0) {
        snprintf(cmd, sizeof(cmd), "\"C:\\Program Files\\Neovide\\neovide.exe\" \"%s\"", path);
    } else {
        snprintf(cmd, sizeof(cmd), "\"C:\\Program Files\\Neovim\\bin\\nvim.exe\" \"%s\"", path);
    }
    
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 
                       CREATE_NEW_CONSOLE, 
                       NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        printf("[NVIM] Opened %s in: %s\n", version == 0 ? "Neovide" : "Nvim", path);
    } else {
        char sys_cmd[2048];
        if (version == 0) {
            snprintf(sys_cmd, sizeof(sys_cmd), "start neovide \"%s\"", path);
        } else {
            snprintf(sys_cmd, sizeof(sys_cmd), "start nvim \"%s\"", path);
        }
        system(sys_cmd);
        printf("[NVIM] Opened (fallback) %s in: %s\n", version == 0 ? "Neovide" : "Nvim", path);
    }
}

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
    
    size_t lineCount = 0;
    Line* lines = FileGetLines(NvimZoxideFile, &lineCount);
    if (!lines) return;
    
    g_cached_nvim_commands = (char**)malloc(lineCount * sizeof(char*));
    g_cached_nvim_count = 0;
    
    for (size_t i = 0; i < lineCount; i++) {
        size_t len = lines[i].length;
        while (len > 0 && lines[i].ptr[len-1] == '\r') len--;
        
        if (len > 0) {
            char* cmd = (char*)malloc(len + 1);
            memcpy(cmd, lines[i].ptr, len);
            cmd[len] = '\0';
            g_cached_nvim_commands[g_cached_nvim_count++] = cmd;
        }
    }
    
    free(lines);
}

static void nvim_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    if (!g_cached_nvim_commands) {
        RebuildNvimCache();
    }
    
    PopulateListBox(hListBox, filter, (const char**)g_cached_nvim_commands, g_cached_nvim_count, NULL);
}

static void neovide_enter_callback(const char* selection) {
    int selected_index = LB_ERR;
    if (hListBox != NULL) {
        selected_index = UI_GetSelection(hListBox);
    }
    
    if (selected_index == LB_ERR) {
        char input[256] = {0};
        if (hEditControl != NULL) {
            GetWindowTextA(hEditControl, input, sizeof(input));
            if (strlen(input) > 0) {
                FileAppend(NvimZoxideFile, input);
                FileAppend(NvimZoxideFile, "\n");
                RebuildNvimCache();
                printf("[NVIM] Saved query: %s\n", input);
                
                char* path = QueryZoxidePath(input);
                LaunchNvim(path, 0);
            }
        }
    } else {
        char* path = QueryZoxidePath(selection);
        LaunchNvim(path, 0);
    }
}

static void nvim_enter_callback(const char* selection) {
    int selected_index = LB_ERR;
    if (hListBox != NULL) {
        selected_index = UI_GetSelection(hListBox);
    }
    
    if (selected_index == LB_ERR) {
        char input[256] = {0};
        if (hEditControl != NULL) {
            GetWindowTextA(hEditControl, input, sizeof(input));
            if (strlen(input) > 0) {
                FileAppend(NvimZoxideFile, input);
                FileAppend(NvimZoxideFile, "\n");
                RebuildNvimCache();
                printf("[NVIM] Saved query: %s\n", input);
                
                char* path = QueryZoxidePath(input);
                LaunchNvim(path, 1);
            }
        }
    } else {
        char* path = QueryZoxidePath(selection);
        LaunchNvim(path, 1);
    }
}

static void TriggerNvimZoxide(int version, HWND hPopupWnd) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    BigVersion = version;
    
    if (!g_cached_nvim_commands) {
        RebuildNvimCache();
    }
    
    const char** list = (const char**)malloc(g_cached_nvim_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_nvim_count; i++) {
        list[i] = g_cached_nvim_commands[i];
    }
    
    if (version == 0) { 
        PromptUser(list, "📝 Neovide", g_cached_nvim_count, 920, 650, 32, 
                   nvim_refresh_callback, 
                   neovide_enter_callback, 
                   NULL, 
                   NULL, 
                   NULL);
    } else {
        PromptUser(list, "📝 Nvim", g_cached_nvim_count, 920,650, 32, 
                   nvim_refresh_callback, 
                   nvim_enter_callback, 
                   NULL, 
                   NULL, 
                   NULL);
    }
    free(list);
}

#endif
