#ifndef YAZI_ZOXIDE_H
#define YAZI_ZOXIDE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern char* PromptQuickSwitcher(const char** items, int count, int width, int height);
extern HWND hPopupWnd;

static void TriggerYaziZoxide(void) {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        return;
    }

    const char* prompt[1] = { "Enter zoxide query (YAZI)..." };
    char* query = PromptQuickSwitcher(prompt, 1, 220, 150);

    if (query != NULL && strlen(query) > 0) {
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
            char yazi_cmd[2048];
            snprintf(yazi_cmd, sizeof(yazi_cmd), "start alacritty --working-directory \"%s\" -e yazi", path);
            system(yazi_cmd);
            printf("[YAZI] Opened yazi in Alacritty at: %s\n", path);
        } else {
            printf("[YAZI] No match found for: %s\n", query);
        }
    }
}

#endif
