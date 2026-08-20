#include "common.h"
#include "features/focus_switcher.h"
#include "core/common_ui.h"
#if defined(_WIN32)
#include <psapi.h>
#include <shlwapi.h>
#endif

void FocusWindow(WindowHandle hwnd) {
#if defined(_WIN32)
    if (WH_TO_HWND(hwnd) == NULL || !IsWindow(WH_TO_HWND(hwnd))) return;
    
    HWND hForegroundWnd = GetForegroundWindow();
    DWORD foregroundThreadID = GetWindowThreadProcessId(hForegroundWnd, NULL);
    DWORD targetThreadID = GetWindowThreadProcessId(WH_TO_HWND(hwnd), NULL);
    DWORD currentThreadID = GetCurrentThreadId();

    if (foregroundThreadID != currentThreadID) {
        AttachThreadInput(currentThreadID, foregroundThreadID, TRUE);
    }
    if (targetThreadID != currentThreadID) {
        AttachThreadInput(currentThreadID, targetThreadID, TRUE);
    }

    if (IsIconic(WH_TO_HWND(hwnd))) {
        ShowWindow(WH_TO_HWND(hwnd), SW_RESTORE);
    } else {
        ShowWindow(WH_TO_HWND(hwnd), SW_SHOW);
    }

    SetForegroundWindow(WH_TO_HWND(hwnd));
    SetFocus(WH_TO_HWND(hwnd));
    SetActiveWindow(WH_TO_HWND(hwnd));
    BringWindowToTop(WH_TO_HWND(hwnd));

    if (foregroundThreadID != currentThreadID) {
        AttachThreadInput(currentThreadID, foregroundThreadID, FALSE);
    }
    if (targetThreadID != currentThreadID) {
        AttachThreadInput(currentThreadID, targetThreadID, FALSE);
    }

    printf("[FOCUS] Window focused!\n");
#else
    (void)hwnd;
#endif
}

#if defined(_WIN32)
static BOOL CALLBACK FindWindowByPartialTitleProc(HWND hwnd, LPARAM lParam) {
    FindWindowSomething* data = (FindWindowSomething*)lParam;
    char window_title[256];
    GetWindowTextA(hwnd, window_title, sizeof(window_title));
    
    if (IsWindowVisible(hwnd) && StrStrIA(window_title, data->search_title) != NULL) {
        data->result = WH_FROM_HWND(hwnd);
        return FALSE;
    }
    return TRUE;
}
#endif

WindowHandle FindWindowByPartialTitle(const char* title) {
#if defined(_WIN32)
    FindWindowSomething data;
    
    data.search_title = title;
    
    data.result = WH_NULL;
    
    EnumWindows(FindWindowByPartialTitleProc, (LPARAM)&data);
    
    return data.result;
#else
    (void)title;
    return WH_NULL;
#endif
}




#if defined(_WIN32)
static WindowHandle FindAnyWindow(const char* titles[], int count) {
    for (int i = 0; i < count; i++) {
        printf("[FOCUS-DEBUG] Looking for: \"%s\"\n", titles[i]);
        
        
        WindowHandle hwnd = WH_FROM_HWND(FindWindowA(NULL, titles[i]));
        if (WH_TO_HWND(hwnd) != NULL && IsWindow(WH_TO_HWND(hwnd)) && IsWindowVisible(WH_TO_HWND(hwnd))) {
            printf("[FOCUS-DEBUG] FOUND (exact): \"%s\" (HWND: %p)\n", titles[i], (void*)WH_TO_HWND(hwnd));
            return hwnd;
        }
        
        
        hwnd = FindWindowByPartialTitle(titles[i]);
        if (WH_TO_HWND(hwnd) != NULL && IsWindow(WH_TO_HWND(hwnd)) && IsWindowVisible(WH_TO_HWND(hwnd))) {
            char window_title[256];
            GetWindowTextA(WH_TO_HWND(hwnd), window_title, sizeof(window_title));
            printf("[FOCUS-DEBUG] FOUND (partial): \"%s\" (HWND: %p)\n", window_title, (void*)WH_TO_HWND(hwnd));
            return hwnd;
        }
        
        printf("[FOCUS-DEBUG] NOT FOUND: \"%s\"\n", titles[i]);
    }
    printf("[FOCUS-DEBUG] No matching windows found.\n");
    return WH_NULL;
}
#else
static WindowHandle FindAnyWindow(const char* titles[], int count) {
    (void)titles;
    (void)count;
    return WH_NULL;
}
#endif




static bool IsBlacklisted(const char* title, const char* blacklist[], int count) {
    for (int i = 0; i < count; i++) {
#if defined(_WIN32)
        if (StrStrIA(title, blacklist[i]) != NULL) {
#else
        if (strstr(title, blacklist[i]) != NULL) {
#endif
            return true;
        }
    }
    return false;
}




bool HandleFocusSwitcher(int vk_code, int modifiers, const char* current_title) {
    for (int i = 0; i < FOCUS_SWITCHER_COUNT; i++) {
        const FocusSwitcherBinding* binding = &G_FOCUS_SWITCHERS[i];
        if (binding->vk_code == vk_code && binding->modifiers == modifiers) {
            if (IsBlacklisted(current_title, binding->blacklist, binding->blacklist_count)) {
                return false;
            }
            else { 
                  WindowHandle target = FindAnyWindow(binding->window_titles, binding->window_count);
                  if (WH_TO_HWND(target) != NULL) {
                      FocusWindow(target);
                      return true;
                  } else {
                      return false;
                  }
            }
              
        }
    }
    return false;
}