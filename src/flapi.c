#include "common.h"
#include "helpers.h"
#include "core/platform.h"
#include "core/flapi.h"
#include "core/flapi_platform.h"
#include "core/hotkey_manager.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "features/window_switcher.h"
#include "features/google_search.h"
#include "features/todo.h"
#include "features/calc.h"
#include "features/app_launcher.h"
#include "features/process_killer.h"
#include "features/zoxide.h"
#include "features/timer.h"
#include "features/explorer.h"
#include "features/terminal_command.h"
#include "features/reminder.h"
#include "features/text_editor_zoxide.h"
#include "features/mute_microphone.h"
#include "features/windows_control.h"
#include "features/pixel_utils.h"
#include "features/screenshot.h"
#include "features/adb.h"
#include "features/misc_hotkeys.h"
#include "features/replica.h"
#include "features/quicklink.h"
#include "features/text_snippets.h"
#include "features/music_player.h"
#include "features/video_viewer.h"
#include "features/focus_switcher.h"



bool fl_copy_into_clipboard(const char* text) {
    if (text == NULL) return 0;
    return fl_copy_into_clipboard_platform(text);
}

bool fl_get_all_files_from_dir(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files,char** list_of_names_to_ignore) {
    if (!dir_path || !list_of_files) return false;

    char expanded_dir[MAX_PATH];
    if (!Platform_ExpandPath(dir_path, expanded_dir, sizeof(expanded_dir))) return false;
    dir_path = expanded_dir;

    return fl_get_all_files_from_dir_platform(dir_path, list_of_allowed_extensions, list_of_files, list_of_names_to_ignore);
}
char* fl_search_file_in_dir(const char* directory, const char* search_string,char* output_path){
    if (!directory) return NULL;

    char expanded_dir[MAX_PATH];
    if (!Platform_ExpandPath(directory, expanded_dir, sizeof(expanded_dir))) return NULL;
    directory = expanded_dir;

    return fl_search_file_in_dir_platform(directory,search_string,output_path);
}

bool fl_launch_with_args(const char* path, const char* args) {
    if (!path) return false;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) return false;
    return fl_launch_with_args_platform(expanded, args);
}

bool fl_launch_with_args_as_admin(const char* path, const char* args) {
    if (!path) return false;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) return false;
    return fl_launch_with_args_as_admin_platform(expanded, args);
}

int fl_load_file_list(const char* filepath, const char* out[], int max) {
    if (!filepath || !out || max <= 0) return 0;
    char temp[100][512];
    int count = ReadFile(temp, filepath);
    if (count > max) count = max;
    for (int i = 0; i < count; i++) {
        char* dup = strdup(temp[i]);
        if (!dup) {
            for (int j = 0; j < i; j++) free((void*)out[j]);
            return 0;
        }
        out[i] = dup;
    }
    return count;
}

bool fl_free_file_list(const char* list[], int count) {
    if (!list || count <= 0) return false;
    for (int i = 0; i < count; i++) free((void*)list[i]);
    return true;
}

bool fl_register_hotkey(char* hotkey_string,void (*on_hotkey_trigger)(void),bool (*optional_condition_for_hotkey)(void),const char** optional_blacklistedwindows) {
    HotkeyBinding binding = convert_into_hotkey_binding(hotkey_string,
                                                        on_hotkey_trigger,
                                                        optional_condition_for_hotkey,
                                                        optional_blacklistedwindows);
    return hotkey_register_internal(binding);
}

void fl_init_all_hotkeys(void) {
    window_switcher_init();
    google_search_init();
    todo_init();
    calc_init();
    app_launcher_init();
    process_killer_init();
    zoxide_init();
    timer_init();
    explorer_init();
    terminal_command_init();
    reminder_init();
    text_editor_zoxide_init();
    mute_microphone_init();
    windows_control_init();
    pixel_utils_init();
    screenshot_init();
    adb_init();
    misc_hotkeys_init();
    replica_init();
    quicklink_init();
    text_snippets_init();
    music_player_init();
    video_viewer_init();
    focus_switcher_init();
}

bool fl_launch_terminal_with_args(char* args) {

}

char* fl_run_terminal_command(char* command,bool open_terminal) {
  if (!command) return NULL;
  return Platform_RunTerminalCommand(command,open_terminal);
}

bool fl_launch_terminal_at_path(char* path,char* args) {
    if (!path) return false;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded)))return false;
    return Platform_LaunchTerminalAtPath(expanded,args);
}

bool fl_launch_file_manager_at_path(char* path,char* args) {
    if (!path) return false;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded)))return false;
    Platform_LaunchFileManagerAtPath(expanded,args);
}

char* fl_get_zoxide_match(char* query) {
  if (!query) return NULL;
  return Platform_QueryZoxidePath(query);
}

int fl_collect_running_processes(char process_list[][128], int max_count) {
    return Platform_CollectRunningProcesses(process_list, max_count);
}

bool fl_terminate_process_by_name(const char* exe_name) {
    if (!exe_name) return false;
    return Platform_TerminateProcessByName(exe_name);
}
bool fl_string_match_insensitive(char* needle,char* haystack) {
    if (!needle || !haystack) return false;
    return Platform_StringContainsCaseInsensitive(needle,haystack);
}
bool fl_launch_browser(char* url) {
  if (!url) return false;
  return Platform_LaunchBrowserAtUrl(url);

}

