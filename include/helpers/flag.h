#ifndef HELPERS_FLAG_H
#define HELPERS_FLAG_H

#include <stdbool.h>
#include "core/types.h"

typedef struct {
    char* flag;
    int index;
} FoundFlag;

int ParseTimeToSeconds(const char* input);

int SearchFlagsInText(const char* text, const void* flag_array, int flag_count, size_t flag_size, FoundFlag* matches, int max_matches);
char* GetFlagParameter(const char* text, int flag_index);
#endif
