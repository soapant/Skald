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

// Check if a window with any of the given titles exists
static HWND FindAnyWindow(const char* titles[], int count) {
    for (int i = 0; i < count; i++) {
        HWND hwnd = FindWindowByTitle(titles[i]);
        if (hwnd != NULL && IsWindow(hwnd) && IsWindowVisible(hwnd)) {
            printf("[FOCUS] Found window: %s\n", titles[i]);
            return hwnd;
        }
    }
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
