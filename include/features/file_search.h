#ifndef FILE_SEARCH_H
#define FILE_SEARCH_H

#include <stdbool.h>
#include "core/types.h"

#if !defined(_WIN32)
typedef struct {
    uint32_t dwLowDateTime;
    uint32_t dwHighDateTime;
} FILETIME;
#endif

#define MAX_FILES 2000

typedef struct {
    char name[256];
    char path[MAX_PATH];
} FileItem;

typedef struct {
    char name[512];
    char path[MAX_PATH];
    FILETIME last_write;
} FileItemWithTime;

#define MAX_APPS 4000

typedef struct {
    char name[128];
    char path[MAX_PATH];
} AppItem;

extern AppItem G_APPS[MAX_APPS];
extern int G_APP_COUNT;

extern FileItemWithTime G_FILES_WITH_TIME[MAX_FILES];

char* SearchFileInDirectory(const char* directory, const char* search_string);
char* SearchFileRecursive(const char* directory, const char* search_string);
void GetFilesRecursive(const char* dir_path, bool show_folder);
void ClearFileList(void);
int GetFileCount(void);
void scan_directory_recursive(const char* dir_path);

#endif
