#ifndef HELPERS_ALIAS_H
#define HELPERS_ALIAS_H

#include "core/types.h"

typedef struct {
    char* item_name;
    int item_index;
    char* alias_name; 
} AliasInfo;

typedef struct {
    AliasInfo matches[1024];
    int count;
} AliasMatchList;

int UI_LookupTrueIndex(const char* name);
bool GetFromHistory(ListBoxInfo* lb);
const char* get_alias_for_item(const char* item);
char** StringList_Append(char** list, int* count, const char* str);

#endif