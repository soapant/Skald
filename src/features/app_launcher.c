#include "core/flapi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
#define APPCACHE_FILE "app_launcher_cache.txt"

ListOfFiles s_apps = {0};

static bool app_cache_grow(ListOfFiles* l) {
    size_t new_cap = l->capacity ? l->capacity * 2 : 64;
    FileItem* grown = (FileItem*)realloc(l->files, new_cap * sizeof(FileItem));
    if (!grown) return false;
    l->files = grown;
    l->capacity = new_cap;
    return true;
}

static void save_app_cache(void) {
    size_t n = s_apps.count;
    if (n == 0) return;
    size_t total = 0;
    for (size_t i = 0; i < n; i++) {
        total += strlen(s_apps.files[i].name) + 1;
        total += strlen(s_apps.files[i].path) + 2;
    }
    const char** lines = (const char**)malloc(n * sizeof(char*));
    char* store = (char*)malloc(total);
    if (!lines || !store) { free(lines); free(store); return; }
    size_t off = 0;
    size_t written = 0;
    for (size_t i = 0; i < n; i++) {
        const char* nm = s_apps.files[i].name;
        const char* pth = s_apps.files[i].path;
        if (!nm[0] || !pth[0]) continue;
        int k = snprintf(store + off, total - off, "%s;%s", nm, pth);
        if (k <= 0 || (size_t)k >= total - off) continue;
        lines[written++] = store + off;
        off += (size_t)k + 1;
    }
    bool ok = fl_write_to_file(APPCACHE_FILE, lines, (int)written);
    free(store);
    free(lines);
    printf("[app_launcher] cache saved entries=%zu ok=%d\n", written, ok ? 1 : 0);
}

static bool load_app_cache(void) {
    FlStringList lines = fl_load_file_list(APPCACHE_FILE);
    if (!lines.items || lines.count <= 0) {
        if (lines.items) fl_free_file_list(&lines);
        return false;
    }
    ListOfFiles tmp = {0};
    for (int i = 0; i < lines.count; i++) {
        char* entry = lines.items[i];
        if (!entry) continue;
        char* colon = strrchr(entry, ';');
        if (!colon || colon == entry || !colon[1]) continue;
        *colon = '\0';
        if (tmp.count >= tmp.capacity && !app_cache_grow(&tmp)) break;
        strncpy(tmp.files[tmp.count].name, entry, sizeof(tmp.files[tmp.count].name) - 1);
        tmp.files[tmp.count].name[sizeof(tmp.files[tmp.count].name) - 1] = '\0';
        strncpy(tmp.files[tmp.count].path, colon + 1, sizeof(tmp.files[tmp.count].path) - 1);
        tmp.files[tmp.count].path[sizeof(tmp.files[tmp.count].path) - 1] = '\0';
        tmp.count++;
    }
    fl_free_file_list(&lines);
    if (tmp.count == 0) {
        free(tmp.files);
        return false;
    }
    if (s_apps.files) free(s_apps.files);
    s_apps = tmp;
    return true;
}

static void scan_app_dirs(void) {
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
        if (strstr(tmp.files[i].path, ".desktop")) {
            bool nodisplay=false;
            if (fl_parse_desktop_file(tmp.files[i].path, display_name, sizeof(display_name), &nodisplay)) has_display=true;
            if (nodisplay) skip=true;
        }
        if (skip) {
            for (size_t k = i; k + 1 < tmp.count; k++) tmp.files[k] = tmp.files[k+1];
            tmp.count--;
            continue;
        }
        if (has_display && display_name[0]) {
            snprintf(tmp.files[i].name, sizeof(tmp.files[i].name), "%s", display_name);
        }
        i++;
    }
    for (size_t i = 0; i < tmp.count; i++) {
        for (size_t j = i + 1; j < tmp.count; ) {
            if (fl_string_contains(tmp.files[i].name, tmp.files[j].name)) {
                for (size_t k = j; k + 1 < tmp.count; k++) tmp.files[k] = tmp.files[k+1];
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
            if (fl_string_contains(tmp.files[app_idx].path, SystemShortcuts[shortcut_idx].path)) {
                strncpy(tmp.files[app_idx].name, SystemShortcuts[shortcut_idx].name, sizeof(tmp.files[app_idx].name) - 1);
                tmp.files[app_idx].name[sizeof(tmp.files[app_idx].name) - 1] = '\0';
                found = true;
                break;
            }
        }
        if (!found) {
            if (tmp.count >= tmp.capacity) {
                tmp.capacity = tmp.capacity == 0 ? 10 : tmp.capacity * 2;
                tmp.files = realloc(tmp.files, tmp.capacity * sizeof(FileItem));
            }
            strncpy(tmp.files[tmp.count].name, SystemShortcuts[shortcut_idx].name, sizeof(tmp.files[tmp.count].name) - 1);
            tmp.files[tmp.count].name[sizeof(tmp.files[tmp.count].name) - 1] = '\0';
            
            strncpy(tmp.files[tmp.count].path, SystemShortcuts[shortcut_idx].path, sizeof(tmp.files[tmp.count].path) - 1);
            tmp.files[tmp.count].path[sizeof(tmp.files[tmp.count].path) - 1] = '\0';
            
            tmp.count++;
        }
    }
#endif
    if (s_apps.files) free(s_apps.files);
    s_apps = tmp;
}

static bool g_app_cache_ready = false;

void ensure_app_cache(void) {
    if (g_app_cache_ready) return;
    g_app_cache_ready = true;
    if (load_app_cache()) {
        printf("[app_launcher] cache loaded entries=%zu\n", s_apps.count);
        return;
    }
    scan_app_dirs();
    save_app_cache();
}

void rescan_app_cache(void) {
    g_app_cache_ready = true;
    scan_app_dirs();
    save_app_cache();
}

void launch_app_by_name(const char* display_name) {
    char path_copy[512] = {0};
    bool found = false;
    for (size_t i = 0; i < s_apps.count; i++) {
        if (fl_string_contains(display_name,s_apps.files[i].name)) {
            strncpy(path_copy, s_apps.files[i].path, sizeof(path_copy)-1);
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
        if (fl_string_contains(s_apps.files[i].name, display_name)) {
            strncpy(path_copy, s_apps.files[i].path, sizeof(path_copy)-1);
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

void applauncher_refresh_callback(const char* filter, ListBoxInfo* lb) {
    ensure_app_cache();
    size_t cnt = s_apps.count;
    
    if (cnt > 2048) {
        const char** names = (const char**)malloc(cnt * sizeof(char*));
        if (!names) return;
        for (size_t i = 0; i < cnt && i < s_apps.count; i++) names[i] = s_apps.files[i].name;
        fl_populate_listbox(lb, filter, names, (int)cnt);
        free(names);
        return;
    }
    const char* names[2048];
    for (size_t i = 0; i < cnt && i < s_apps.count; i++) names[i] = s_apps.files[i].name;
    fl_populate_listbox(lb, filter, names, (int)cnt);
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
        strncpy(buffer, s_apps.files[true_index].path, sizeof(buffer) - 1);
        text = strdup(buffer);
        break;
      }
    }
    fl_copy_to_clipboard(text);
}

void TriggerAppLauncher(void) {
    for (size_t i = 0; i < ARRAY_LEN(ListOfProgramPaths); i++) {
        fl_on_dir_changed(ListOfProgramPaths[i], rescan_app_cache);
    }
    PROMPT(.title = "🚀 AppLauncher",
           .refresh = applauncher_refresh_callback,
           .enter = applauncher_enter_callback,
           .ctrl_enter = applauncher_ctrlenter_callback,
           .copy = applauncher_copy_callback);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+E", TriggerAppLauncher,"Fortnite");
    fl_register_feature("App Launcher", TriggerAppLauncher);
}
