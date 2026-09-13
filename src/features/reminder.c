#include "core/flapi.h"
#if !defined(_WIN32)
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#endif
#if defined(_WIN32)
#include <windows.h>
#endif

const char* ReminderHistoryFilePath = "data/ReminderHistory.txt";


typedef void (*ActionFn)(const char* parameter);
typedef bool (*ConditionFn)(const char* parameter);
typedef void (*TimerFn)(const char* parameter);
typedef struct { char* str; ActionFn    func; } ActionDef;
typedef struct { char* str; ConditionFn func; } ConditionDef;
typedef struct { char* str; TimerFn     func; } TimerDef;
typedef struct {
    char* name;
    ConditionFn func;
} ConditionParamDef;

ActionFn async_action = NULL;
char action_param[256] = {0};
char timer_param[256] = {0};


#if defined(_WIN32)
DWORD WINAPI SimpleTimerThread(LPVOID lpParam) {
    int remaining = (int)(INT_PTR)lpParam;
    
    while (remaining > 0) {
        Sleep(1000); 
        remaining--;
    }
    
    if (async_action != NULL) {
        async_action(action_param);
    }
    fl_terminate_process_by_name("sowon.exe");
    return 0;
}
#else
void* SimpleTimerThread(void* lpParam) {
    int remaining = (int)(intptr_t)lpParam;
    
    while (remaining > 0) {
        sleep(1); 
        remaining--;
    }
    
    if (async_action != NULL) {
        async_action(action_param);
    }
    fl_terminate_process_by_name("sowon.exe");
    return NULL;
}
#endif

void StartSimpleBackgroundTimer(const char* parameter) {
    int total_seconds = ParseTimeToSeconds(parameter);
    if (total_seconds <= 0) return;

#if defined(_WIN32)
    HANDLE hThread = CreateThread(NULL, 0, SimpleTimerThread, (LPVOID)(INT_PTR)total_seconds, 0, NULL);
    if (hThread != NULL) {
        CloseHandle(hThread);
    }
#else
    pthread_t thread;
    pthread_create(&thread, NULL, SimpleTimerThread, (void*)(intptr_t)total_seconds);
    pthread_detach(thread);
#endif
}

void RunBackgroundTimer(const char* parameter) {
    StartSimpleBackgroundTimer(parameter);
}

void RunWindowTimer(const char* parameter) {
    strncpy(timer_param, parameter, sizeof(timer_param) - 1);
    timer_param[sizeof(timer_param) - 1] = '\0';

    printf("[TIMER] Timer duration: %s, Action to execute: %s\n", parameter, action_param);
    StartSimpleBackgroundTimer(parameter);

    if (strlen(timer_param) > 0) {
        printf("[TIMER] Launching timer with param: %s\n", timer_param);

    }
}

bool FortniteGameLoaded(const char* param) {
    printf("[CONDITION] Checking if fortnite match loaded...\n");
    return true;
}

ConditionParamDef custom_conditions[] = {
    {"fortnite_loaded", &FortniteGameLoaded}
};



bool CheckGameRunning(const char* parameter) {
    int condition_count = ARRAY_LEN(custom_conditions);
    
    for (int i = 0; i < condition_count; i++) {
        if (strcmp(parameter, custom_conditions[i].name) == 0) {
            return custom_conditions[i].func(parameter);
        }
    }
    
    printf("[ERROR] No registered condition function found matching name: '%s'\n", parameter);
    return true; 
}








void DisplayText(const char* parameter){
    Platform_DisplayMessage(parameter,960,999,950,70,3,COLOR_TEXT);
}

const ActionDef REMINDER_ACTION_FLAGS[] = {

    {"-dt", DisplayText},
};

const ConditionDef REMINDER_CONDITION_FLAGS[] = {
    {"-cg", CheckGameRunning},
};

const TimerDef REMINDER_TIMER_FLAGS[] = {
    {"-trw", RunWindowTimer},
    {"-tr",  RunBackgroundTimer},
};

const char* REMINDER_CONSTRAINT_FLAGS[] = {
    "-t",
};

void reminder_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(ReminderHistoryFilePath, items, MAX_FILE_LINES);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, n, NULL);
    fl_free_file_list(items, n);
}


void StartReminder(const char* selection) {
    if (!selection || strlen(selection) == 0) {
        printf("[REMINDER] Empty input\n");
        return;
    }
    
    printf("[REMINDER] Processing: '%s'\n", selection);
    
    FoundFlag action_matches[10];
    FoundFlag condition_matches[10];
    FoundFlag constraint_matches[10];
    FoundFlag timer_matches[10];
    
    memset(action_matches, 0, sizeof(action_matches));
    memset(condition_matches, 0, sizeof(condition_matches));
    memset(constraint_matches, 0, sizeof(constraint_matches));
    memset(timer_matches, 0, sizeof(timer_matches));
#define REMINDER_ACTION_FLAGS_COUNT ARRAY_LEN(action_matches)
#define REMINDER_CONDITION_FLAGS_COUNT ARRAY_LEN(condition_matches)
#define REMINDER_CONSTRAINT_FLAGS_COUNT ARRAY_LEN(constraint_matches)
#define REMINDER_TIMER_FLAGS_COUNT ARRAY_LEN(timer_matches)
    
    int total_action_flags = SearchFlagsInText(selection, REMINDER_ACTION_FLAGS, REMINDER_ACTION_FLAGS_COUNT, sizeof(ActionDef), action_matches, 10);
    int total_condition_flags = SearchFlagsInText(selection, REMINDER_CONDITION_FLAGS, REMINDER_CONDITION_FLAGS_COUNT, sizeof(ConditionDef), condition_matches, 10);
    int total_constraint_flags = SearchFlagsInText(selection, REMINDER_CONSTRAINT_FLAGS, REMINDER_CONSTRAINT_FLAGS_COUNT, sizeof(char*), constraint_matches, 10);
    int total_timer_flags = SearchFlagsInText(selection, REMINDER_TIMER_FLAGS, REMINDER_TIMER_FLAGS_COUNT, sizeof(TimerDef), timer_matches, 10);
    
    int grand_total = total_action_flags + total_condition_flags + total_constraint_flags + total_timer_flags;
    
    printf("[REMINDER] Found %d flags total\n", grand_total);
    
    if (total_condition_flags > 1 || total_action_flags > 1 || total_timer_flags > 1 || total_constraint_flags > 1) {
        printf("[REMINDER] ERROR: Multiple flags of same type not allowed\n");
        return;
    }
    if (grand_total < 2) {
        printf("[REMINDER] ERROR: Need at least 2 flags (e.g., -f Notepad -trw 5m)\n");
        return;
    }
    
    char* action_param = NULL;
    char* condition_param = NULL;
    char* timer_param = NULL;
    char* constraint_param = NULL;
    
    ActionFn action_func = NULL;
    ConditionFn condition_func = NULL;
    TimerFn timer_func = NULL;
    
    if (total_action_flags > 0) {
        printf("[REMINDER] Action flag: %s\n", action_matches[0].flag);
        action_param = GetFlagParameter(selection, action_matches[0].index);
        printf("[REMINDER] Action param: '%s'\n", action_param ? action_param : "(null)");
        
        for (int i = 0; i < REMINDER_ACTION_FLAGS_COUNT; i++) {
            if (strcmp(action_matches[0].flag, REMINDER_ACTION_FLAGS[i].str) == 0) {
                action_func = REMINDER_ACTION_FLAGS[i].func;
                break;
            }
        }
        
        async_action = action_func;
    }
    
    if (total_condition_flags > 0) {
        printf("[REMINDER] Condition flag: %s\n", condition_matches[0].flag);
        condition_param = GetFlagParameter(selection, condition_matches[0].index);
        printf("[REMINDER] Condition param: '%s'\n", condition_param ? condition_param : "(null)");
        
        for (int i = 0; i < REMINDER_CONDITION_FLAGS_COUNT; i++) {
            if (strcmp(condition_matches[0].flag, REMINDER_CONDITION_FLAGS[i].str) == 0) {
                condition_func = REMINDER_CONDITION_FLAGS[i].func;
                break;
            }
        }
    }
    
    if (total_timer_flags > 0) {
        printf("[REMINDER] Timer flag: %s\n", timer_matches[0].flag);
        timer_param = GetFlagParameter(selection, timer_matches[0].index);
        printf("[REMINDER] Timer param: '%s'\n", timer_param ? timer_param : "(null)");
        
        for (int i = 0; i < REMINDER_TIMER_FLAGS_COUNT; i++) {
            if (strcmp(timer_matches[0].flag, REMINDER_TIMER_FLAGS[i].str) == 0) {
                timer_func = REMINDER_TIMER_FLAGS[i].func;
                break;
            }
        }
    }
    
    if (total_constraint_flags > 0) {
        printf("[REMINDER] Constraint flag: %s\n", constraint_matches[0].flag);
        constraint_param = GetFlagParameter(selection, constraint_matches[0].index);
        printf("[REMINDER] Constraint param: '%s'\n", constraint_param ? constraint_param : "(null)");
    }
    
    int max_timeout = 0;
    if (constraint_param) {
        max_timeout = ParseTimeToSeconds(constraint_param);
        printf("[REMINDER] Timeout set to %d seconds\n", max_timeout);
    }
    
    int elapsed_seconds = 0;
    
    if (condition_func != NULL) {
        printf("[REMINDER] Stage 1: Waiting for condition...\n");
        while (!condition_func(condition_param)) {
#if defined(_WIN32)
            Sleep(1000);
#else
            sleep(1);
#endif
            if (max_timeout > 0) {
                elapsed_seconds++;
                printf("[REMINDER] Condition wait: %d/%d seconds\n", elapsed_seconds, max_timeout);
                if (elapsed_seconds >= max_timeout) {
                    printf("[REMINDER] Condition timeout reached!\n");
                    break;
                }
            }
        }
        printf("[REMINDER] Condition met!\n");
    }
    
    if (timer_func != NULL) {
        printf("[REMINDER] Stage 2: Starting timer with param '%s'\n", timer_param ? timer_param : "(null)");
        
        if (timer_func != NULL && action_param) {
            printf("[REMINDER] Timer will execute action '%s' after delay\n", action_param);
        }
        
        timer_func(timer_param);
        printf("[REMINDER] Timer offloaded to background thread. Returning.\n");
        return;
    }
    
    if (action_func != NULL) {
        printf("[REMINDER] Stage 3: Executing action immediately for action param %s \n", action_param);
        action_func(action_param);
        printf("[REMINDER] Action executed\n");
    }
    
    printf("[REMINDER] Done\n");
}

bool reminder_enter_callback(const char* selection, ListBoxInfo* lb) {
      if (!UI_IsListBoxSelected(lb)) fl_append_unique_line(ReminderHistoryFilePath, selection);
      StartReminder(selection);
    return true;
}
static void edit_confirm(const char* new_text, void* user_data) {
    EditCallbackData* data = (EditCallbackData*)user_data;
    if (new_text && data) {
        data->new_text = (char*)new_text;
        EditCallback_Handle(data, reminder_refresh_callback);
    }
    free(data);
}
void reminder_edit_callback(const char* selection, ListBoxInfo* lb) {
    int idx = lb ? lb->selected_index : -1;
    int true_idx = (lb && idx >= 0 && idx < lb->populated_count) ? lb->matching_indices[idx] : idx;
    WindowManager* wm = WindowManager_Get();
    EditCallbackData* data = calloc(1, sizeof(EditCallbackData));
    if (!data) return;
    data->filepath = ReminderHistoryFilePath;
    data->lb = lb;
    data->selected_index = idx;
    data->true_index = true_idx;
    data->new_text = NULL;
    data->restore_selection = true;
    data->hListBox = FlintContext_Get()->window.hListBox;
    data->hEditControl = FlintContext_Get()->window.hEditControl;
    data->refresh_cb = reminder_refresh_callback;
    Popup* popup = EditPopup_Create(wm, selection, "Edit Reminder", 960, 540, edit_confirm, data);
    WindowManager_PushPopup(wm, popup);
}

void TriggerReminder(void) {
    PROMPT(.title = "⏱️ Reminder",
           .refresh = reminder_refresh_callback,
           .enter = reminder_enter_callback,
           .edit = reminder_edit_callback);
}

FLINT_FEATURE_INIT {
fl_register_hotkey("Alt+S", TriggerReminder);

    fl_register_feature("Reminder", TriggerReminder);
}
