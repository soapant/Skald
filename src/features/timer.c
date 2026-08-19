#include "common.h"
#include "features/timer.h"
#include "features/app_launcher.h"
#include "core/common_ui.h"

void LaunchTimer(const char* selection) {
    if (selection != NULL && strlen(selection) > 0) {
        launch_exe_with_args(TimerPath, selection);
        printf("[TIMER] Launching: %s %s\n", TimerPath, selection);
    }
}

void TriggerTimer(void) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    char* input;
    
    const char* prompt =  "Enter timer duration (10s 50m 2h or enter 0 for a stopwatch)";
    
    input = Platform_SimplePromptUser(prompt,"⏱️ Timer",920, 650,32);
    
    LaunchTimer(input);
}
