#ifndef FLAPI_H
#define FLAPI_H

#if defined(_WIN32)
#include <windows.h>
#endif
#include <stdbool.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <shellapi.h>
#include <shlwapi.h>
#include <psapi.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pwd.h>
#include <dirent.h>
#include <pthread.h>
#endif
#include "core/context.h"
#include "core/modal.h"
#include "helpers.h"
#include "config.h"
#include "core/platform.h"
#include "platform/window.h"
#include "common_ui.h"

#if !defined(MAX_PATH)
#define MAX_PATH 260
#endif

#define WHITELIST 1
#define BLACKLIST 0

typedef struct {
    char name[256];
    char path[MAX_PATH];
} FileItem;

typedef struct {
    FileItem* items;
    size_t count;
    size_t capacity;
} ListOfFiles;

bool fl_copy_into_clipboard(const char* text);
char* fl_search_file_in_dir(const char* directory, const char* search_string,char* output_path);
bool fl_get_all_files_from_dir(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files,char** list_of_names_to_ignore);
bool fl_launch_with_args(const char* path, const char* args);
bool fl_launch_with_args_as_admin(const char* path, const char* args);
int fl_load_file_list(const char* filepath, const char* out[], int max);
bool fl_free_file_list(const char* list[], int count);
bool fl_append_to_file(const char* filepath, const char* line);
bool fl_write_to_file(const char* filepath, const char* lines[], int count);
bool fl_register_hotkey_impl(char* hotkey_string,void (*on_hotkey_trigger)(void),bool (*optional_condition_for_hotkey)(void),bool whitelist,const char** optional_blacklistedwindows);
void fl_init_all_hotkeys(void);
bool fl_launch_terminal_with_args(char* args);
bool fl_launch_terminal_at_path(char* path,char* args);
bool fl_launch_file_manager_at_path(char* path,char* args);
char* fl_get_zoxide_match(char* query);
char* fl_run_terminal_command(char* command);
char* fl_run_terminal_command_and_open(char* command);
int fl_collect_running_processes(char process_list[][128], int max_count);
bool fl_terminate_process_by_name(const char* exe_name);
bool fl_string_match_insensitive(char* needle,char* haystack);
bool fl_launch_browser(char* url);
bool fl_launch_browser_in_a_new_window(char* url);
char* fl_on_dir_changed(char* dir_path,void(callback)(void));
char* fl_adb_tap(int x,int y);
char* fl_adb_type_text(char* text);
char* fl_adb_get_devices();
char* fl_adb_install(char* app_path);
char* fl_adb_uninstall(char* app_name);
char* fl_adb_force_close(char* app_name);
char* fl_adb_clear_app_data(char* app_name);
char* fl_adb_get_file(char* filepath,char* output_file_path);
char* fl_adb_put_file(char* filepath,char* output_file_path);
char* fl_adb_reboot();

#define _FL_HOTKEY_EXPAND(hotkey, trigger, cond, whitelist, ...) \
    fl_register_hotkey_impl(hotkey, trigger, cond, whitelist, (const char*[]){__VA_ARGS__, NULL})

#define _FL_AS_COND(x)   _Generic((x)+0, int: (bool(*)(void))NULL, default: (bool(*)(void))(x))
#define _FL_AS_BOOL(x)   _Generic((x)+0, int: (bool)(x), default: false)
#define _FL_AS_STR(x)    _Generic((x)+0, int: (const char*)NULL, default: (const char*)(x))

#define _FL_HOTKEY_ROUTER_2(hk, tr)              _FL_HOTKEY_EXPAND(hk, tr, NULL, 0, NULL)
#define _FL_HOTKEY_ROUTER_3(hk, tr, s1)          _FL_HOTKEY_EXPAND(hk, tr, NULL, 0, s1)

#define _FL_HOTKEY_ROUTER_4(hk, tr, arg3, arg4) \
    _Generic((arg3) + 0, \
        int: _FL_HOTKEY_EXPAND(hk, tr, NULL, _FL_AS_BOOL(arg3), _FL_AS_STR(arg4)), \
        default: _FL_HOTKEY_EXPAND(hk, tr, _FL_AS_COND(arg3), false, _FL_AS_STR(arg4)) \
    )

#define _FL_HOTKEY_ROUTER_5(hk, tr, cond, wl, s1) _FL_HOTKEY_EXPAND(hk, tr, cond, wl, s1)

#define _FL_GET_MACRO(_1, _2, _3, _4, _5, NAME, ...) NAME

#define fl_register_hotkey(...) \
    _FL_GET_MACRO(__VA_ARGS__, _FL_HOTKEY_ROUTER_5, _FL_HOTKEY_ROUTER_4, _FL_HOTKEY_ROUTER_3, _FL_HOTKEY_ROUTER_2)(__VA_ARGS__)


#if defined(_WIN32)
#define FLINT_FEATURE_EXPORT static
#else
#define FLINT_FEATURE_EXPORT __attribute__((visibility("default")))
#endif

#define FLINT_FEATURE_INIT FLINT_FEATURE_EXPORT __attribute__((constructor)) void FlintFeatureInit(void)

#endif
