#ifndef REMINDER_H
#define REMINDER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h> 
#include "app_launcher.h"
#include "types.h"
#include "helpers.h"
#include "focus_switcher.h"
#include "calc.h"
#include "timer.h"

// Blueprint for any function that runs at the end (Actions)
typedef void (*ActionFn)(const char* parameter);

// Blueprint for any function that checks a state loop (Conditions)
typedef bool (*ConditionFn)(const char* parameter);

// Blueprint for any function that handles a countdown delay (Timers)
typedef void (*TimerFn)(const char* parameter);

// The definitions that link strings directly to their code execution
typedef struct { char* str; ActionFn    func; } ActionDef;
typedef struct { char* str; ConditionFn func; } ConditionDef;
typedef struct { char* str; TimerFn     func; } TimerDef;

typedef struct {
    char* name;
    ConditionFn func;
} ConditionParamDef;


bool FortniteGameLoaded(const char* param) {
    printf("[CONDITION] Checking if fortnite match loaded...\n");
    return true;
}

ConditionParamDef custom_conditions[] = {
    {"fortnite_loaded", &FortniteGameLoaded}
};

// Global placeholders to pass payload details directly to our ultra-simple thread
static ActionFn g_async_action = NULL;
static char g_async_param[256] = {0};

void HandleFocusWindowFlag(const char* parameter) {
    HWND target_hwnd = FindWindowA(NULL, parameter);
    
    if (target_hwnd != NULL) {
        FocusWindow(target_hwnd);
    } else {
        printf("[ERROR] Could not find window handle for: %s\n", parameter);
    }
}

static DWORD WINAPI SimpleTimerThread(LPVOID lpParam) {
    int remaining = (int)(INT_PTR)lpParam;
    
    while (remaining > 0) {
        Sleep(1000); 
        remaining--;
    }
    
    if (g_async_action != NULL) {
        g_async_action(g_async_param);
    }
    return 0;
}

static void StartSimpleBackgroundTimer(const char* parameter) {
    int total_seconds = ParseTimeToSeconds(parameter);
    if (total_seconds <= 0) return;

    HANDLE hThread = CreateThread(NULL, 0, SimpleTimerThread, (LPVOID)(INT_PTR)total_seconds, 0, NULL);
    if (hThread != NULL) {
        CloseHandle(hThread);
    }
}

void RunBackgroundTimer(const char* parameter) {
    StartSimpleBackgroundTimer(parameter);
}

void RunWindowTimer(const char* parameter) {
    StartSimpleBackgroundTimer(parameter);
    LaunchTimer(parameter);
}
bool CheckGameRunning(const char* parameter) {
    int condition_count = sizeof(custom_conditions) / sizeof(custom_conditions[0]);
    
    for (int i = 0; i < condition_count; i++) {
        if (strcmp(parameter, custom_conditions[i].name) == 0) {
            return custom_conditions[i].func(parameter);
        }
    }
    
    // Fallback error if the user types a function name that doesn't exist
    printf("[ERROR] No registered condition function found matching name: '%s'\n", parameter);
    return true; // Return true to break the blocking loop so the program doesn't freeze forever
}

typedef struct {
    int max_timeout_seconds;

    ConditionFn condition_func;
    const char* condition_param;

    TimerFn     timer_func;
    const char* timer_param;

    ActionFn    action_func;
    const char* action_param;
} ExecutionPipeline;

ActionDef action_flags[] = {
    {"-f",  HandleFocusWindowFlag},
    // {"-dt" DisplayText}
};

ConditionDef condition_flags[] = {
    {"-cg", CheckGameRunning}
};

TimerDef timer_flags[] = {
    {"-trw", RunWindowTimer},
    {"-tr",  RunBackgroundTimer}
};

char* constraint_flags[] = {"-t"};

static void ExtractActionStrs(ActionDef master[], int count, char* output[]) {
    for (int i = 0; i < count; i++) {
        output[i] = master[i].str;
    }
}

static void ExtractConditionStrs(ConditionDef master[], int count, char* output[]) {
    for (int i = 0; i < count; i++) {
        output[i] = master[i].str;
    }
}

static void ExtractTimerStrs(TimerDef master[], int count, char* output[]) {
    for (int i = 0; i < count; i++) {
        output[i] = master[i].str;
    }
}

static void StartReminder(const char* selection) {
    int action_count = sizeof(action_flags) / sizeof(action_flags[0]);
    int condition_count = sizeof(condition_flags) / sizeof(condition_flags[0]);
    int constraint_count = sizeof(constraint_flags) / sizeof(constraint_flags[0]);
    int timer_count = sizeof(timer_flags) / sizeof(timer_flags[0]);
    
    char* action_strs[action_count];
    char* condition_strs[condition_count];
    char* constraint_strs[constraint_count];
    char* timer_strs[timer_count];
    
    FoundFlag action_matches[10];
    FoundFlag condition_matches[10];
    FoundFlag constraint_matches[10];
    FoundFlag timer_matches[10];
    
    ExtractActionStrs(action_flags, action_count, action_strs);
    ExtractConditionStrs(condition_flags, condition_count, condition_strs);
    ExtractTimerStrs(timer_flags, timer_count, timer_strs);
    
    int total_action_flags = SearchFlagsInText(selection, action_flags, action_count, sizeof(ActionDef), action_matches, 10);
    int total_condition_flags = SearchFlagsInText(selection, condition_flags, condition_count, sizeof(ConditionDef), condition_matches, 10);
    int total_constraint_flags = SearchFlagsInText(selection, constraint_flags, constraint_count, sizeof(char*), constraint_matches, 10);
    int total_timer_flags = SearchFlagsInText(selection, timer_flags, timer_count, sizeof(TimerDef), timer_matches, 10);
    
    int grand_total = total_action_flags + total_condition_flags + total_constraint_flags + total_timer_flags;
    
    printf("GOT %i FLAG(S) \n", grand_total);
    
    if (total_condition_flags > 1 || total_action_flags > 1 || total_timer_flags > 1 || total_constraint_flags > 1) {
        printf("YOU HAVE MORE THAN ONE OF ANY RESPECITIVE FLAG TYPE YOU CANT DO THAT\n");
        return;
    }
    if (grand_total < 2) {
        printf("NOT ENOUGH FLAGS TO DO SOMETHING\n");
        return;
    }
    
    if (total_action_flags > 0) {
        printf("\n--- ACTION FLAG FOUND (%s) ---\n", action_matches[0].flag);
        printf("--- ACTION PARAMETER FOUND (%s) ---\n", GetFlagParameter(selection, action_matches[0].index));
    } else {
        printf("\n--- NO ACTION FLAG FOUND ---\n");
    }
    
    if (total_condition_flags > 0) {
        printf("\n--- CONDITION FLAG FOUND (%s) ---\n", condition_matches[0].flag);
        printf("--- CONDITION PARAMETER FOUND (%s) ---\n", GetFlagParameter(selection, condition_matches[0].index));
    } else {
        printf("\n--- NO CONDITION FLAG FOUND ---\n");
    }
    
    if (total_timer_flags > 0) {
        printf("\n--- TIMER FLAG FOUND (%s) ---\n", timer_matches[0].flag);
        printf("--- TIMER PARAMETER FOUND (%s) ---\n", GetFlagParameter(selection, timer_matches[0].index));
    } else {
        printf("\n--- NO TIMER FLAG FOUND ---\n");
    }
    
    if (total_constraint_flags > 0) {
        printf("\n--- CONSTRAINT FLAG FOUND (%s) ---\n", constraint_matches[0].flag);
        printf("--- CONSTRAINT PARAMETER FOUND (%s) ---\n", GetFlagParameter(selection, constraint_matches[0].index));
    } else {
        printf("\n--- NO CONSTRAINT FLAG FOUND ---\n");
    }

    ExecutionPipeline pipeline = {0};

    if (total_constraint_flags > 0) {
        char* constraint_param = GetFlagParameter(selection, constraint_matches[0].index);
        pipeline.max_timeout_seconds = ParseTimeToSeconds(constraint_param);
    }

    if (total_condition_flags > 0) {
        pipeline.condition_param = GetFlagParameter(selection, condition_matches[0].index);
        int master_count = sizeof(condition_flags) / sizeof(condition_flags[0]);
        for (int i = 0; i < master_count; i++) {
            if (strcmp(condition_matches[0].flag, condition_flags[i].str) == 0) {
                pipeline.condition_func = condition_flags[i].func;
                break;
            }
        }
    }

    if (total_timer_flags > 0) {
        pipeline.timer_param = GetFlagParameter(selection, timer_matches[0].index);
        int master_count = sizeof(timer_flags) / sizeof(timer_flags[0]);
        for (int i = 0; i < master_count; i++) {
            if (strcmp(timer_matches[0].flag, timer_flags[i].str) == 0) {
                pipeline.timer_func = timer_flags[i].func;
                break;
            }
        }
    }

    if (total_action_flags > 0) {
        pipeline.action_param = GetFlagParameter(selection, action_matches[0].index);
        int master_count = sizeof(action_flags) / sizeof(action_flags[0]);
        for (int i = 0; i < master_count; i++) {
            if (strcmp(action_matches[0].flag, action_flags[i].str) == 0) {
                pipeline.action_func = action_flags[i].func;
                break;
            }
        }
    }

    // Capture the payload actions into our global async slot before processing stages
    g_async_action = pipeline.action_func;
    if (pipeline.action_param != NULL) {
        strncpy(g_async_param, pipeline.action_param, sizeof(g_async_param) - 1);
    }

    int elapsed_seconds = 0;

    // STAGE A: THE CONDITION BLOCKING LOOP
    if (pipeline.condition_func != NULL) {
        printf("\n[PIPELINE] Entering blocking condition handler...\n");
        while (!pipeline.condition_func(pipeline.condition_param)) {
            Sleep(1000); 
            if (pipeline.max_timeout_seconds > 0) {
                elapsed_seconds++;
                if (elapsed_seconds >= pipeline.max_timeout_seconds) {
                    printf("[PIPELINE] Timeout constraint hit! Forcing loop break.\n");
                    break; 
                }
            }
        }
        printf("[PIPELINE] Exited condition handler.\n");
    }

    // STAGE B: TIMER HANDLING
    if (pipeline.timer_func != NULL) {
        printf("[PIPELINE] Activating selected timer routine...\n");
        pipeline.timer_func(pipeline.timer_param);
        
        // Return early so our main UI thread stays unlocked while background worker handles the delay
        printf("[PIPELINE] Offloaded routine execution. Main thread returning control.\n\n");
        return; 
    }

    // STAGE C: IMMEDIATE DIRECT PAYLOAD ACTIONS
    // Runs instantly ONLY if no timer flags were caught to defer execution
    if (pipeline.action_func != NULL) {
        printf("[PIPELINE] Firing immediate payload actions...\n");
        pipeline.action_func(pipeline.action_param);
    }
    
    printf("[PIPELINE] Pipeline routine finished successfully.\n\n");
}

static void TriggerReminder() {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    char* input;
    
    const char* prompt =  "";
    
    input = SimplePromptUser(prompt,"⏱️ Reminder",520, 650,32);
    
    StartReminder(input);
}

#endif
