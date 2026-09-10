#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#if defined(_WIN32)
#include <windows.h>
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
#include "config.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include "config.h"
#include "core/platform.h"
#include "core/common_ui.h"
#include "core/window_manager.h"
#include "core/flapi.h"
#include "core/flapi_platform.h"
#include "core/hotkey_manager.h"



bool fl_copy_to_clipboard(const char* text) {
    if (text == NULL) return 0;
    return Platform_CopyToClipboard(text);
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

char* fl_read_file_to_string(const char* path, size_t* out_len) {
    if (!path) return NULL;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) strncpy(expanded, path, sizeof(expanded)-1);
    FILE* f = fopen(expanded, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END)!=0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) sz = 0;
    rewind(f);
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t read = fread(buf, 1, (size_t)sz, f);
    buf[read] = '\0';
    fclose(f);
    if (out_len) *out_len = read;
    return buf;
}
bool fl_write_string_to_file(const char* path, const char* content) {
    if (!path || !content) return false;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) strncpy(expanded, path, sizeof(expanded)-1);
    FILE* f = fopen(expanded, "w");
    if (!f) return false;
    fputs(content, f);
    fclose(f);
    return true;
}
bool fl_file_exists(const char* path) {
    if (!path) return false;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) strncpy(expanded, path, sizeof(expanded)-1);
    FILE* f = fopen(expanded, "rb");
    if (f) { fclose(f); return true; }
    return false;
}
bool fl_append_unique_line(const char* path, const char* line) {
    if (!path || !line) return false;
    const char* items[1024]; int n = fl_load_file_list(path, items, 1024);
    for (int i=0;i<n;i++) if (strcmp(items[i], line)==0) { fl_free_file_list(items, n); return false; }
    bool ok = fl_append_to_file(path, line);
    fl_free_file_list(items, n);
    return ok;
}
bool fl_remove_line(const char* path, const char* line) {
    if (!path || !line) return false;
    const char* items[1024]; int n = fl_load_file_list(path, items, 1024);
    const char* kept[1024]; int k=0;
    for (int i=0;i<n;i++) if (strcmp(items[i], line)!=0) kept[k++]=items[i]; else { free((void*)items[i]); items[i]=NULL; }
    bool ok = fl_write_to_file(path, kept, k);
    for (int i=0;i<n;i++) if (items[i]) free((void*)items[i]);
    return ok;
}
bool fl_remove_line_contains(const char* path, const char* substr) {
    if (!path || !substr) return false;
    const char* items[1024]; int n = fl_load_file_list(path, items, 1024);
    const char* kept[1024]; int k=0;
    for (int i=0;i<n;i++) if (!strstr(items[i], substr)) kept[k++]=items[i]; else { free((void*)items[i]); items[i]=NULL; }
    bool ok = fl_write_to_file(path, kept, k);
    for (int i=0;i<n;i++) if (items[i]) free((void*)items[i]);
    return ok;
}
bool fl_parse_desktop_file(const char* path, char* out_name, size_t n, bool* out_nodisplay) {
    if (!path || !out_name) return false;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) strncpy(expanded, path, sizeof(expanded)-1);
    FILE* f = fopen(expanded, "r");
    if (!f) return false;
    char line[1024]; bool found=false;
    if (out_nodisplay) *out_nodisplay=false;
    while(fgets(line,sizeof(line),f)){
        char* p=line; while(*p==' '||*p=='\t') p++;
        if(Platform_StringContainsCaseInsensitive(p, "NoDisplay=true") || Platform_StringContainsCaseInsensitive(p, "Hidden=true")){
            if(out_nodisplay) *out_nodisplay=true;
        }
        if(!found && Platform_StringContainsCaseInsensitive(p, "Name=")){
            p+=5; while(*p==' '||*p=='\t') p++;
            p[strcspn(p,"\r\n")]='\0';
            strncpy(out_name, p, n-1); out_name[n-1]='\0'; found=true;
        }
    }
    fclose(f);
    return found;
}


bool fl_register_hotkey_impl(char* hotkey_string,void (*on_hotkey_trigger)(void),bool (*optional_condition_for_hotkey)(void),bool whitelist,const char** optional_blacklistedwindows) {
    HotkeyBinding binding = convert_into_hotkey_binding(hotkey_string,
                                                        on_hotkey_trigger,
                                                        optional_condition_for_hotkey,
                                                        optional_blacklistedwindows,
                                                        whitelist);
    return hotkey_register_internal(binding);
}

typedef struct { char name[128]; void (*cb)(void); char icon[512]; } FeatureReg;
static FeatureReg g_feature_regs[1000];
static int g_feature_reg_count = 0;

void fl_register_feature(const char* name, void (*cb)(void)) {
    fl_register_feature_with_icon(name, cb, NULL);
}
void fl_register_feature_with_icon(const char* name, void (*cb)(void), const char* icon_path) {
    if (!name || !cb) return;
    if (g_feature_reg_count >= MaxFeatures) return;
    for (int i=0;i<g_feature_reg_count;i++) if (strcmp(g_feature_regs[i].name, name)==0) return;
    strncpy(g_feature_regs[g_feature_reg_count].name, name, sizeof(g_feature_regs[g_feature_reg_count].name)-1);
    g_feature_regs[g_feature_reg_count].cb = cb;
    if (icon_path) strncpy(g_feature_regs[g_feature_reg_count].icon, icon_path, sizeof(g_feature_regs[g_feature_reg_count].icon)-1);
    else g_feature_regs[g_feature_reg_count].icon[0]='\0';
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

int fl_get_feature_list_with_icons(const char** out_names, void (**out_cbs)(void), const char*** out_icons, int max) {
    int n = g_feature_reg_count < max ? g_feature_reg_count : max;
    for (int i=0;i<n;i++) {
        if (out_names) out_names[i] = g_feature_regs[i].name;
        if (out_cbs) out_cbs[i] = g_feature_regs[i].cb;
        if (out_icons) *out_icons = (const char**)g_feature_regs[i].icon;
    }
    static const char* icon_ptrs[1000];
    for (int i=0;i<n;i++) icon_ptrs[i] = g_feature_regs[i].icon[0] ? g_feature_regs[i].icon : NULL;
    if (out_icons) *out_icons = icon_ptrs;
    return n;
}
char* fl_get_icon_path(const char* desktop_or_icon_path) {
    if (!desktop_or_icon_path) return NULL;
    return Platform_GetIconPath((char*)desktop_or_icon_path);
}
int fl_get_icon_size(const char* icon_path) {
    if (!icon_path) return 20;
    return Platform_GetIconSize((char*)icon_path);
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
bool fl_query_browser(const char* query) {
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

void fl_edit_confirm_callback(const char* new_text, void* user_data) {
    EditCallbackData* data = (EditCallbackData*)user_data;
    if (new_text && data) {
        data->new_text = (char*)new_text;
        EditCallback_Handle(data, data->refresh_cb);
    }
    free(data);
}

bool fl_edit_text_popup(const char* initial_text, const char* title, int x, int y, const char* filepath, ListBoxInfo* lb, RefreshCallback refresh) {
    if (!title) title = "Edit";
    if (!initial_text) initial_text = "";
    if (!lb) return false;
    int idx = lb->selected_index;
    int true_idx = (idx >= 0 && idx < lb->populated_count) ? lb->matching_indices[idx] : idx;
    WindowManager* wm = WindowManager_Get();
    if (!wm) return false;
    EditCallbackData* data = (EditCallbackData*)calloc(1, sizeof(EditCallbackData));
    if (!data) return false;
    data->filepath = filepath;
    data->lb = lb;
    data->selected_index = idx;
    data->true_index = true_idx;
    data->new_text = NULL;
    data->restore_selection = true;
    data->hListBox = FlintContext_Get()->window.hListBox;
    data->hEditControl = FlintContext_Get()->window.hEditControl;
    data->refresh_cb = refresh;
    Popup* popup = EditPopup_Create(wm, initial_text, title, x, y, fl_edit_confirm_callback, data);
    if (!popup) {
        free(data);
        return false;
    }
    WindowManager_PushPopup(wm, popup);
    return true;
}
bool fl_focus_window_by_title(char* title) {
  if (!title) return false;
  return Platform_FocusWindow(title);
}


