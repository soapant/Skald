#ifndef APP_LAUNCHER_H
#define APP_LAUNCHER_H

#include "config.h"
#include "focus_switcher.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlwapi.h>

#define MAX_APPS 2000

typedef struct {
    char name[128];
    char path[MAX_PATH];
} AppItem;

static AppItem G_APPS[MAX_APPS];
static int G_APP_COUNT = 0;

// Recursively look through folders for shortcuts and executables
static void scan_directory_recursive(const char* dir_path) {
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        // Skip current and parent directories
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // --- THE INSTANT FIX: Skip compilation and dependency blackholes ---
            if (StrCmpIA(find_data.cFileName, "node_modules") == 0 ||
                StrCmpIA(find_data.cFileName, ".git") == 0 ||
                StrCmpIA(find_data.cFileName, "bin") == 0 ||
                StrCmpIA(find_data.cFileName, "obj") == 0 ||
                StrCmpIA(find_data.cFileName, "build") == 0) {
                continue;
            }

            scan_directory_recursive(full_path);
        } else {
            char* ext = PathFindExtensionA(find_data.cFileName);
            if (StrCmpIA(ext, ".lnk") == 0 || StrCmpIA(ext, ".exe") == 0 || StrCmpIA(ext, ".url") == 0) {
                if (G_APP_COUNT < MAX_APPS) {
                    // Store the clean display name without extension
                    char clean_name[128];
                    strncpy(clean_name, find_data.cFileName, sizeof(clean_name) - 1);
                    PathRemoveExtensionA(clean_name);

                    strncpy(G_APPS[G_APP_COUNT].name, clean_name, sizeof(G_APPS[G_APP_COUNT].name) - 1);
                    strncpy(G_APPS[G_APP_COUNT].path, full_path, sizeof(G_APPS[G_APP_COUNT].path) - 1);
                    G_APP_COUNT++;
                }
            }
        }
    } while (FindNextFileA(hFind, &find_data) && G_APP_COUNT < MAX_APPS);

    FindClose(hFind);
}

// Populate the collection
static void initialize_app_cache() {
    G_APP_COUNT = 0;
    // Scan your custom Start Menu path
    scan_directory_recursive("C:\\Users\\HP\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu");
    
    scan_directory_recursive("C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs");
    
    scan_directory_recursive("D:\\Projects");
}

static void launch_app_by_name(const char* display_name) {
    printf("[LAUNCHER-EXEC] Attempting execution for query: \"%s\"\n", display_name);
    printf("[LAUNCHER-EXEC] Checking against %d cached applications...\n", G_APP_COUNT);
    
    for (int i = 0; i < G_APP_COUNT; i++) {
        // Case-insensitive match check
        if (StrCmpIA(G_APPS[i].name, display_name) == 0) {
            printf("[LAUNCHER-EXEC] MATCH FOUND! Index: %d\n", i);
            printf("[LAUNCHER-EXEC] Display Name: \"%s\"\n", G_APPS[i].name);
            printf("[LAUNCHER-EXEC] File Target Path: \"%s\"\n", G_APPS[i].path);

            SHELLEXECUTEINFOA sei = {0};
            sei.cbSize = sizeof(SHELLEXECUTEINFOA);
            sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_ASYNCOK; 
            sei.lpVerb = "open";               
            sei.lpFile = G_APPS[i].path;       
            sei.nShow = SW_SHOWNORMAL;

            printf("[LAUNCHER-EXEC] Calling Windows Shell API...\n");
            if (ShellExecuteExA(&sei)) {
                printf("[LAUNCHER-EXEC] SUCCESS: Windows accepted execution target handle!\n");
            } else {
                printf("[LAUNCHER-EXEC] CRITICAL WINDOWS ERROR: ShellExecuteExA failed. Error Code: %lu\n", GetLastError());
            }
            return;
        }
    }
    
    // If it reaches here, the name isn't inside G_APPS at all. Let's dump the cache to see why!
    printf("[LAUNCHER-EXEC] CRITICAL ERROR: No cache entry matched \"%s\". dumping first 10 cache items:\n", display_name);
    int dump_limit = G_APP_COUNT < 10 ? G_APP_COUNT : 10;
    for (int i = 0; i < dump_limit; i++) {
        printf("  [%d] Cached Name: \"%s\" -> Path: \"%s\"\n", i, G_APPS[i].name, G_APPS[i].path);
    }
}

static void launch_app_as_admin(const char* display_name) {
    for (int i = 0; i < G_APP_COUNT; i++) {
        if (StrCmpIA(G_APPS[i].name, display_name) == 0) {
            SHELLEXECUTEINFOA sei = {0};
            sei.cbSize = sizeof(SHELLEXECUTEINFOA);
            sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_ASYNCOK;
            sei.lpVerb = "runas";  // This triggers administrator elevation
            sei.lpFile = G_APPS[i].path;
            sei.nShow = SW_SHOWNORMAL;

            if (ShellExecuteExA(&sei)) {
                printf("[LAUNCHER-ADMIN] Successfully launched as admin: %s\n", display_name);
            } else {
                printf("[LAUNCHER-ADMIN] Failed to launch as admin: %s (Error: %lu)\n", display_name, GetLastError());
            }
            return;
        }
    }
}
static void launch_exe_with_args(const char* exe_path, const char* args) {
    char cmd[MAX_PATH * 2 + 512];
    
    if (args != NULL && strlen(args) > 0) {
        snprintf(cmd, sizeof(cmd), "\"%s\" %s", exe_path, args);
    } else {
        snprintf(cmd, sizeof(cmd), "\"%s\"", exe_path);
    }
    
    printf("[LAUNCHER-EXEC] Running: %s\n", cmd);
    
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        printf("[LAUNCHER-EXEC] Process started successfully!\n");
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
    } else {
        printf("[LAUNCHER-EXEC] Failed to start process. Error: %lu\n", GetLastError());
    }
}

// Launch an executable by name (from cache) with arguments
static void launch_app_by_name_with_args(const char* display_name, const char* args) {
    printf("[LAUNCHER-EXEC] Attempting execution for query: \"%s\" with args: \"%s\"\n", display_name, args ? args : "");
    printf("[LAUNCHER-EXEC] Checking against %d cached applications...\n", G_APP_COUNT);
    
    for (int i = 0; i < G_APP_COUNT; i++) {
        if (StrCmpIA(G_APPS[i].name, display_name) == 0) {
            printf("[LAUNCHER-EXEC] MATCH FOUND! Index: %d\n", i);
            printf("[LAUNCHER-EXEC] Display Name: \"%s\"\n", G_APPS[i].name);
            printf("[LAUNCHER-EXEC] File Target Path: \"%s\"\n", G_APPS[i].path);
            
            launch_exe_with_args(G_APPS[i].path, args);
            return;
        }
    }
    
    printf("[LAUNCHER-EXEC] ERROR: No cache entry matched \"%s\"\n", display_name);
}

// Launch an executable by full path with arguments
static void launch_exe_by_path(const char* exe_path, const char* args) {
    if (!PathFileExistsA(exe_path)) {
        printf("[LAUNCHER-EXEC] ERROR: File not found: %s\n", exe_path);
        return;
    }
    launch_exe_with_args(exe_path, args);
}

#endif
