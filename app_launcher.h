#ifndef APP_LAUNCHER_H
#define APP_LAUNCHER_H

#include <windows.h>
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlwapi.h>
#include "config.h"
#include "helpers.h"


#define MAX_APPS 2000

typedef struct {
    char name[128];
    char path[MAX_PATH];
} AppItem;


AppItem G_APPS[MAX_APPS];
int G_APP_COUNT = 0;


static void scan_directory_recursive(const char* dir_path) {
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);

        
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            char first = tolower(find_data.cFileName[0]);
            
            if (first == 'n' || first == '.' || first == 'b' || first == 'o' || 
                first == 'd' || first == 'g' || first == 'y' || first == 'h') {
                
                bool skip = false;
                for (int i = 0; i <  sizeof(ListOfShitToIgnore) / sizeof(ListOfShitToIgnore[0]); i++) {
                    if (StrCmpIA(find_data.cFileName, ListOfShitToIgnore[i]) == 0) {
                        skip = true;
                        break;
                    }
                }
                if (skip) continue;
            }
            scan_directory_recursive(full_path);
        } else {
            char* ext = PathFindExtensionA(find_data.cFileName);
            if (StrCmpIA(ext, ".lnk") == 0 || StrCmpIA(ext, ".exe") == 0 || StrCmpIA(ext, ".url") == 0) {
                if (G_APP_COUNT < MAX_APPS) {
                    char clean_name[128];
                    strncpy(clean_name, find_data.cFileName, sizeof(clean_name) - 1);
                    clean_name[sizeof(clean_name) - 1] = '\0';
                    PathRemoveExtensionA(clean_name);

                    bool is_duplicate = false;
                    for (int i = 0; i < G_APP_COUNT; i++) {
                        if (StrCmpIA(G_APPS[i].name, clean_name) == 0) {
                            is_duplicate = true;
                            break;
                        }
                    }
                    if (is_duplicate) continue;

                    strncpy(G_APPS[G_APP_COUNT].name, clean_name, sizeof(G_APPS[G_APP_COUNT].name) - 1);
                    strncpy(G_APPS[G_APP_COUNT].path, full_path, sizeof(G_APPS[G_APP_COUNT].path) - 1);
                    G_APP_COUNT++;
                }
            }
        }
    } while (FindNextFileA(hFind, &find_data) && G_APP_COUNT < MAX_APPS);

    FindClose(hFind);
}

static void initialize_app_cache() {
    G_APP_COUNT = 0;
    for (int i = 0; i < sizeof(ListOfProgramPaths) / sizeof(ListOfProgramPaths[0]); i++) {
        scan_directory_recursive(ListOfProgramPaths[i]);
    }
}

static bool FindAppInCache(const char* name, char* out_path, size_t path_size) {
    for (int i = 0; i < G_APP_COUNT; i++) {
        if (StrStrIA(G_APPS[i].name, name) != NULL) {
            strncpy(out_path, G_APPS[i].path, path_size);
            return true;
        }
    }
    return false;
}

static char* FindAppPathByName(const char* name) {
    for (int i = 0; i < G_APP_COUNT; i++) {
        if (StrStrIA(G_APPS[i].name, name) != NULL) {
            return G_APPS[i].path;
        }
    }
    return NULL;
}

static void launch_app_by_name(const char* display_name) {
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

static void launch_exe_with_args(const char* exe_path, const char* args) {
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
    if (selection != NULL && strlen(selection) > 0) {
        launch_app_by_name(selection);
    }
}

static void applauncher_ctrlenter_callback(const char* selection) {
    if (selection != NULL && strlen(selection) > 0) {
        launch_app_as_admin(selection);
        selection_complete = true;
    }
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

void TriggerAppLauncher() {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    initialize_app_cache();

    if (G_APP_COUNT == 0) return;

    const char** initial_items = (const char**)malloc(G_APP_COUNT * sizeof(char*));
    if (!initial_items) return;
    
    for (int i = 0; i < G_APP_COUNT; i++) {
        initial_items[i] = G_APPS[i].name;
    }

    PromptUser(initial_items,"🚀 LAUNCHER", G_APP_COUNT, 520, 650,32,
                        applauncher_refresh_callback,
                        applauncher_enter_callback,   
                        applauncher_ctrlenter_callback,
                        NULL,
                        NULL);                         
    free(initial_items);
}

#endif
