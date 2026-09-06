#include "core/flapi.h"
#if !defined(_WIN32)
#include <sys/types.h>
#include "x11_display.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#endif

#define MAX_WINDOWS 256
#define MAX_SWITCHER_WINDOWS 100
#define MAX_EXE_CACHE 128

typedef struct {
    uint32_t processId;
    char exeName[64];
    uint32_t lastAccessTime;
} ExeCacheEntry;

typedef struct {
    WindowHandle hwnd;
    char title[256];
    char exe[256];
} WindowInfo;

typedef struct {
    WindowInfo items[100];
    int count;
} WindowList;

typedef struct {
    WindowList* list;
    const char* pattern;
    const char** blacklist;
    int blacklistCount;
    int mode;
} EnumContext;



static ExeCacheEntry g_exeCache[MAX_EXE_CACHE] = {0};
static int g_exeCacheCount = 0;

static SavedWindow g_switcher_windows[MAX_SWITCHER_WINDOWS];
static int g_switcher_count = 0;


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
    static char exe_name[256];
    memset(exe_name, 0, sizeof(exe_name));

    Display* dpy = get_display();
    if (!dpy) {
        strncpy(exe_name, "unknown", sizeof(exe_name));
        return exe_name;
    }

    Window w = (Window)WH_TO_HWND(hwnd);
    pid_t pid = GetWindowPid(dpy, w);
    GetExeNameFromPid(pid, exe_name, sizeof(exe_name));

    return exe_name;
}
#endif

static void FetchExeNameOnDemand(WindowHandle hwnd, char* outExe, size_t maxLen) {
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

#if !defined(_WIN32)
static bool WindowSwitcher_GetWindowTitle(Display* dpy, Window w, char* buf, int max_len) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop_data = NULL;

    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", True);
    if (net_wm_name != None && 
        XGetWindowProperty(dpy, w, net_wm_name, 0, 1024, False, AnyPropertyType,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop_data) == Success && prop_data) {
        snprintf(buf, max_len, "%s", (char*)prop_data);
        XFree(prop_data);
        if (strlen(buf) > 0) return true;
    }

    if (XGetWindowProperty(dpy, w, XA_WM_NAME, 0, 1024, False, AnyPropertyType,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop_data) == Success && prop_data) {
        snprintf(buf, max_len, "%s", (char*)prop_data);
        XFree(prop_data);
        if (strlen(buf) > 0) return true;
    }

    return false;
}

// X11 Window Collection implementation via _NET_CLIENT_LIST
static void CollectX11Windows(WindowList* list, const char** blacklist, int blacklistCount) {
    Display* dpy = get_display();
    if (!dpy) return;

    Window root = DefaultRootWindow(dpy);
    Atom net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", True);

    Atom actual_type;
    int actual_format;
    unsigned long nitems = 0, bytes_after = 0;
    unsigned char* prop_data = NULL;

    if (net_client_list != None &&
        XGetWindowProperty(dpy, root, net_client_list, 0, 1024, False, XA_WINDOW,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop_data) == Success && prop_data) {
        
        Window* windows = (Window*)prop_data;
        for (unsigned long i = 0; i < nitems && list->count < MAX_WINDOWS; i++) {
            char title[256] = {0};

            if (WindowSwitcher_GetWindowTitle(dpy, windows[i], title, sizeof(title))) {
                bool blacklisted = false;
                for (int b = 0; b < blacklistCount; b++) {
                    if (strcasestr(title, blacklist[b]) != NULL) {
                        blacklisted = true;
                        break;
                    }
                }

                if (!blacklisted) {
                    WindowInfo* info = &list->items[list->count];
                    info->hwnd = WH_FROM_HWND(windows[i]);
                    strcpy(info->title, title);
                    
                    const char* exe = GetExeNameForWindow(WH_FROM_HWND(windows[i]));
                    strcpy(info->exe, exe);
                    
                    list->count++;
                }
            }
        }
        XFree(prop_data);
    }
}
#endif

// ============================================================
// PUBLIC WINDOW FUNCTIONS
// ============================================================

static void WindowUtils_GetAll(WindowList* list) {
    list->count = 0;
#if defined(_WIN32)
    EnumContext ctx = { .list = list, .mode = 0 };
    EnumWindows(EnumWindowsProcEx, (LPARAM)&ctx);
#else
    if (getenv("WAYLAND_DISPLAY") != NULL) {
        // TODO: Implement Wayland/Hyprland IPC client parsing here if needed 
        // (e.g., parsing output of `hyprctl clients -j`)
        printf("[SWITCHER] Native Wayland window listing requires compositor IPC (e.g., hyprctl)\n");
        return;
    }
    
    CollectX11Windows(list, NULL, 0);
#endif
}

static void WindowUtils_GetFiltered(WindowList* list, const char** blacklist, int blacklistCount) {
    list->count = 0;
#if defined(_WIN32)
    EnumContext ctx = { .list = list, .blacklist = blacklist, .blacklistCount = blacklistCount, .mode = 3 };
    EnumWindows(EnumWindowsProcEx, (LPARAM)&ctx);
#else
    if (getenv("WAYLAND_DISPLAY") != NULL) {
        return;
    }
    
    CollectX11Windows(list, blacklist, blacklistCount);
#endif
}

static bool switcher_match_helper(const char* item, const char* filter) {
    for (int i = 0; i < g_switcher_count; i++) {
        if (strcmp(g_switcher_windows[i].title, item) == 0) {
            return (strcasecmp(g_switcher_windows[i].title, filter) != NULL ||
                    strcasecmp(g_switcher_windows[i].exe, filter) != NULL);
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

static bool switcher_enter_callback(const char* selection, ListBoxInfo* lb) {
    (void)lb;
    for (int i = 0; i < g_switcher_count; i++) {
#if defined(_WIN32)
        if (StrStrIA(g_switcher_windows[i].title, selection) != NULL) {
#else
        if (strcasestr(g_switcher_windows[i].title, selection) != NULL) {
#endif
            if (IsWindowValid(g_switcher_windows[i].hwnd)) {
                Platform_FocusWindow(g_switcher_windows[i].title);
            }
            break;
        }
    }
    return true;
}

static void switcher_copy_callback(const char* selection, int true_index, int type) {
    (void)selection;
    (void)type;
    if (true_index >= 0 && true_index < g_switcher_count) {
        fl_copy_into_clipboard(g_switcher_windows[true_index].exe);
    }
}

static void TriggerWindowSwitcher() {
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
    
    PROMPT(.title = "🪟 Switcher", .items = ui_list, .count = g_switcher_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = switcher_refresh_callback,
           .enter = switcher_enter_callback,
           .copy = switcher_copy_callback);
}

FLINT_FEATURE_INIT {
fl_register_hotkey("Alt+Space", TriggerWindowSwitcher);
}
