#ifndef TIMER_H
#define TIMER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "app_launcher.h"
#include "types.h"

extern HWND hPopupWnd;

static void LaunchTimer(const char* selection) {
    if (selection != NULL && strlen(selection) > 0) {
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

        launch_exe_with_args(sowon_path, selection);
        printf("[TIMER] Launching: %s %s\n", sowon_path, selection);
    }
}

static void TriggerTimer() {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    char* input;
    
    const char* prompt =  "Enter timer duration (10s 50m 2h or enter 0 for a stopwatch)";
    
    input = SimplePromptUser(prompt,"⏱️ Timer",920, 650,32);
    
    LaunchTimer(input);
}

#endif
