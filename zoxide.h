#ifndef ZOXIDE_H
#define ZOXIDE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "helpers.h"
#include "config.h"

#define FILE_TERMINAL_ZOXIDE 3
#define TerminalZoxideFile G_FILE_ENTRIES[FILE_TERMINAL_ZOXIDE].mf

static char** g_cached_zoxide_commands = NULL;
static int g_cached_zoxide_count = 0;

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
    
    size_t lineCount = 0;
    Line* lines = FileGetLines(TerminalZoxideFile, &lineCount);
    if (!lines) return;
    
    g_cached_zoxide_commands = (char**)malloc(lineCount * sizeof(char*));
    g_cached_zoxide_count = 0;
    
    for (size_t i = 0; i < lineCount; i++) {
        size_t len = lines[i].length;
        while (len > 0 && lines[i].ptr[len-1] == '\r') len--;
        
        if (len > 0) {
            char* cmd = (char*)malloc(len + 1);
            memcpy(cmd, lines[i].ptr, len);
            cmd[len] = '\0';
            g_cached_zoxide_commands[g_cached_zoxide_count++] = cmd;
        }
    }
    
    free(lines);
}

static void zoxide_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    if (!g_cached_zoxide_commands) {
        RebuildZoxideCache();
    }
    
    PopulateListBox(hListBox, filter, (const char**)g_cached_zoxide_commands, g_cached_zoxide_count, NULL);
}

static void zoxide_edit_callback(const char* selection) {
    char* NewZoxideQuery = EditTextPopup(selection,960,540,1000,300);
    
    if (NewZoxideQuery != NULL) {
        WriteToCacheIndex(g_SelectedIndex,NewZoxideQuery);
    }
    
    ReplaceLineInMappedFile(TerminalZoxideFile,g_SelectedIndex,NewZoxideQuery);
    
    RebuildZoxideCache();
    
    zoxide_refresh_callback("",hListBox);
}



static void zoxide_enter_callback(const char* selection) {
    int selected_index = LB_ERR;
    if (hListBox != NULL) {
        selected_index = UI_GetSelection(hListBox);
    }
    
    if (selected_index == LB_ERR) {
        char input[256] = {0};
        if (hEditControl != NULL) {
            UI_GetText(hEditControl, input, sizeof(input));
            printf("THE USER INPUT  IS %s",input);
            if (strlen(input) > 0) {
                if (TerminalZoxideFile->fileSize > 0) {
                    char lastChar = TerminalZoxideFile->pBuffer[TerminalZoxideFile->fileSize - 1];
                    if (lastChar != '\n') {
                        FileAppend(TerminalZoxideFile, "\n");
                    }
                }

                FileAppend(TerminalZoxideFile, input);
                FileAppend(TerminalZoxideFile, "\n");
                RebuildZoxideCache();
                
                
                char* path = QueryZoxidePath(input);
                if (path != NULL) {
                    char terminal[512];
                    snprintf(terminal,sizeof(terminal),"%s.exe",UsedTerminal);
                    ShellExecuteA(NULL, "open", terminal, NULL, path, SW_SHOWNORMAL);
                }
            }
        }
    } else {
        char* path = QueryZoxidePath(selection);
        if (path != NULL) {
            char terminal[512];
            snprintf(terminal,sizeof(terminal),"%s.exe",UsedTerminal);
            ShellExecuteA(NULL, "open", terminal, NULL, path, SW_SHOWNORMAL);
        }
    }
}

static void TriggerZoxide() {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    if (!g_cached_zoxide_commands) {
        RebuildZoxideCache();
    }
    
    const char** list = (const char**)malloc(g_cached_zoxide_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_zoxide_count; i++) {
        list[i] = g_cached_zoxide_commands[i];
    }
    
    PromptUser(list, "⌨️ Zoxide", g_cached_zoxide_count,920,650, 32, 
               zoxide_refresh_callback, 
               zoxide_enter_callback, 
               NULL, 
               NULL, 
               NULL,
               zoxide_edit_callback);
    
    free(list);
}

#endif
