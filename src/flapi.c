#include "core/context.h"
#include "core/modal.h"
#include "helpers.h"
#include "config.h"
#include "core/platform.h"
#include "core/flapi.h"
#include "core/flapi_platform.h"
#include "core/hotkey_manager.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



bool fl_copy_to_clipboard(const char* text) {
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
    FILE* f = fopen(filepath, "r");
    if (!f) return 0;
    int count = 0;
    char buffer[MAX_FILE_LINE_LENGTH];
    while (count < max && fgets(buffer, sizeof(buffer), f)) {
        buffer[strcspn(buffer, "\r\n")] = '\0';
        if (buffer[0] == '\0') continue;
        char* dup = strdup(buffer);
        if (!dup) {
            for (int j = 0; j < count; j++) free((void*)out[j]);
            fclose(f);
            return 0;
        }
        out[count++] = dup;
    }
    fclose(f);
    return count;
}

bool fl_append_to_file(const char* filepath, const char* line) {
    if (!filepath || !line) return false;
    FILE* f = fopen(filepath, "a");
    if (!f) {
        f = fopen(filepath, "w");
        if (!f) return false;
    }
    fclose(f);
    f = fopen(filepath, "r");
    bool need_newline = false;
    if (f) {
        if (fseek(f, 0, SEEK_END) == 0) {
            long sz = ftell(f);
            if (sz > 0) {
                if (fseek(f, -1, SEEK_END) == 0) {
                    int c = fgetc(f);
                    if (c != '\n' && c != '\r') need_newline = true;
                }
            }
        }
        fclose(f);
    }
    f = fopen(filepath, "a");
    if (!f) return false;
    if (need_newline) fputc('\n', f);
    fputs(line, f);
    fputc('\n', f);
    fclose(f);
    return true;
}

bool fl_write_to_file(const char* filepath, const char* lines[], int count) {
    if (!filepath) return false;
    if (count < 0) return false;
    if (count > 0 && !lines) return false;
    FILE* f = fopen(filepath, "w");
    if (!f) return false;
    for (int i = 0; i < count; i++) {
        if (lines[i]) fprintf(f, "%s\n", lines[i]);
        else fprintf(f, "\n");
    }
    fclose(f);
    return true;
}

bool fl_free_file_list(const char* list[], int count) {
    if (!list || count <= 0) return false;
    for (int i = 0; i < count; i++) free((void*)list[i]);
    return true;
}


bool fl_register_hotkey_impl(char* hotkey_string,void (*on_hotkey_trigger)(void),bool (*optional_condition_for_hotkey)(void),bool whitelist,const char** optional_blacklistedwindows) {
    HotkeyBinding binding = convert_into_hotkey_binding(hotkey_string,
                                                        on_hotkey_trigger,
                                                        optional_condition_for_hotkey,
                                                        optional_blacklistedwindows,
                                                        whitelist);
    return hotkey_register_internal(binding);
}

#define MAX_FEATURE_REG 128
typedef struct { char name[128]; void (*cb)(void); } FeatureReg;
static FeatureReg g_feature_regs[MAX_FEATURE_REG];
static int g_feature_reg_count = 0;

void fl_register_feature(const char* name, void (*cb)(void)) {
    if (!name || !cb) return;
    if (g_feature_reg_count >= MAX_FEATURE_REG) return;
    for (int i=0;i<g_feature_reg_count;i++) if (strcmp(g_feature_regs[i].name, name)==0) return;
    strncpy(g_feature_regs[g_feature_reg_count].name, name, sizeof(g_feature_regs[g_feature_reg_count].name)-1);
    g_feature_regs[g_feature_reg_count].cb = cb;
    g_feature_reg_count++;
}

int fl_get_feature_list(const char** out_names, void (**out_cbs)(void), int max) {
    int n = g_feature_reg_count < max ? g_feature_reg_count : max;
    for (int i=0;i<n;i++) {
        if (out_names) out_names[i] = g_feature_regs[i].name;
        if (out_cbs) out_cbs[i] = g_feature_regs[i].cb;
    }
    return n;
}


bool fl_launch_terminal_with_args(char* args) {

}

char* fl_run_terminal_command(char* command) {
  if (!command) return NULL;
  return Platform_RunTerminalCommand(command,false);
}

char* fl_run_terminal_command_and_open(char* command) {
  if (!command) return NULL;
  return Platform_RunTerminalCommand(command,true);
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
    return Platform_LaunchFileManagerAtPath(expanded,args);
}

char* fl_get_zoxide_match(char* query) {
  if (!query) return NULL;
  return Platform_QueryZoxidePath(query);
}

int fl_collect_running_processes(char process_list[][MaxFeatures], int max_count) {
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
bool fl_launch_browser_window(char* url) {
  if (!url) return false;
  return Platform_LaunchBrowserAtUrl(url,true);
}

bool fl_launch_browser(char* url) {
  if (!url) return false;
  return Platform_LaunchBrowserAtUrl(url,false);
}
bool fl_query_browser(char* query) {
    if (!query) return false;
    return Platform_QueryBrowser(query);
}

bool fl_click(int x,int y) {
  return Platform_ClickAtPos(x,y);

}
bool fl_move(int x,int y) {

}
FlColor fl_get_color(int x,int y) {

}
bool fl_compare_color(FlColor color,int x,int y) {

}

char* fl_on_dir_changed(char* dir_path, void (*callback)(void)) {
    if (!dir_path) return NULL;
    return Platform_RegisterDirWatcher(dir_path, callback);
}

char* fl_adb_tap(int x,int y) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb shell input tap %i %i",x,y);
  return Platform_RunTerminalCommand(buffer,false);
}
char* fl_adb_type_text(char* text) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb shell input text %s",text);
  return Platform_RunTerminalCommand(buffer,false);
}
char* fl_adb_get_devices() {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb devices");
  return Platform_RunTerminalCommand(buffer,false);
}

char* fl_adb_install(char* app_path) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb install %s",app_path);
  return Platform_RunTerminalCommand(buffer,false);

}
char* fl_adb_uninstall(char* app_name) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb uninstall %s",app_name);
  return Platform_RunTerminalCommand(buffer,false);

}
char* fl_adb_force_close(char* app_name) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb shell am force-stop %s",app_name);
  return Platform_RunTerminalCommand(buffer,false);

}
char* fl_adb_clear_app_data(char* app_name) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb shell pm clear %s",app_name);
  return Platform_RunTerminalCommand(buffer,false);
}
char* fl_adb_get_file(char* filepath,char* output_file_path) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb pull %s %s",filepath,output_file_path);
  return Platform_RunTerminalCommand(buffer,false);

}
char* fl_adb_put_file(char* filepath,char* output_file_path) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb push %s %s",filepath,output_file_path);
  return Platform_RunTerminalCommand(buffer,false);

}

char* fl_adb_reboot() {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb reboot");
  return Platform_RunTerminalCommand(buffer,false);

}
char* fl_get_last_clipboard_entry() {
   return Platform_GetLastClipboardEntry();
}
bool fl_paste_from_clipboard() {
   char* content = Platform_GetLastClipboardEntry();
   return Platform_PasteContent(content);
}
bool fl_paste(char* content) {
    if (!content) return false;
   return Platform_PasteContent(content);
}




