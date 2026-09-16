#ifndef TYPES_FILE_H
#define TYPES_FILE_H

#include <stddef.h>
#include <stdbool.h>
#if defined(_WIN32)
#include <windows.h>
#endif
#include "core/types.h"

typedef struct {
#if defined(_WIN32)
    HANDLE hFile;
    HANDLE hMapping;
    size_t fileSize;   
    size_t capacity;   
#else 
     int fd;           
     size_t fileSize;
#endif
    char *pBuffer;
} MappedFile;

typedef struct {
    char *ptr;
    size_t length;
} Line;

#if defined(_WIN32)
typedef struct {
     ListBoxInfo listbox;
     bool        has_listbox;
     WindowHandle hListBox;
     WindowHandle hEditControl;
     WindowHandle hPopupWnd;
} WinPromptState;
#endif

#if defined(_WIN32)
#define DATA_DIR "data\\"
#else
#define DATA_DIR "data/"
#endif
#define TODO_FILE_PATH DATA_DIR "todo.txt"
#define FORGETFULL_FILE_PATH DATA_DIR "forgetfull.txt"

#endif
