#ifndef TIMER_H
#define TIMER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "app_launcher.h"
#include "config.h"
#include "types.h"

static void LaunchTimer(const char* selection) {
    if (selection != NULL && strlen(selection) > 0) {
        launch_exe_with_args(TimerPath, selection);
        printf("[TIMER] Launching: %s %s\n", TimerPath, selection);
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
