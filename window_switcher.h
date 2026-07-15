#ifndef WINDOW_SWITCHER_H
#define WINDOW_SWITCHER_H

#include <windows.h>
#include "types.h"
#include "focus_switcher.h"
#include <stdio.h>
#include <string.h>
#include <shlwapi.h>
#include <psapi.h>


// Global state for the switcher
static SavedWindow g_switcher_windows[100];
static int g_switcher_count = 0;

static void FetchExeNameOnDemand(HWND hwnd, char* outExe, DWORD maxLen) {
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != 0) {
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
            }
            CloseHandle(hProcess);
            return;
        }
    }
    strncpy(outExe, "Unknown.exe", maxLen);
}

static void switcher_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
    
    if (!filter || strlen(filter) == 0) {
        // Show all windows
        for (int i = 0; i < g_switcher_count; i++) {
            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)g_switcher_windows[i].title);
        }
    } else {
        // Show only matching windows (title OR exe)
        for (int i = 0; i < g_switcher_count; i++) {
            if (StrStrIA(g_switcher_windows[i].title, filter) != NULL ||
                StrStrIA(g_switcher_windows[i].exe, filter) != NULL) {
                SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)g_switcher_windows[i].title);
            }
        }
    }
    
    if (g_switcher_count > 0) {
        SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
    }
}

// Enter callback - focuses the selected window
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

static char* TriggerWindowSwitcher(HWND hPopupWnd, WindowList myWindows) {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        return NULL;
    }

    char *list_of_not_wanteds[] = {"Ray", "Input", "Manager", "Settings"};
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
        return NULL;
    }

    const char* ui_list[100];
    for (int i = 0; i < g_switcher_count; i++) {
        ui_list[i] = g_switcher_windows[i].title;
    }

    printf("[SWITCHER] Launching UI Selection Prompt...\n");
    char* selected_title = PromptUser(ui_list,"🪟 Switcher",g_switcher_count, 220, 350,32,
                                                switcher_refresh_callback,  // refresh -> filters on typing
                                                switcher_enter_callback,    // enter -> focuses window
                                                NULL,                       // ctrl enter
                                                NULL,
                                                NULL);                      // tab

    if (selected_title == NULL) {
        printf("[SWITCHER] UI Closed. Selection was NULL.\n");
        return NULL;
    }

    printf("[SWITCHER] UI Returned Selection: \"%s\"\n", selected_title);

    // Match by title or exe
    for (int i = 0; i < g_switcher_count; i++) {
        if (StrStrIA(g_switcher_windows[i].title, selected_title) != NULL || 
            StrStrIA(g_switcher_windows[i].exe, selected_title) != NULL) {
            
            HWND target_hwnd = g_switcher_windows[i].hwnd;
            if (IsWindow(target_hwnd)) {
                FocusWindow(target_hwnd);
                printf("[SWITCHER] Switching complete.\n");
            }
            break;
        }
    }

    return selected_title;
}

#endif
