#ifndef HELPERS_TEXT_UI_H
#define HELPERS_TEXT_UI_H

typedef struct {
    char **items;
    size_t count;
} StringList;

StringList String_Split(const char *string, const char *delimiter);
void StringList_Free(StringList *list);

long File_ReadAll(const char *path, char *buffer, long buffer_size);
StringList File_ReadLines(const char *path, long buffer_size);

char *String_CutAt(const char *str, char delim);
char *String_CutBefore(const char *str, char delim);
char *String_CutAfter(const char *str, char delim);
char *String_Between(const char *str, char open, char close);
char *String_Inside(const char *str, char delim);
char *String_Trim(const char *str);
char *String_ToLower(const char *str);
char *String_ToUpper(const char *str);
bool String_StartsWith(const char *str, const char *prefix);
bool String_EndsWith(const char *str, const char *suffix);
char *String_Replace(const char *str, const char *from, const char *to);
char *String_Slice(const char *str, int start, int end);

#endif
