#include "core/flapi.h"

typedef struct {
    int vk_code;
    int modifiers;
    const char* exe_name;
    const char* app_name;  
} AppHotkey;

const char* ListOfShitToIgnore[] = {
  "node_modules",
  ".git",
  "bin",
  "dll",
  "gtnh 2.8",
  ".h",
  ".c",
  "yazi",
  "obj",
  "build",
};

const char* ListOfProgramPaths[] = {
#if defined(_WIN32)
    "{APPDATA}/Microsoft/Windows/Start Menu",
    "{PROGRAMDATA}/Microsoft/Windows/Start Menu/Programs",
    "{MISC}",
    "{SystemRoot}/System32/",             
    "shell:AppsFolder",
#else
    "/usr/share/applications",
    "/home/diggy/.local/share/applications",
    "/usr/bin",
    "/home/diggy/.local/bin",
    "/home/diggy/miscellaneous",
#endif
};

const PathEntry SystemShortcuts[] = {
    {"control.exe", "Control Panel"},
    {"devmgmt.msc", "Device Manager"},
    {"compmgmt.msc", "Computer Management"},
    {"diskmgmt.msc", "Disk Management"},
    {"services.msc", "Services"},
    {"taskmgr.exe", "Task Manager"},
    {"notepad.exe", "Notepad"},
    {"calc.exe", "Calculator"},
    {"mspaint.exe", "Paint"},
    {"cmd.exe", "Command Prompt"},
    {"powershell.exe", "PowerShell"},
    {"msconfig.exe", "System Configuration"},
    {"dxdiag.exe", "DirectX Diagnostic Tool"},
    {"sysdm.cpl", "System Properties"},
    {"inetcpl.cpl", "Internet Options"},
    {"appwiz.cpl", "Programs and Features"},
    {"timedate.cpl", "Date and Time"},
    {"desk.cpl", "Display Settings"},
    {"main.cpl", "Mouse Properties"}
};

#define SYSTEM_SHORTCUT_COUNT ARRAY_LEN(SystemShortcuts)
#define MAX_APPS 2048

ListOfFiles s_apps = {0};

void initialize_app_cache(void) {
#if defined(_WIN32)
    char* allowed[] = {"lnk", "exe", "url", NULL};
#else
    char* allowed[] = {"desktop",NULL};
#endif
    ListOfFiles tmp = {0};
    for (int i = 0; i < (int)ARRAY_LEN(ListOfProgramPaths); i++) {
        if (!ListOfProgramPaths[i] || ListOfProgramPaths[i][0] == '\0') continue;
        fl_get_all_files_from_dir(ListOfProgramPaths[i], allowed, &tmp, (char**)ListOfShitToIgnore);
    }
    for (size_t i = 0; i < tmp.count; ) {
        bool skip = false;
        char display_name[256] = {0};
        bool has_display = false;
        if (strstr(tmp.items[i].path, ".desktop")) {
            bool nodisplay=false;
            if (fl_parse_desktop_file(tmp.items[i].path, display_name, sizeof(display_name), &nodisplay)) has_display=true;
            if (nodisplay) skip=true;
        }
        if (skip) {
            for (size_t k = i; k + 1 < tmp.count; k++) tmp.items[k] = tmp.items[k+1];
            tmp.count--;
            continue;
        }
        if (has_display && display_name[0]) {
            snprintf(tmp.items[i].name, sizeof(tmp.items[i].name), "%s", display_name);
        }
        i++;
    }
    for (size_t i = 0; i < tmp.count; i++) {
        for (size_t j = i + 1; j < tmp.count; ) {
            if (fl_string_match_insensitive(tmp.items[i].name, tmp.items[j].name)) {
                for (size_t k = j; k + 1 < tmp.count; k++) tmp.items[k] = tmp.items[k+1];
                tmp.count--;
            } else {
                j++;
            }
        }
    }
#if defined(_WIN32)
    for (int shortcut_idx = 0; shortcut_idx < SYSTEM_SHORTCUT_COUNT; shortcut_idx++) {
        if (tmp.count >= MAX_APPS) break;
        
        bool found = false;
        for (size_t app_idx = 0; app_idx < tmp.count; app_idx++) {
            if (fl_string_match_insensitive(tmp.items[app_idx].path, SystemShortcuts[shortcut_idx].path)) {
                strncpy(tmp.items[app_idx].name, SystemShortcuts[shortcut_idx].name, sizeof(tmp.items[app_idx].name) - 1);
                tmp.items[app_idx].name[sizeof(tmp.items[app_idx].name) - 1] = '\0';
                found = true;
                break;
            }
        }
        if (!found) {
            if (tmp.count >= tmp.capacity) {
                tmp.capacity = tmp.capacity == 0 ? 10 : tmp.capacity * 2;
                tmp.items = realloc(tmp.items, tmp.capacity * sizeof(FileItem));
            }
            strncpy(tmp.items[tmp.count].name, SystemShortcuts[shortcut_idx].name, sizeof(tmp.items[tmp.count].name) - 1);
            tmp.items[tmp.count].name[sizeof(tmp.items[tmp.count].name) - 1] = '\0';
            
            strncpy(tmp.items[tmp.count].path, SystemShortcuts[shortcut_idx].path, sizeof(tmp.items[tmp.count].path) - 1);
            tmp.items[tmp.count].path[sizeof(tmp.items[tmp.count].path) - 1] = '\0';
            
            tmp.count++;
        }
    }
#endif
    if (s_apps.items) free(s_apps.items);
    s_apps = tmp;
}

void launch_app_by_name(const char* display_name) {
    char path_copy[512] = {0};
    bool found = false;
    for (size_t i = 0; i < s_apps.count; i++) {
        if (fl_string_match_insensitive(display_name,s_apps.items[i].name)) {
            strncpy(path_copy, s_apps.items[i].path, sizeof(path_copy)-1);
            found = true;
            break;
        }
    }
    if (found) fl_launch_with_args(path_copy, NULL);
}

void launch_app_as_admin(const char* display_name) {
    char path_copy[512] = {0};
    bool found = false;
    for (size_t i = 0; i < s_apps.count; i++) {
        if (fl_string_match_insensitive(s_apps.items[i].name, display_name)) {
            strncpy(path_copy, s_apps.items[i].path, sizeof(path_copy)-1);
            found = true;
            break;
        }
    }
    if (found) fl_launch_with_args_as_admin(path_copy, NULL);
}
bool applauncher_enter_callback(const char* selection, ListBoxInfo* lb) {
    launch_app_by_name(selection);
    return true;
}

bool applauncher_ctrlenter_callback(const char* selection, ListBoxInfo* lb) {
    launch_app_as_admin(selection);
    return true;
}

void applauncher_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    size_t cnt = s_apps.count;
    
    if (cnt > 2048) {
        const char** names = (const char**)malloc(cnt * sizeof(char*));
        if (!names) return;
        for (size_t i = 0; i < cnt && i < s_apps.count; i++) names[i] = s_apps.items[i].name;
        PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, names, (int)cnt, NULL);
        free(names);
        return;
    }
    const char* names[2048];
    for (size_t i = 0; i < cnt && i < s_apps.count; i++) names[i] = s_apps.items[i].name;
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, names, (int)cnt, NULL);
}

void applauncher_copy_callback(const char* selection,int true_index,int type) {
    char* text = (char*)selection;
    switch(type) {
      case 0: {
        int start = strlen(text);
        for (int i = start; i > 0 ; i--) {
          if (text[i] == '\\') {
            for (int j = i; j < start; ++j){
                text[j] = '\0';
            }
            break;
          }
        }
        break;
      }
      case 1: {
        char buffer[512];
        strncpy(buffer, s_apps.items[true_index].path, sizeof(buffer) - 1);
        text = strdup(buffer);
        break;
      }
    }
    fl_copy_to_clipboard(text);
}

void TriggerAppLauncher(void) {
    // for (size_t i = 0; i < ARRAY_LEN(ListOfProgramPaths); i++) {
    //     fl_on_dir_changed(ListOfProgramPaths[i], initialize_app_cache);
    // }
    PROMPT(.title = "🚀 AppLauncher",
           .refresh = applauncher_refresh_callback,
           .enter = applauncher_enter_callback,
           .ctrl_enter = applauncher_ctrlenter_callback,
           .copy = applauncher_copy_callback);
}

FLINT_FEATURE_INIT {
    // initialize_app_cache();
    fl_register_hotkey("Alt+E", TriggerAppLauncher,"Fortnite");
    fl_register_feature("App Launcher", TriggerAppLauncher);
}
