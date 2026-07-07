#ifndef TIMER_H
#define TIMER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "app_launcher.h"

extern char* PromptQuickSwitcher(const char** items, int count, int width, int height);
extern HWND hPopupWnd;

static void TriggerTimer(void) {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        return;
    }

    const char* prompt[1] = { "Enter timer duration (e.g., 5m, 10h,20s or 0 for a stopwatch" };
    char* input = PromptQuickSwitcher(prompt, 1, 220, 150);

    if (input != NULL && strlen(input) > 0) {
        // Find sowon in cache
        char sowon_path[MAX_PATH] = {0};
        for (int i = 0; i < G_APP_COUNT; i++) {
            if (StrStrIA(G_APPS[i].name, "sowon") != NULL) {
                strncpy(sowon_path, G_APPS[i].path, sizeof(sowon_path) - 1);
                break;
            }
        }

        if (strlen(sowon_path) == 0) {
            printf("[TIMER] ERROR: sowon.exe not found in cache!\n");
            return;
        }

        // Launch sowon with the input as argument
        launch_exe_with_args(sowon_path, input);
        printf("[TIMER] Launching: %s %s\n", sowon_path, input);
    }
}

#endif
