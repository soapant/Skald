#ifndef REMINDER_H
#define REMINDER_H



bool FortniteGameLoaded(const char* param);

extern ConditionParamDef custom_conditions[];

void HandleFocusWindowFlag(const char* parameter);
void DisplayText(const char* parameter);
void RunBackgroundTimer(const char* parameter);
void RunWindowTimer(const char* parameter);
bool CheckGameRunning(const char* parameter);

typedef struct {
    int max_timeout_seconds;

    ConditionFn condition_func;
    const char* condition_param;

    TimerFn     timer_func;
    const char* timer_param;

    ActionFn    action_func;
    const char* action_param;
} ExecutionPipeline;

#define REMINDER_ACTION_FLAGS_COUNT (sizeof(REMINDER_ACTION_FLAGS) / sizeof(REMINDER_ACTION_FLAGS[0]))
#define REMINDER_CONDITION_FLAGS_COUNT (sizeof(REMINDER_CONDITION_FLAGS) / sizeof(REMINDER_CONDITION_FLAGS[0]))
#define REMINDER_TIMER_FLAGS_COUNT (sizeof(REMINDER_TIMER_FLAGS) / sizeof(REMINDER_TIMER_FLAGS[0]))
#define REMINDER_CONSTRAINT_FLAGS_COUNT (sizeof(REMINDER_CONSTRAINT_FLAGS) / sizeof(REMINDER_CONSTRAINT_FLAGS[0]))

void TriggerReminder(void);

#endif
