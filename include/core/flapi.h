#ifndef FLAPI_H
#define FLAPI_H

#include "common.h"
#include "helpers.h"
#include "core/common_ui.h"
#include "core/window_manager.h"
#include "core/platform.h"
#include "config.h"
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#if defined(_WIN32)
#include <shellapi.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pwd.h>
#include <dirent.h> 
#include <pthread.h>
#endif

typedef struct {
    char name[256];
    char path[MAX_PATH];
} FileItem;

typedef struct {
    FileItem* items;
    size_t count;
    size_t capacity;
} ListOfFiles;

int fl_copy_into_clipboard_platform(const char* text);
void fl_get_all_files_from_dir_platform(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files, char** list_of_names_to_ignore);
char* fl_search_file_in_dir_platform(const char* directory, const char* search_string,char* output_path);

int fl_copy_into_clipboard(const char* text);

#endif
