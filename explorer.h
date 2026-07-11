#ifndef EXPLORER_H
#define EXPLORER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <shellapi.h>
#include "types.h"
typedef void (*RefreshCallback)(const char* filter, HWND hListBox);
typedef void (*EnterCallback)(const char* selection);
typedef void (*CtrlEnterCallback)(const char* selection);
typedef void (*TabCallback)(void);

extern HWND hPopupWnd;

static void TriggerExplorer(HWND hPopupWnd) {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        return;
    }

    const char* prompt[1] = { "Enter zoxide query (EXPLORER)..." };
    char* query = PromptUser(prompt, 1, 220, 150,32,NULL,NULL,NULL,NULL,NULL);

    if (query != NULL && strlen(query) > 0) {
        // Run zoxide to get the path
        char command[512];
        snprintf(command, sizeof(command), "zoxide query %s", query);
        
        char path[1024] = {0};
        FILE* pipe = popen(command, "r");
        if (pipe) {
            fgets(path, sizeof(path), pipe);
            pclose(pipe);
        }
        
        path[strcspn(path, "\r\n")] = '\0';

        if (strlen(path) > 0) {
            // Open File Explorer - NO terminal window!
            ShellExecuteA(NULL, "open", "explorer.exe", path, NULL, SW_SHOWNORMAL);
            printf("[EXPLORER] Opened Explorer in: %s\n", path);
        } else {
            printf("[EXPLORER] No match found for: %s\n", query);
        }
    }
}

#endif
