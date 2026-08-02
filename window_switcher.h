#ifndef WINDOW_SWITCHER_H
#define WINDOW_SWITCHER_H

#include <windows.h>
#include "helpers.h"
#include "types.h"
#include "focus_switcher.h"
#include <stdio.h>
#include <string.h>
#include <shlwapi.h>
#include <psapi.h>
#include "common_ui.h"

static SavedWindow g_switcher_windows[100];
static int g_switcher_count = 0;

typedef struct {
    DWORD processId;
    char exeName[64];
    DWORD lastAccessTime;
} ExeCacheEntry;

static ExeCacheEntry g_exeCache[128] = {0};
static int g_exeCacheCount = 0;
static DWORD g_lastCacheCleanup = 0;

static void FetchExeNameOnDemand(HWND hwnd, char* outExe, DWORD maxLen) {
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    
    if (processId == 0) {
        strncpy(outExe, "Unknown.exe", maxLen);
        return;
    }
    
    DWORD currentTime = GetTickCount();
    
    if (currentTime - g_lastCacheCleanup > 10000) {
        g_lastCacheCleanup = currentTime;
        int keepCount = 0;
        for (int i = 0; i < g_exeCacheCount; i++) {
            if (currentTime - g_exeCache[i].lastAccessTime < 5000) {
                if (keepCount != i) {
                    memcpy(&g_exeCache[keepCount], &g_exeCache[i], sizeof(ExeCacheEntry));
                }
                keepCount++;
            }
        }
        g_exeCacheCount = keepCount;
    }
    
    for (int i = 0; i < g_exeCacheCount; i++) {
        if (g_exeCache[i].processId == processId) {
            g_exeCache[i].lastAccessTime = currentTime;
            strncpy(outExe, g_exeCache[i].exeName, maxLen);
            return;
        }
    }
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (hProcess) {
        char path[MAX_PATH] = {0};
        if (GetProcessImageFileNameA(hProcess, path, sizeof(path))) {
            char* lastBackslash = strrchr(path, '\\');
            if (lastBackslash) {
                strncpy(outExe, lastBackslash + 1, maxLen);
            } else {
                strncpy(outExe, path, maxLen);
            }
        } else {
            strncpy(outExe, "Unknown.exe", maxLen);
        }
        CloseHandle(hProcess);
    } else {
        strncpy(outExe, "Unknown.exe", maxLen);
    }
    
    if (g_exeCacheCount < 128) {
        g_exeCache[g_exeCacheCount].processId = processId;
        strncpy(g_exeCache[g_exeCacheCount].exeName, outExe, sizeof(g_exeCache[0].exeName) - 1);
        g_exeCache[g_exeCacheCount].lastAccessTime = currentTime;
        g_exeCacheCount++;
    }
}

static bool switcher_match_helper(const char* item, const char* filter) {
    for (int i = 0; i < g_switcher_count; i++) {
        if (g_switcher_windows[i].title == item) {
            return (StrStrIA(g_switcher_windows[i].title, filter) != NULL ||
                    StrStrIA(g_switcher_windows[i].exe, filter) != NULL);
        }
    }
    return false;
}

static void switcher_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    const char* titles[100];
    int count = (g_switcher_count < 100) ? g_switcher_count : 100;
    
    for (int i = 0; i < count; i++) {
        titles[i] = g_switcher_windows[i].title;
    }
    
    PopulateListBox(hListBox, filter, titles, count, switcher_match_helper);
}

static void switcher_enter_callback(const char* selection) {
    if (selection == NULL || strlen(selection) == 0) return;
    
    for (int i = 0; i < g_switcher_count; i++) {
        if (StrStrIA(g_switcher_windows[i].title, selection) != NULL) {
            HWND target_hwnd = g_switcher_windows[i].hwnd;
            if (IsWindow(target_hwnd)) {
                FocusWindow(target_hwnd);
                printf("[SWITCHER] Focused: %s\n", g_switcher_windows[i].title);
            }
            break;
        }
    }
}
static void switcher_copy_callback(int type) {
    CopyToClipboard(g_switcher_windows[g_SelectedIndex].exe);
}

static void TriggerWindowSwitcher() {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        return;
    }
    WindowList myWindows;
    myWindows.count = 0;
    myWindows.capacity = 10;
    myWindows.items = (WindowData*)malloc(myWindows.capacity * sizeof(WindowData));
    if (myWindows.items != NULL) {
        EnumWindows(EnumWindowsProc, (LPARAM)&myWindows);
    }

    char *list_of_not_wanteds[] = {"Input", "Manager", "Settings"};
    size_t size_of_list = sizeof(list_of_not_wanteds) / sizeof(list_of_not_wanteds[0]);

    g_switcher_count = 0;

    for (int i = 0; i < myWindows.count; i++) {
        HWND targetHwnd = myWindows.items[i].hwnd;
        char* targetTitle = myWindows.items[i].title;
        bool not_wanted = false;
        
        for (size_t j = 0; j < size_of_list; j++) {
            if (strstr(targetTitle, list_of_not_wanteds[j]) != NULL) {
                not_wanted = true;
                break; 
            }
        }
        if (not_wanted) continue; 

        g_switcher_windows[g_switcher_count].index = g_switcher_count;
        g_switcher_windows[g_switcher_count].hwnd = targetHwnd;
        strncpy(g_switcher_windows[g_switcher_count].title, targetTitle, sizeof(g_switcher_windows[g_switcher_count].title) - 1);
        g_switcher_windows[g_switcher_count].title[sizeof(g_switcher_windows[g_switcher_count].title) - 1] = '\0'; 

        FetchExeNameOnDemand(targetHwnd, g_switcher_windows[g_switcher_count].exe, sizeof(g_switcher_windows[g_switcher_count].exe) - 1);
        g_switcher_count++;
        if (g_switcher_count >= 100) break;
    }

    printf("[SWITCHER] Filter complete. Kept %d windows out of %d.\n", g_switcher_count, myWindows.count);

    if (g_switcher_count == 0) {
        printf("[SWITCHER] No windows passed the filter.\n");
        return;
    }

    const char* ui_list[100];
    for (int i = 0; i < g_switcher_count; i++) {
        ui_list[i] = g_switcher_windows[i].title;
    }

    printf("[SWITCHER] Launching UI Selection Prompt...\n");
    char* selected_title = PromptUser(ui_list,"🪟 Switcher",g_switcher_count, 920, 650,32,
                                                switcher_refresh_callback,  
                                                switcher_enter_callback,    
                                                NULL,NULL,switcher_copy_callback,NULL,NULL,NULL,NULL);

    free(myWindows.items);
}

#endif
