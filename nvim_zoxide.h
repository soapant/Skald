// nvim_zoxide.h
#ifndef NVIM_ZOXIDE_H
#define NVIM_ZOXIDE_H
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common_ui.h"
#include "types.h"
#include "helpers.h"
#include "config.h"

#define FILE_NVIM_ZOXIDE 2
#define NvimZoxideFile G_FILE_ENTRIES[FILE_NVIM_ZOXIDE].mf

static char** g_cached_nvim_commands = NULL;
static int g_cached_nvim_count = 0;
char* TextEditorName;
bool Initialized = false;

static void InitTextEditor() {
    
    const char* last_slash = strrchr(TextEditorPath, '\\');

    const char* start = (last_slash != NULL) ? (last_slash + 1) : TextEditorPath;

    const char* dot_exe = strstr(start, ".exe");

    size_t len = dot_exe - start;

    static char app_name[64];
    
    snprintf(app_name, sizeof(app_name), "%.*s", (int)len, start);
    
    TextEditorName = app_name;
    
    TextEditorName[0] = toupper(TextEditorName[0]);
    
    Initialized = true;
          
}
static void LaunchNvim(const char* path) {
    if (!path) {
        printf("[NVIM] ERROR: path is NULL\n");
        return;
    }
    
    char cmd[2048];
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    
    snprintf(cmd, sizeof(cmd), "%s \"%s\"", TextEditorPath, path);
    
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 
                       CREATE_NEW_CONSOLE, 
                       NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        ShellExecuteA(NULL, "open", TextEditorName, path, NULL, SW_SHOWNORMAL);
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

static void neovide_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    if (!g_cached_nvim_commands) {
        RebuildNvimCache();
    }
    
    PopulateListBox(hListBox, filter, (const char**)g_cached_nvim_commands, g_cached_nvim_count, NULL);
}


static void neovide_enter_callback(const char* selection) {
      char* path = QueryZoxidePath(selection);
      if (path) {
          LaunchNvim(path);
          if (NvimZoxideFile->fileSize > 0) {
              char lastChar = NvimZoxideFile->pBuffer[NvimZoxideFile->fileSize - 1];
              if (lastChar != '\n') {
                  FileAppend(NvimZoxideFile, "\n");
              }
          }

          if (!UI_IsListBoxSelected()) {
              FileAppend(NvimZoxideFile, selection);
              FileAppend(NvimZoxideFile, "\n");
              RebuildNvimCache();
          }
      }
}
static void neovide_edit_callback(const char* selection) {
    char* NewneovideQuery = EditTextPopup(selection,960,540,1000,300);
    
    if (NewneovideQuery != NULL) {
        WriteToCacheIndex(g_SelectedIndex,NewneovideQuery);
    }
    
    ReplaceLineInMappedFile(NvimZoxideFile,g_SelectedIndex,NewneovideQuery);
    
    RebuildNvimCache();
    
    neovide_refresh_callback("",hListBox);
}

static void TriggerNvimZoxide() {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    if (!g_cached_nvim_commands) {
        RebuildNvimCache();
    }
    if (!Initialized) {
      InitTextEditor();
    }
    
    const char** list = (const char**)malloc(g_cached_nvim_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_nvim_count; i++) {
        list[i] = g_cached_nvim_commands[i];
    }
    char titleBuffer[256];
    
    snprintf(titleBuffer,sizeof(titleBuffer),"📝 %s",TextEditorName);
    
    ImportantPromptUser(list, titleBuffer, g_cached_nvim_count, 920, 650, 32, 
               neovide_refresh_callback, 
               neovide_enter_callback, 
               neovide_edit_callback,NULL);
    free(list);
}

#endif
