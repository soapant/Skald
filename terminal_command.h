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

#define FILE_TERMINAL_HISTORY 0
#define TerminalHistoryFile G_FILE_ENTRIES[FILE_TERMINAL_HISTORY].mf
static char** g_cached_commands = NULL;
static int g_cached_command_count = 0;

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
    
    size_t lineCount = 0;
    Line* lines = FileGetLines(TerminalHistoryFile, &lineCount);
    if (!lines) return;
    
    g_cached_commands = (char**)malloc(lineCount * sizeof(char*));
    g_cached_command_count = 0;
    
    for (size_t i = 0; i < lineCount; i++) {
        size_t len = lines[i].length;
        while (len > 0 && lines[i].ptr[len-1] == '\r') len--;
        
        if (len > 0) {
            char* cmd = (char*)malloc(len + 1);
            memcpy(cmd, lines[i].ptr, len);
            cmd[len] = '\0';
            g_cached_commands[g_cached_command_count++] = cmd;
        }
    }
    
    free(lines);
}

static void terminal_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    if (!g_cached_commands) {
        RebuildCommandCache();
    }
    
    PopulateListBox(hListBox, filter, (const char**)g_cached_commands, g_cached_command_count, NULL);
}


static void RunTerminalCommand(const char* Command, bool save_to_history) {
    if (Command == NULL || strlen(Command) == 0) return;
    
    char clean_cmd[512];
    strncpy(clean_cmd, Command, sizeof(clean_cmd) - 1);
    clean_cmd[sizeof(clean_cmd) - 1] = '\0';
    
    char* newline = strchr(clean_cmd, '\n');
    if (newline) *newline = '\0';
    newline = strchr(clean_cmd, '\r');
    if (newline) *newline = '\0';
    
    if (save_to_history && strlen(clean_cmd) > 0) {
        FileAppend(TerminalHistoryFile, clean_cmd);
        FileAppend(TerminalHistoryFile, "\n");
        RebuildCommandCache();
        printf("[TERMINAL] Saved to history: %s\n", clean_cmd);
    }
    
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
    char sys_cmd[2048];
    snprintf(sys_cmd, sizeof(sys_cmd), 
             "start %s -e \"C:\\Program Files\\Git\\bin\\bash.exe\" \"%s\"",UsedTerminal,
             temp_file);
    system(sys_cmd);
    printf("[TERMINAL] Running %s\n", clean_cmd);
}

static void terminal_enter_callback(const char* selection) {
    int selected_index = LB_ERR;
    if (hListBox != NULL) {
        selected_index = UI_GetSelection(hListBox);
    }
    
    if (selected_index != LB_ERR) {
        RunTerminalCommand(selection, false);
    } else {
        char input[256] = {0};
        if (hEditControl != NULL) {
            UI_GetText(hEditControl, input, sizeof(input));
            if (strlen(input) > 0) {
                RunTerminalCommand(input, true);
            }
        }
    }
}

static void TriggerTerminalCommand() {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    if (!g_cached_commands) {
        RebuildCommandCache();
    }
    
    const char** list = (const char**)malloc(g_cached_command_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_command_count; i++) {
        list[i] = g_cached_commands[i];
    }
    
    PromptUser(list, "⌨️ Terminal", g_cached_command_count, 920, 650, 32, 
               terminal_refresh_callback, 
               terminal_enter_callback, 
               NULL, 
               NULL, 
               NULL,
               NULL);
    
    free(list);
}

#endif
