#ifndef HELPERS_TEXT_UI_H
#define HELPERS_TEXT_UI_H

#include "helpers/helpers_text.h"
#include "core/types/file.h"
#include "core/types/window.h"
#include "core/types/keys.h"

typedef struct {
  char **items;
  size_t count;
} StringList;

StringList String_Split(const char *string, const char *delimiter);
void StringList_Free(StringList *list);
bool SafeCopy(char *dst, size_t cap, const char *src);
bool SafeCat(char *dst, size_t cap, const char *src);

long File_ReadAll(const char *path, char *buffer, long buffer_size);
StringList File_ReadLines(const char *path);
void Url_BuildSearchUrl(const char *engine, const char *query, char *out,
                        size_t cap);

bool FileWalk_AllFilesFromDir(const char *dir_path,
                              char **list_of_allowed_extensions,
                              ListOfFiles *list_of_files,
                              char **list_of_names_to_ignore,
                              bool strip_extension, int max_depth);

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

bool HotkeyText_IsPrintable(const PopupKeyEvent *key);
bool HotkeyText_ShouldSwallow(const PopupKeyEvent *key);
bool String_ContainsCI(const char *haystack, const char *needle);
bool String_StartsWithCI(const char *str, const char *prefix);
bool String_EqualsCI(const char *a, const char *b);
int Utf8_LeadLen(unsigned char c);
bool List_Append(PlWindowList *list, const char *item);
bool Window_ListAllows(int window_count, bool whitelist, char *const *windows,
                       const char *title);
#endif
