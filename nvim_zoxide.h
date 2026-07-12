#ifndef NVIM_ZOXIDE_H
#define NVIM_ZOXIDE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"

extern HWND hPopupWnd;

static void TriggerNvimZoxide(int version,HWND hPopupWnd) {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        return;
    }

    const char* prompt[1] = { "Enter zoxide query (NEOVIDE)..." };
    char* query = PromptUser(prompt,"📝 Neovide / Nvim", 1, 220, 150,32,NULL,NULL,NULL,NULL,NULL);
    if (query != NULL && strlen(query) > 0) {
        // Build the zoxide query command
        char command[512];
        snprintf(command, sizeof(command), "zoxide query %s", query);
        
        // Run zoxide and capture output
        char path[1024] = {0};
        FILE* pipe = popen(command, "r");
        if (pipe) {
            fgets(path, sizeof(path), pipe);
            pclose(pipe);
        }
        
        // Remove newline from path
        path[strcspn(path, "\r\n")] = '\0';
        if (version == 0 ){
            if (strlen(path) > 0) {
                char nvim_cmd[2048];
                snprintf(nvim_cmd, sizeof(nvim_cmd), "start neovide \"%s\"", path);
                system(nvim_cmd);
                printf("[NEOVIDE] Opened nvim in: %s\n", path);
            } else {
                printf("[NEOVIDE] No match found for: %s\n", query);
            }
        } else{
            if (strlen(path) > 0) {
                char nvim_cmd[2048];
                snprintf(nvim_cmd, sizeof(nvim_cmd), "start nvim \"%s\"", path);
                system(nvim_cmd);
                printf("[NVIM] Opened nvim in: %s\n", path);
            } else {
                printf("[NVIM] No match found for: %s\n", query);
            }
        }
    }
}

#endif
