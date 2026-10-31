#ifndef TYPES_FILE_H
#define TYPES_FILE_H

#include <stddef.h>
#include <stdbool.h>
#include "core/types.h"

typedef struct {
    char name[256];
    char path[MAX_PATH];
} FileItem;

typedef struct {
    FileItem* files;
    size_t count;
    size_t capacity;
} ListOfFiles;

#if defined(_WIN32)
typedef struct {
     ListBoxInfo listbox;
     bool        has_listbox;
     WindowHandle hListBox;
     WindowHandle hPopupWnd;
} WinPromptState;
#endif

#endif
