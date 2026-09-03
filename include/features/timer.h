#ifndef TIMER_H
#define TIMER_H

#include "core/flapi.h"
void LaunchTimer(const char* selection) {
    fl_launch_with_args(TimerPath, selection);
    printf("[TIMER] Launching: %s %s\n", TimerPath, selection);
}

void TriggerTimer(void) {
    char* input;
    
    const char* prompt =  "Enter timer duration (10s 50m 2h or enter 0 for a stopwatch)";
    
    input = SIMPLE_PROMPT(.prompt = prompt, .title = "⏱️ Timer",
                          .height = 920, .width = 650, .item_height = 32);


    LaunchTimer(input);
}

static inline void timer_init(void) {
    fl_register_hotkey("Alt+O", TriggerTimer);
}

#endif
