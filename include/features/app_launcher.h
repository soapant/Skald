#ifndef APP_LAUNCHER_H
#define APP_LAUNCHER_H

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
  "yazi",
  "obj",
  "build",
  NULL
};

const char* ListOfProgramPaths[] = {
#if defined(_WIN32)
    "{APPDATA}/Microsoft/Windows/Start Menu",
    "{PROGRAMDATA}/Microsoft/Windows/Start Menu/Programs",
    "{DEV}",
    "{MISC}",
    "{SystemRoot}/System32/",             
    "shell:AppsFolder",
#else
    "/usr/share/applications",
    "/usr/local/share/applications",
    "/home/diggy/.local/share/applications",
    "/usr/bin",
    "/home/diggy/.local/bin",
    "/home/diggy/Programming",
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
#ifndef MAX_APPS
#define MAX_APPS 2048
#endif

ListOfFiles s_apps = {0};

void initialize_app_cache(void) {
#if defined(_WIN32)
    char* allowed[] = {"lnk", "exe", "url", NULL};
#else
    char* allowed[] = {"desktop", "sh","exe", "url", NULL};
#endif
    for (int i = 0; i < (int)ARRAY_LEN(ListOfProgramPaths); i++) {
        if (!ListOfProgramPaths[i] || ListOfProgramPaths[i][0] == '\0') continue;
        fl_get_all_files_from_dir(ListOfProgramPaths[i], allowed, &s_apps, (char**)ListOfShitToIgnore);
    }
    
#if defined(_WIN32)
    for (int shortcut_idx = 0; shortcut_idx < SYSTEM_SHORTCUT_COUNT; shortcut_idx++) {
        if (s_apps.count >= MAX_APPS) break;
        
        bool found = false;
        for (size_t app_idx = 0; app_idx < s_apps.count; app_idx++) {
            if (fl_string_match_insensitive(s_apps.items[app_idx].path, SystemShortcuts[shortcut_idx].path)) {
                strncpy(s_apps.items[app_idx].name, SystemShortcuts[shortcut_idx].name, sizeof(s_apps.items[app_idx].name) - 1);
                s_apps.items[app_idx].name[sizeof(s_apps.items[app_idx].name) - 1] = '\0';
                found = true;
                break;
            }
        }
        if (!found) {
            if (s_apps.count >= s_apps.capacity) {
                s_apps.capacity = s_apps.capacity == 0 ? 10 : s_apps.capacity * 2;
                s_apps.items = realloc(s_apps.items, s_apps.capacity * sizeof(FileItem));
            }
            strncpy(s_apps.items[s_apps.count].name, SystemShortcuts[shortcut_idx].name, sizeof(s_apps.items[s_apps.count].name) - 1);
            s_apps.items[s_apps.count].name[sizeof(s_apps.items[s_apps.count].name) - 1] = '\0';
            
            strncpy(s_apps.items[s_apps.count].path, SystemShortcuts[shortcut_idx].path, sizeof(s_apps.items[s_apps.count].path) - 1);
            s_apps.items[s_apps.count].path[sizeof(s_apps.items[s_apps.count].path) - 1] = '\0';
            
            s_apps.count++;
        }
    }
#endif

}
void launch_app_by_name(const char* display_name) {
    for (size_t i = 0; i < s_apps.count; i++) {
        if (fl_string_match_insensitive(display_name,s_apps.items[i].name)) {
            fl_launch_with_args(s_apps.items[i].path, NULL);
            return;
        }
    }
}

void launch_app_as_admin(const char* display_name) {
    for (size_t i = 0; i < s_apps.count; i++) {
        if (fl_string_match_insensitive(s_apps.items[i].name, display_name)) {
            fl_launch_with_args_as_admin(s_apps.items[i].path, NULL);
            return;
        }
    }
}

void applauncher_enter_callback(const char* selection, ListBoxInfo* lb) {
    launch_app_by_name(selection);
}

void applauncher_ctrlenter_callback(const char* selection, ListBoxInfo* lb) {
    launch_app_as_admin(selection);
    selection_complete = true;
}

void applauncher_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char** names = (const char**)malloc(s_apps.count * sizeof(char*));
    for (size_t i = 0; i < s_apps.count; i++) {
        names[i] = s_apps.items[i].name;
    }
    PopulateListBox(lb, hListBox, filter, names, (int)s_apps.count, NULL);
    free(names);
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
        if (true_index >= 0 && (size_t)true_index < s_apps.count) {
            text = s_apps.items[true_index].path;
        }
        break;
      }
    }
    fl_copy_into_clipboard(text);
}

void TriggerAppLauncher(void) {
    const char** initial_items = (const char**)malloc(s_apps.count * sizeof(char*));
    if (!initial_items) return;
    
    for (size_t i = 0; i < s_apps.count; i++) {
        initial_items[i] = s_apps.items[i].name;
    }
    for (int i = 0; i < ARRAY_LEN(ListOfProgramPaths); i++) {
        fl_on_dir_changed(ListOfProgramPaths[i], initialize_app_cache);
    }

    PROMPT(.title = "🚀 Launcher", .items = initial_items, .count = (int)s_apps.count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = applauncher_refresh_callback,
           .enter = applauncher_enter_callback,
           .ctrl_enter = applauncher_ctrlenter_callback,
           .copy = applauncher_copy_callback);
    free(initial_items);
}

static inline void app_launcher_init(void) {
    fl_register_hotkey("Alt+E", TriggerAppLauncher);
    initialize_app_cache();
}

#endif
