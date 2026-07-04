#ifndef FOCUS_SWITCHER_H
#define FOCUS_SWITCHER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <shlwapi.h>

// Find a window by title (partial match)
static HWND FindWindowByTitle(const char* title) {
    return FindWindowA(NULL, title);
}
typedef struct {
    const char* search_title;
    HWND result;
} FindWindowData;

static BOOL CALLBACK FindWindowByPartialTitleProc(HWND hwnd, LPARAM lParam) {
    FindWindowData* data = (FindWindowData*)lParam;
    char window_title[256];
    GetWindowTextA(hwnd, window_title, sizeof(window_title));
    
    if (IsWindowVisible(hwnd) && StrStrIA(window_title, data->search_title) != NULL) {
        data->result = hwnd;
        return FALSE; // Stop enumerating
    }
    return TRUE;
}

static HWND FindWindowByPartialTitle(const char* title) {
    FindWindowData data;
    data.search_title = title;
    data.result = NULL;
    EnumWindows(FindWindowByPartialTitleProc, (LPARAM)&data);
    return data.result;
}
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

// Focus a window (bring to foreground)
static void FocusWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;
    
    // Get current foreground window info for thread attaching
    HWND hForegroundWnd = GetForegroundWindow();
    DWORD foregroundThreadID = GetWindowThreadProcessId(hForegroundWnd, NULL);
    DWORD targetThreadID = GetWindowThreadProcessId(hwnd, NULL);
    DWORD currentThreadID = GetCurrentThreadId();

    // Attach threads to allow focus change
    if (foregroundThreadID != currentThreadID) {
        AttachThreadInput(currentThreadID, foregroundThreadID, TRUE);
    }
    if (targetThreadID != currentThreadID) {
        AttachThreadInput(currentThreadID, targetThreadID, TRUE);
    }

    // Restore if minimized
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
    }

    // Force focus
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    SetActiveWindow(hwnd);
    BringWindowToTop(hwnd);

    // Detach threads
    if (foregroundThreadID != currentThreadID) {
        AttachThreadInput(currentThreadID, foregroundThreadID, FALSE);
    }
    if (targetThreadID != currentThreadID) {
        AttachThreadInput(currentThreadID, targetThreadID, FALSE);
    }
    
    printf("[FOCUS] Window focused!\n");
}

// Check if a window title matches any in the blacklist
static bool IsBlacklisted(const char* title, const char* blacklist[], int count) {
    for (int i = 0; i < count; i++) {
        if (StrStrIA(title, blacklist[i]) != NULL) {
            return true;
        }
    }
    return false;
}

// Handle focus switcher hotkey
static bool HandleFocusSwitcher(int vk_code, int modifiers, const char* current_title) {
    for (int i = 0; i < FOCUS_SWITCHER_COUNT; i++) {
        const FocusSwitcherBinding* binding = &G_FOCUS_SWITCHERS[i];
        
        // Check if key and modifiers match
        if (binding->vk_code == vk_code && binding->modifiers == modifiers) {
            // Check blacklist
            if (IsBlacklisted(current_title, binding->blacklist, binding->blacklist_count)) {
                printf("[FOCUS] Blacklisted window: %s\n", current_title);
                return false;
            }
            
            // Find and focus any matching window
            HWND target = FindAnyWindow(binding->window_titles, binding->window_count);
            if (target) {
                FocusWindow(target);
                return true;
            } else {
                printf("[FOCUS] No matching window found.\n");
                return false;
            }
        }
    }
    return false;
}

#endif
