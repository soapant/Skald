#ifndef TYPES_TODO_H
#define TYPES_TODO_H

#include "core/types/color.h"

typedef struct {
    const char* alias;
    const char* alias_label;
} TodoAliasEntry;

typedef struct {
    const char* flag;
    const char* internal_name;
    const char* tag_label;
    ColorRef color;
    bool is_alias;
} TodoConfigEntry;

typedef struct {
    char tag_label[64];
    char internal_name[64];
    ColorRef color;
} TodoTag;

typedef struct {
    TodoTag tags[10];
    int tag_count;
    char text[256];
} TodoEntry;

typedef struct {
    const char* flag;
    void (*handler)(const char*);
} GenericFlagEntry;

typedef struct {
    const char* open_bracket;
    const char* close_bracket;
    const char* tag_separator;
    const char* tag_prefix;
    const char* alias_separator;
} TagFormatConfig;

#define TODO_TAG_OPEN "["
#define TODO_TAG_CLOSE "]"
#define TODO_TAG_SEPARATOR "|"
#define TODO_ENTRY_DELIMITER "||"

#endif
