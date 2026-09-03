#ifndef ZOXIDE_HELPERS_H
#define ZOXIDE_HELPERS_H

#include "core/types.h"

void ZoxideHelper_RebuildCache(const char* filepath, char*** cached_out, int* count_out);
void ZoxideHelper_Refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb, const char* filepath, char*** cached, int* count);
void ZoxideHelper_Edit(const char* selection, ListBoxInfo* lb, const char* filepath, char*** cached, int* count, RefreshCallback refresh);
void ZoxideHelper_AppendNewEntry(const char* filepath, const char* selection, ListBoxInfo* lb, char*** cached, int* count);
const char** ZoxideHelper_PrepareTriggerList(const char* filepath, char*** cached, int* count, void (*rebuild)(void));
void ZoxideHelper_CleanupTriggerList(const char** list);

#endif
