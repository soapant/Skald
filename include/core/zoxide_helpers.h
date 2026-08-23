#ifndef ZOXIDE_HELPERS_H
#define ZOXIDE_HELPERS_H

#include "core/types.h"

void ZoxideHelper_RebuildCache(MappedFile* file, char*** cached_out, int* count_out);
void ZoxideHelper_Refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb, MappedFile* file, char*** cached, int* count);
void ZoxideHelper_Edit(const char* selection, ListBoxInfo* lb, MappedFile* file, char*** cached, int* count, RefreshCallback refresh);
void ZoxideHelper_AppendNewEntry(MappedFile* file, const char* selection, ListBoxInfo* lb, char*** cached, int* count);
const char** ZoxideHelper_PrepareTriggerList(MappedFile* file, char*** cached, int* count, void (*rebuild)(void));
void ZoxideHelper_CleanupTriggerList(const char** list);

#endif
