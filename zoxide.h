#ifndef ZOXIDE_H
#define ZOXIDE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"

extern HWND hPopupWnd;

static void TriggerZoxide(HWND hPopupWnd) {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        return;
    }

    const char* prompt[1] = { "Enter zoxide query (TERMINAL)..." };
    char* query = PromptUser(prompt,"⌨️ Zoxide",1, 220, 150,32,NULL,NULL,NULL,NULL,NULL);

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
        
            path[strcspn(path, "\r\n")] = '\0';

            if (strlen(path) > 0) {
                char alacritty_cmd[2048];
                snprintf(alacritty_cmd, sizeof(alacritty_cmd), "start alacritty --working-directory \"%s\"", path);
                system(alacritty_cmd);
                printf("[ALACRITTY] Opened Alacritty in: %s\n", path);
            } 
          }
        }

#endif
