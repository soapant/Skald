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
#include <shlwapi.h>
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
bool fl_get_all_files_from_dir_platform(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files, char** list_of_names_to_ignore);
char* fl_search_file_in_dir_platform(const char* directory, const char* search_string,char* output_path);
bool fl_launch_with_args_platform(const char* path, const char* args);
bool fl_launch_with_args_as_admin_platform(const char* path, const char* args);
bool fl_case_insensitive_string_match_platform(char* needle,char* haystack);

bool fl_copy_into_clipboard(const char* text);
char* fl_search_file_in_dir(const char* directory, const char* search_string,char* output_path);
bool fl_get_all_files_from_dir(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files,char** list_of_names_to_ignore);
bool fl_launch_with_args(const char* path, const char* args);
bool fl_launch_with_args_as_admin(const char* path, const char* args);
int fl_load_file_list(const char* filepath, const char* out[], int max);
bool fl_free_file_list(const char* list[], int count);
bool fl_register_hotkey(char* hotkey_string,void (*on_hotkey_trigger)(void),bool (*optional_condition_for_hotkey)(void),const char** optional_blacklistedwindows);
void fl_init_all_hotkeys(void);
bool fl_launch_terminal_with_args(char* args);
bool fl_launch_terminal_at_path(char* path,char* args);

#endif
