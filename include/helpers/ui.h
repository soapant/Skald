#ifndef HELPERS_UI_H
#define HELPERS_UI_H

#include "core/types.h"

typedef struct {
    char **items;
    size_t count;
} StringList;

StringList String_Split(const char *string, const char *delimiter);
void StringList_Free(StringList *list);

unsigned long rgb_to_hex(unsigned long win_color);
void eval_window_title_color(PromptState * state,unsigned long *window_title_color);
void eval_xoffset_for_alias(char* alias_text,int main_text_lenght,int *xOffsetAlias);
unsigned short OpacityAlpha16(void);

#endif