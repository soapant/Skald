#include "common.h"
#include "features/app_launcher.h"
#include "features/file_search.h"
#include "helpers.h"
#include "core/common_ui.h"

void initialize_app_cache(void) {
    G_APP_COUNT = 0;
    
    for (int i = 0; i < sizeof(ListOfProgramPaths) / sizeof(ListOfProgramPaths[0]); i++) {
        scan_directory_recursive(ListOfProgramPaths[i]);
    }
    
    for (int shortcut_idx = 0; shortcut_idx < SYSTEM_SHORTCUT_COUNT; shortcut_idx++) {
        if (G_APP_COUNT >= MAX_APPS) break;
        
        bool found = false;
        for (int app_idx = 0; app_idx < G_APP_COUNT; app_idx++) {
            if (StrStrIA(G_APPS[app_idx].path, SystemShortcuts[shortcut_idx].path) != NULL) {
                strncpy(G_APPS[app_idx].name, SystemShortcuts[shortcut_idx].name, sizeof(G_APPS[app_idx].name) - 1);
                G_APPS[app_idx].name[sizeof(G_APPS[app_idx].name) - 1] = '\0';
                found = true;
                break;
            }
        }
        
        if (!found) {
            strncpy(G_APPS[G_APP_COUNT].name, SystemShortcuts[shortcut_idx].name, sizeof(G_APPS[G_APP_COUNT].name) - 1);
            G_APPS[G_APP_COUNT].name[sizeof(G_APPS[G_APP_COUNT].name) - 1] = '\0';
            
            strncpy(G_APPS[G_APP_COUNT].path, SystemShortcuts[shortcut_idx].path, sizeof(G_APPS[G_APP_COUNT].path) - 1);
            G_APPS[G_APP_COUNT].path[sizeof(G_APPS[G_APP_COUNT].path) - 1] = '\0';
            
            G_APP_COUNT++;
        }
    }
}
void launch_app_by_name(const char* display_name) {
    for (int i = 0; i < G_APP_COUNT; i++) {
        if (StrCmpIA(G_APPS[i].name, display_name) == 0) {
            LaunchApp(G_APPS[i].path);
            return;
        }
    }
}

static void launch_app_as_admin(const char* display_name) {
    for (int i = 0; i < G_APP_COUNT; i++) {
        if (StrCmpIA(G_APPS[i].name, display_name) == 0) {
            LaunchAppAsAdmin(G_APPS[i].path);
            return;
        }
    }
}

void launch_exe_with_args(const char* exe_path, const char* args) {
    char cmd[MAX_PATH * 2 + 512];
    
    if (args != NULL && strlen(args) > 0 && strcmp(args, "0") != 0) {
        snprintf(cmd, sizeof(cmd), "\"%s\" %s", exe_path, args);
    } else {
        snprintf(cmd, sizeof(cmd), "\"%s\"", exe_path);
    }
    
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

static void applauncher_enter_callback(const char* selection) {
    launch_app_by_name(selection);
}

static void applauncher_ctrlenter_callback(const char* selection) {
    launch_app_as_admin(selection);
    selection_complete = true;
}

static void applauncher_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;

    const char* names[MAX_APPS];
    int count = (G_APP_COUNT < MAX_APPS) ? G_APP_COUNT : MAX_APPS;
    
    for (int i = 0; i < count; i++) {
        names[i] = G_APPS[i].name;
    }
    PopulateListBox(hListBox, filter, names, count, NULL);
}
static void applauncher_copy_callback(const char* selection,int true_index,int type) {
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
        if (true_index >= 0 && true_index < G_APP_COUNT) {
            text = G_APPS[true_index].path;
        }
        break;
      }
    }
    CopyToClipboard(text);
}

void TriggerAppLauncher(void) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;

    const char** initial_items = (const char**)malloc(G_APP_COUNT * sizeof(char*));
    if (!initial_items) return;
    
    for (int i = 0; i < G_APP_COUNT; i++) {
        initial_items[i] = G_APPS[i].name;
    }

    PromptUser(initial_items,"🚀 Launcher", G_APP_COUNT, 920, 650,32,
                        applauncher_refresh_callback,
                        applauncher_enter_callback,   
                        applauncher_ctrlenter_callback,
                        NULL,
                        applauncher_copy_callback,
                        NULL,
                        NULL,
                        NULL,NULL);                         
    free(initial_items);
}
