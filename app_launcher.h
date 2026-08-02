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
#include "file_search.h"


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
        printf("TRYING TO LAUNCEH AS ADMING ON %s",selection);
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
static void applauncher_copy_callback(int type) {
    char* text;
    switch(type) {
      case 0: {
        text = G_APPS[g_SelectedIndex].path;
        int start = strlen(text);
        for (int i = start; i > 0 ; i--) {
          if (text[i] == '\\') {
            for (int j = i; j < start; ++j){
                text[j] = '\0';
            }
            break;
          }
        }
      }
      case 1: {
        text = G_APPS[g_SelectedIndex].path;
      }
    }
    CopyToClipboard(text);
}

void TriggerAppLauncher() {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;

    if (G_APP_COUNT == 0) initialize_app_cache();

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

#endif
