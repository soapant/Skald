#ifndef HELPERS_FLAG_H
#define HELPERS_FLAG_H

#include <stdbool.h>
#include "core/types.h"

typedef struct {
    char* flag;
    int index;
} FoundFlag;

int ParseTimeToSeconds(const char* input);
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

typedef struct {
    const char* prompt_title;
    const char* icon;
    bool open_neovide;
    bool open_yazi;
    bool open_explorer;
} ZoxideOptions;

int SearchFlagsInText(const char* text, const void* flag_array, int flag_count, size_t flag_size, FoundFlag* matches, int max_matches);
char* GetFlagParameter(const char* text, int flag_index);
char* GetCleanText(const char* text);
void StripCheckboxes(char* str);
int convert_string_to_int(const char *str);
void HandleCommandExecution(ListBoxInfo* lb, char* string);

#endif
