#ifndef FOCUS_SWITCHER_H
#define FOCUS_SWITCHER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <shlwapi.h>
#include "config.h"

// ============================================================
// FIND WINDOW BY EXE NAME
// ============================================================

static void FocusWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;
    
    HWND hForegroundWnd = GetForegroundWindow();
    DWORD foregroundThreadID = GetWindowThreadProcessId(hForegroundWnd, NULL);
    DWORD targetThreadID = GetWindowThreadProcessId(hwnd, NULL);
    DWORD currentThreadID = GetCurrentThreadId();

    if (foregroundThreadID != currentThreadID) {
        AttachThreadInput(currentThreadID, foregroundThreadID, TRUE);
    }
    if (targetThreadID != currentThreadID) {
        AttachThreadInput(currentThreadID, targetThreadID, TRUE);
    }

    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
    }

    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    SetActiveWindow(hwnd);
    BringWindowToTop(hwnd);

    if (foregroundThreadID != currentThreadID) {
        AttachThreadInput(currentThreadID, foregroundThreadID, FALSE);
    }
    if (targetThreadID != currentThreadID) {
        AttachThreadInput(currentThreadID, targetThreadID, FALSE);
    }
    
    printf("[FOCUS] Window focused!\n");
}
 
typedef struct {
    const char* search_exe;
    HWND result;
} FindWindowData;

static BOOL CALLBACK FindWindowByExeProc(HWND hwnd, LPARAM lParam) {
    FindWindowData* data = (FindWindowData*)lParam;
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    
    if (processId != 0 && IsWindowVisible(hwnd)) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (hProcess) {
            char exe_name[MAX_PATH] = {0};
            DWORD size = MAX_PATH;
            if (GetModuleBaseNameA(hProcess, NULL, exe_name, size)) {
                printf("[DEBUG] Found exe: %s, searching for: %s\n", exe_name, data->search_exe);
                if (StrStrIA(exe_name, data->search_exe) != NULL) {
                    data->result = hwnd;
                    CloseHandle(hProcess);
                    return FALSE;
                }
            }
            CloseHandle(hProcess);
        }
    }
    return TRUE;
}

static bool FocusWindowByExe(const char* exe_name) {
    printf("[DEBUG] FocusWindowByExe called with: %s\n", exe_name);
    if (!exe_name || strlen(exe_name) == 0) return false;
    
    FindWindowData data;
    data.search_exe = exe_name;
    data.result = NULL;
    
    EnumWindows(FindWindowByExeProc, (LPARAM)&data);
    
    if (data.result) {
        FocusWindow(data.result);
        return true;
    } else {
        printf("[FOCUS] No window found with exe: %s\n", exe_name);
        return false;
    }
}

typedef struct {
    const char* search_title;
    HWND result;
} FindWindowNigga;

static BOOL CALLBACK FindWindowByPartialTitleProc(HWND hwnd, LPARAM lParam) {
    FindWindowNigga* data = (FindWindowNigga*)lParam;
    char window_title[256];
    GetWindowTextA(hwnd, window_title, sizeof(window_title));
    
    if (IsWindowVisible(hwnd) && StrStrIA(window_title, data->search_title) != NULL) {
        data->result = hwnd;
        return FALSE;
    }
    return TRUE;
}

static HWND FindWindowByPartialTitle(const char* title) {
    FindWindowNigga data;
    data.search_title = title;
    data.result = NULL;
    EnumWindows(FindWindowByPartialTitleProc, (LPARAM)&data);
    return data.result;
}

// ============================================================
// FIND ANY WINDOW (by title list)
// ============================================================
static HWND FindAnyWindow(const char* titles[], int count) {
    for (int i = 0; i < count; i++) {
        printf("[FOCUS-DEBUG] Looking for: \"%s\"\n", titles[i]);
        
        // Try exact match first
        HWND hwnd = FindWindowA(NULL, titles[i]);
        if (hwnd != NULL && IsWindow(hwnd) && IsWindowVisible(hwnd)) {
            printf("[FOCUS-DEBUG] FOUND (exact): \"%s\" (HWND: %p)\n", titles[i], (void*)hwnd);
            return hwnd;
        }
        
        // Try partial match
        hwnd = FindWindowByPartialTitle(titles[i]);
        if (hwnd != NULL && IsWindow(hwnd) && IsWindowVisible(hwnd)) {
            char window_title[256];
            GetWindowTextA(hwnd, window_title, sizeof(window_title));
            printf("[FOCUS-DEBUG] FOUND (partial): \"%s\" (HWND: %p)\n", window_title, (void*)hwnd);
            return hwnd;
        }
        
        printf("[FOCUS-DEBUG] NOT FOUND: \"%s\"\n", titles[i]);
    }
    printf("[FOCUS-DEBUG] No matching windows found.\n");
    return NULL;
}

// ============================================================
// BLACKLIST CHECK
// ============================================================
static bool IsBlacklisted(const char* title, const char* blacklist[], int count) {
    for (int i = 0; i < count; i++) {
        if (StrStrIA(title, blacklist[i]) != NULL) {
            return true;
        }
    }
    return false;
}

// ============================================================
// HANDLE FOCUS SWITCHER HOTKEY
// ============================================================
static bool HandleFocusSwitcher(int vk_code, int modifiers, const char* current_title) {
    for (int i = 0; i < FOCUS_SWITCHER_COUNT; i++) {
        const FocusSwitcherBinding* binding = &G_FOCUS_SWITCHERS[i];
        
        if (binding->vk_code == vk_code && binding->modifiers == modifiers) {
            if (IsBlacklisted(current_title, binding->blacklist, binding->blacklist_count)) {
                printf("[FOCUS] Blacklisted window: %s\n", current_title);
                return false;
            }
            
              if (strcmp(binding->window_titles[2],"Alacritty") == 0) {
                 if (FocusWindowByExe("alacritty.exe")) {
                   return true;
                 }else {
                    HWND target = FindAnyWindow(binding->window_titles, binding->window_count);
                    if (target) {
                        FocusWindow(target);
                    } else {
                        printf("[FOCUS] No matching window found.\n");
                    }
                 }
              }else { 
                  HWND target = FindAnyWindow(binding->window_titles, binding->window_count);
                  if (target) {
                      FocusWindow(target);
                  } else {
                      printf("[FOCUS] No matching window found.\n");
                  }
              }
              
        }
    }
    return false;
}

#endif
