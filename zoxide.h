#ifndef ZOXIDE_H
#define ZOXIDE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern char* PromptQuickSwitcher(const char** items, int count, int width, int height);
extern HWND hPopupWnd;

static void TriggerZoxide() {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        return;
    }

    const char* prompt[1] = { "Enter zoxide query (TERMINAL)..." };
    char* query = PromptQuickSwitcher(prompt, 1, 220, 150);

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
