#ifndef HISTORY_H
#define HISTORY_H

#include <stdbool.h>
#include <stddef.h>

#define HISTORY_MAX_PER_FEATURE 100

void History_Init(void);
void History_Shutdown(void);
void History_Add(const char* feature, const char* text);
bool History_CycleUp(const char* feature, char* out, size_t out_cap);
bool History_CycleDown(const char* feature, char* out, size_t out_cap);
void History_ResetCursor(const char* feature);
int History_Count(const char* feature);
void History_Clear(const char* feature);

#endif
