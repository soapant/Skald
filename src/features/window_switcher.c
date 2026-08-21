#include "common.h"
#include "features/window_switcher.h"
#include "features/focus_switcher.h"
#include "core/common_ui.h"
#include "helpers.h"
#if defined(_WIN32)
#include <psapi.h>
#include <shlwapi.h>
#endif

// ============================================================
// STATIC DATA
// ============================================================

static ExeCacheEntry g_exeCache[MAX_EXE_CACHE] = {0};
static int g_exeCacheCount = 0;

static SavedWindow g_switcher_windows[MAX_SWITCHER_WINDOWS];
static int g_switcher_count = 0;

// ============================================================
// EXE CACHE
// ============================================================

#if defined(_WIN32)
static const char* GetExeNameForWindow(HWND hwnd) {
    static char result[64];
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    
    if (processId == 0) {
        strcpy(result, "Unknown.exe");
        return result;
    }
    
    for (int i = 0; i < g_exeCacheCount; i++) {
        if (g_exeCache[i].processId == processId) {
            strcpy(result, g_exeCache[i].exeName);
            return result;
        }
    }
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (hProcess) {
        char path[MAX_PATH] = {0};
        if (GetProcessImageFileNameA(hProcess, path, sizeof(path))) {
            char* last = strrchr(path, '\\');
            strcpy(result, last ? last + 1 : path);
        } else {
            strcpy(result, "Unknown.exe");
        }
        CloseHandle(hProcess);
    } else {
        strcpy(result, "Unknown.exe");
    }
    
    if (g_exeCacheCount < MAX_EXE_CACHE) {
        g_exeCache[g_exeCacheCount].processId = processId;
        strcpy(g_exeCache[g_exeCacheCount].exeName, result);
        g_exeCache[g_exeCacheCount].lastAccessTime = GetTickCount();
        g_exeCacheCount++;
    }
    
    return result;
}
#else
static const char* GetExeNameForWindow(WindowHandle hwnd) {
    (void)hwnd;
    static char result[64];
    strcpy(result, "Unknown.exe");
    return result;
}
#endif

void FetchExeNameOnDemand(WindowHandle hwnd, char* outExe, size_t maxLen) {
#if defined(_WIN32)
    const char* name = GetExeNameForWindow(WH_TO_HWND(hwnd));
#else
    const char* name = GetExeNameForWindow(hwnd);
#endif
    strncpy(outExe, name, maxLen - 1);
    outExe[maxLen - 1] = '\0';
}

#if defined(_WIN32)
static BOOL CALLBACK EnumWindowsProcEx(HWND hwnd, LPARAM lParam) {
    EnumContext* ctx = (EnumContext*)lParam;
    WindowList* list = ctx->list;
    
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindowTextLengthA(hwnd) == 0) return TRUE;
    if (list->count >= MAX_WINDOWS) return FALSE;
    
    char title[256];
    GetWindowTextA(hwnd, title, sizeof(title));
    const char* exe = GetExeNameForWindow(hwnd);
    
    // Filter based on mode
    if (ctx->mode == 1) { // By title
        if (strstr(title, ctx->pattern) == NULL) return TRUE;
    } else if (ctx->mode == 2) { // By exe
        if (strstr(exe, ctx->pattern) == NULL) return TRUE;
    } else if (ctx->mode == 3) { // Filtered (blacklist)
        for (int i = 0; i < ctx->blacklistCount; i++) {
            if (strstr(title, ctx->blacklist[i]) != NULL) return TRUE;
        }
    }
    
    WindowInfo* info = &list->items[list->count];
    info->hwnd = WH_FROM_HWND(hwnd);
    strcpy(info->title, title);
    strcpy(info->exe, exe);
    list->count++;
    
    return TRUE;
}
#endif

// ============================================================
// PUBLIC WINDOW FUNCTIONS
// ============================================================

void WindowUtils_GetAll(WindowList* list) {
#if defined(_WIN32)
    list->count = 0;
    EnumContext ctx = { .list = list, .mode = 0 };
    EnumWindows(EnumWindowsProcEx, (LPARAM)&ctx);
#else
    list->count = 0;
#endif
}

static void WindowUtils_GetFiltered(WindowList* list, const char** blacklist, int blacklistCount) {
#if defined(_WIN32)
    list->count = 0;
    EnumContext ctx = { .list = list, .blacklist = blacklist, .blacklistCount = blacklistCount, .mode = 3 };
    EnumWindows(EnumWindowsProcEx, (LPARAM)&ctx);
#else
    (void)blacklist;
    (void)blacklistCount;
    list->count = 0;
#endif
}

// ============================================================
// SWITCHER CALLBACKS
// ============================================================

static bool switcher_match_helper(const char* item, const char* filter) {
    for (int i = 0; i < g_switcher_count; i++) {
        if (strcmp(g_switcher_windows[i].title, item) == 0) {
#if defined(_WIN32)
            return (StrStrIA(g_switcher_windows[i].title, filter) != NULL ||
                    StrStrIA(g_switcher_windows[i].exe, filter) != NULL);
#else
            return (strstr(g_switcher_windows[i].title, filter) != NULL ||
                    strstr(g_switcher_windows[i].exe, filter) != NULL);
#endif
        }
    }
    return false;
}

static void switcher_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    if (!lb) return;
    
    const char* titles[MAX_SWITCHER_WINDOWS];
    for (int i = 0; i < g_switcher_count && i < MAX_SWITCHER_WINDOWS; i++) {
        titles[i] = g_switcher_windows[i].title;
    }
    
    PopulateListBox(lb, hListBox, filter, titles, g_switcher_count, switcher_match_helper);
}

static void switcher_enter_callback(const char* selection, ListBoxInfo* lb) {
    for (int i = 0; i < g_switcher_count; i++) {
#if defined(_WIN32)
        if (StrStrIA(g_switcher_windows[i].title, selection) != NULL) {
#else
        if (strstr(g_switcher_windows[i].title, selection) != NULL) {
#endif
            if (IsWindowValid(g_switcher_windows[i].hwnd)) {
                FocusWindow(g_switcher_windows[i].hwnd);
            }
            break;
        }
    }
}

static void switcher_copy_callback(const char* selection, int true_index, int type) {
    (void)selection;
    if (true_index >= 0 && true_index < g_switcher_count) {
        Platform_CopyToClipboard(g_switcher_windows[true_index].exe);
    }
}

// ============================================================
// TRIGGER
// ============================================================

void TriggerWindowSwitcher() {
    if (IsPopupActive(hPopupWnd)) return;
    
    WindowList windows;
    static const char* blacklist[] = {"Input", "Manager", "Settings"};
    WindowUtils_GetFiltered(&windows, blacklist, 3);
    
    g_switcher_count = 0;
    for (int i = 0; i < windows.count && g_switcher_count < MAX_SWITCHER_WINDOWS; i++) {
        g_switcher_windows[g_switcher_count].hwnd = windows.items[i].hwnd;
        strcpy(g_switcher_windows[g_switcher_count].title, windows.items[i].title);
        strcpy(g_switcher_windows[g_switcher_count].exe, windows.items[i].exe);
        g_switcher_windows[g_switcher_count].index = g_switcher_count;
        g_switcher_count++;
    }
    
    if (g_switcher_count == 0) {
        printf("[SWITCHER] No windows found\n");
        return;
    }
    
    const char* ui_list[MAX_SWITCHER_WINDOWS];
    for (int i = 0; i < g_switcher_count; i++) {
        ui_list[i] = g_switcher_windows[i].title;
    }
    
    Platform_PromptUser(ui_list, "🪟 Switcher", g_switcher_count, 920, 650, 32,
               switcher_refresh_callback,
               switcher_enter_callback,
               NULL, NULL,
               switcher_copy_callback,
               NULL, NULL, NULL, NULL);
}