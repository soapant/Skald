#ifndef EXPLORER_H
#define EXPLORER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <shellapi.h>
#include "common_ui.h"
#include "types.h"
#include "helpers.h"
#include "config.h"

#define FILE_EXPLORER_ZOXIDE 4
#define ExplorerZoxideFile G_FILE_ENTRIES[FILE_EXPLORER_ZOXIDE].mf

static char** g_cached_explorer_commands = NULL;
static int g_cached_explorer_count = 0;


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
    
    size_t lineCount = 0;
    Line* lines = FileGetLines(ExplorerZoxideFile, &lineCount);
    if (!lines) return;
    
    g_cached_explorer_commands = (char**)malloc(lineCount * sizeof(char*));
    g_cached_explorer_count = 0;
    
    for (size_t i = 0; i < lineCount; i++) {
        size_t len = lines[i].length;
        while (len > 0 && lines[i].ptr[len-1] == '\r') len--;
        
        if (len > 0) {
            char* cmd = (char*)malloc(len + 1);
            memcpy(cmd, lines[i].ptr, len);
            cmd[len] = '\0';
            g_cached_explorer_commands[g_cached_explorer_count++] = cmd;
        }
    }
    
    free(lines);
}

static void explorer_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    if (!g_cached_explorer_commands) {
        RebuildExplorerCache();
    }
    
    PopulateListBox(hListBox, filter, (const char**)g_cached_explorer_commands, g_cached_explorer_count, NULL);
}

static void explorer_enter_callback(const char* selection) {
          if (ExplorerZoxideFile->fileSize > 0) {
              char lastChar = ExplorerZoxideFile->pBuffer[ExplorerZoxideFile->fileSize - 1];
              if (lastChar != '\n') {
                  FileAppend(ExplorerZoxideFile, "\n");
              }
          }

          if (!UI_IsListBoxSelected()) {
            FileAppend(ExplorerZoxideFile, selection);
            FileAppend(ExplorerZoxideFile, "\n");
            RebuildExplorerCache();
          }
          char* path = QueryZoxidePath(selection);
          if (path) {
              ShellExecuteA(NULL, "open", "explorer.exe", path, NULL, SW_SHOWNORMAL);
          }
}

static void explorer_edit_callback(const char* selection) {
    char* NewexplorerQuery = EditTextPopup(selection,960,540,1000,300);
    
    if (NewexplorerQuery != NULL) {
        WriteToCacheIndex(g_SelectedIndex,NewexplorerQuery);
    }
    
    ReplaceLineInMappedFile(ExplorerZoxideFile,g_SelectedIndex,NewexplorerQuery);
    
    RebuildExplorerCache();
    
    explorer_refresh_callback("",hListBox);
}

static void TriggerExplorer() {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    if (!g_cached_explorer_commands) {
        RebuildExplorerCache();
    }
    
    const char** list = (const char**)malloc(g_cached_explorer_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_explorer_count; i++) {
        list[i] = g_cached_explorer_commands[i];
    }
    
    ImportantPromptUser(list, "📁 Explorer", g_cached_explorer_count, 920, 650, 32, 
               explorer_refresh_callback, 
               explorer_enter_callback, 
               explorer_edit_callback,NULL);
    
    free(list);
}

#endif
