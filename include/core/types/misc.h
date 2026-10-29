#ifndef TYPES_MISC_H
#define TYPES_MISC_H

#include <stdint.h>
#include "core/types.h"

typedef struct {
    int64_t start;
    int64_t frequency;
} WinTimer;

typedef struct {
    char* initial_text;
    char* result;
    void* hFont;
    int alpha;
    WindowHandle hEdit;
} EditPopupData;

typedef enum {
    DEFAULT_BROWSER,
    DEFAULT_TERMINAL,
    DEFAULT_SHELL,
    DEFAULT_APP_COUNT
} DefaultAppKind;

#endif
