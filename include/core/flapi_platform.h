#ifndef FLAPI_PLATFORM_H
#define FLAPI_PLATFORM_H

#include "core/flapi.h"


int fl_copy_into_clipboard_platform(const char* text);
bool fl_get_all_files_from_dir_platform(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files, char** list_of_names_to_ignore);
char* fl_search_file_in_dir_platform(const char* directory, const char* search_string,char* output_path);
bool fl_launch_with_args_platform(const char* path, const char* args);
bool fl_launch_with_args_as_admin_platform(const char* path, const char* args);

#endif
