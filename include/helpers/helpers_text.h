#ifndef HELPERS_TEXT_UI_H
#define HELPERS_TEXT_UI_H

typedef struct {
    char **items;
    size_t count;
} StringList;

StringList String_Split(const char *string, const char *delimiter);
void StringList_Free(StringList *list);

#endif
