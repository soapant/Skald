#ifndef TYPES_MISC_H
#define TYPES_MISC_H

#include <stdint.h>
#include "core/types.h"

/* Platform neutral: raw tick count plus ticks-per-second. core/timer.h
   fills these in from QueryPerformanceCounter or CLOCK_MONOTONIC, which
   keeps <windows.h> out of this header (it is on the feature .so path). */
typedef struct {
    int64_t start;
    int64_t frequency;
} WinTimer;

struct feature_data {
    char* name;
    const char* last_selections[2048];
    int selection_count;
};

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
