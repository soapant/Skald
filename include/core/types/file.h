#ifndef TYPES_FILE_H
#define TYPES_FILE_H

#include <stddef.h>
#include <stdbool.h>
#include "core/types.h"

typedef struct {
#if defined(_WIN32)
    void* hFile;
    void* hMapping;
    size_t fileSize;
    size_t capacity;
#elif defined (__linux__)
     int fd;
     size_t fileSize;
#endif
    char *pBuffer;
} MappedFile;

typedef struct {
    char *ptr;
    size_t length;
} Line;

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
