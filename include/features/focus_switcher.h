#ifndef FOCUS_SWITCHER_H
#define FOCUS_SWITCHER_H

#include "common.h"
#include "helpers.h"
#include "features/focus_switcher.h"
#include "core/common_ui.h"

#if defined(_WIN32)
#include <psapi.h>
#include <shlwapi.h>
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <string.h>
#include <strings.h>
#endif

static const char* focus_blacklist_games[] = {"CrosshairX"};
static const char* games_windows[] = {"Brawlhalla","Fortnite","RocketLeague","Factorio","Minecraft"};
static const char* focus_blacklist_browsers[] = {"CrosshairX"};
static const char* browsers_windows[] = {"Zen","Chrome"};
static const char* focus_blacklist_coding[] = {"CrosshairX"};
static const char* coding_windows[] = {"Neovide","vim"};
static const char* focus_blacklist_terminal[] = {"CrosshairX"};
static const char* terminal_windows[] = {"alacritty","pwsh","foot"};



static char* my_strcasestr(const char* haystack, const char* needle) {
#if defined(_WIN32)
    return (char*)StrStrIA(haystack, needle);
#else
    return (char*)strcasestr(haystack, needle);
#endif
}

#if !defined(_WIN32)
static bool GetX11WindowPropertyString(Display* dpy, Window w, Atom prop, char* buf, int max_len) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop_data = NULL;

    if (XGetWindowProperty(dpy, w, prop, 0, 1024, False, XA_STRING,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop_data) == Success && prop_data) {
        snprintf(buf, max_len, "%s", (char*)prop_data);
        XFree(prop_data);
        return true;
    }
    
    // Fallback for UTF8 strings (_NET_WM_NAME)
    Atom utf8_string = XInternAtom(dpy, "UTF8_STRING", True);
    if (utf8_string != None) {
        if (XGetWindowProperty(dpy, w, prop, 0, 1024, False, utf8_string,
                               &actual_type, &actual_format, &nitems, &bytes_after, &prop_data) == Success && prop_data) {
            snprintf(buf, max_len, "%s", (char*)prop_data);
            XFree(prop_data);
            return true;
        }
    }
    return false;
}

// Helper to recursively collect all client windows on Linux
static void GetX11ClientWindows(Display* dpy, Window w, Window** list, int* count, int* capacity) {
    Window root, parent;
    Window* children;
    unsigned int nchildren;

    if (XQueryTree(dpy, w, &root, &parent, &children, &nchildren) && children) {
        for (unsigned int i = 0; i < nchildren; i++) {
            char title[256] = {0};
            Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", True);
            if ((net_wm_name != None && GetX11WindowPropertyString(dpy, children[i], net_wm_name, title, sizeof(title))) ||
                GetX11WindowPropertyString(dpy, children[i], XA_WM_NAME, title, sizeof(title))) {
                
                if (*count >= *capacity) {
                    int newCap = *capacity * 2;
                    Window* tmp = realloc(*list, newCap * sizeof(Window));
                    if (!tmp) return;
                    *list = tmp;
                    *capacity = newCap;
                }
                (*list)[(*count)++] = children[i];
            }
            GetX11ClientWindows(dpy, children[i], list, count, capacity);
        }
        XFree(children);
    }
}
#endif

static void FocusWindow(WindowHandle hwnd) {
#if defined(_WIN32)
    HWND win = WH_TO_HWND(hwnd);
    if (win == NULL || !IsWindow(win)) return;
    
    HWND hForegroundWnd = GetForegroundWindow();
    DWORD foregroundThreadID = GetWindowThreadProcessId(hForegroundWnd, NULL);
    DWORD targetThreadID = GetWindowThreadProcessId(win, NULL);
    DWORD currentThreadID = GetCurrentThreadId();

    if (foregroundThreadID != currentThreadID) {
        AttachThreadInput(currentThreadID, foregroundThreadID, TRUE);
    }
    if (targetThreadID != currentThreadID) {
        AttachThreadInput(currentThreadID, targetThreadID, TRUE);
    }

    if (IsIconic(win)) {
        ShowWindow(win, SW_RESTORE);
    } else {
        ShowWindow(win, SW_SHOW);
    }

    SetForegroundWindow(win);
    SetFocus(win);
    SetActiveWindow(win);
    BringWindowToTop(win);

    if (foregroundThreadID != currentThreadID) {
        AttachThreadInput(currentThreadID, foregroundThreadID, FALSE);
    }
    if (targetThreadID != currentThreadID) {
        AttachThreadInput(currentThreadID, targetThreadID, FALSE);
    }

    printf("[FOCUS] Window focused!\n");
#else
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) return;

    // Assuming WH_TO_HWND or a similar macro extracts the raw X11 Window ID, 
    // or you can adjust this extraction based on how your common.h defines WindowHandle.
    Window w = (Window)WH_TO_HWND(hwnd); 
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    XEvent event;
    memset(&event, 0, sizeof(event));
    event.type = ClientMessage;
    event.xclient.window = w;
    event.xclient.message_type = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1; 
    event.xclient.data.l[1] = CurrentTime;
    event.xclient.data.l[2] = 0;

    XSendEvent(dpy, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XMapRaised(dpy, w);
    XFlush(dpy);
    XCloseDisplay(dpy);

    printf("[FOCUS] X11 Window focused!\n");
#endif
}

#if defined(_WIN32)
typedef struct {
    const char* search_title;
    WindowHandle result;
} FindWindowSomething;

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

static WindowHandle FindWindowByPartialTitle(const char* title) {
#if defined(_WIN32)
    FindWindowSomething data;
    data.search_title = title;
    data.result = WH_NULL;
    EnumWindows(FindWindowByPartialTitleProc, (LPARAM)&data);
    return data.result;
#else
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) return WH_NULL;

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    int capacity = 64;
    int count = 0;
    Window* windows = malloc(capacity * sizeof(Window));
    if (!windows) { XCloseDisplay(dpy); return WH_NULL; }
    GetX11ClientWindows(dpy, root, &windows, &count, &capacity);

    WindowHandle found = WH_NULL;
    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", True);

    for (int i = 0; i < count; i++) {
        char window_title[256] = {0};
        if ((net_wm_name == None || !GetX11WindowPropertyString(dpy, windows[i], net_wm_name, window_title, sizeof(window_title)))) {
            GetX11WindowPropertyString(dpy, windows[i], XA_WM_NAME, window_title, sizeof(window_title));
        }

        if (strlen(window_title) > 0 && my_strcasestr(window_title, title) != NULL) {
            found = WH_FROM_HWND(windows[i]);
            break;
        }
    }

    free(windows);
    XCloseDisplay(dpy);
    return found;
#endif
}

static WindowHandle FindAnyWindow(const char* titles[], int count) {
#if defined(_WIN32)
    for (int i = 0; i < count; i++) {
        printf("[FOCUS-DEBUG] Looking for: \"%s\"\n", titles[i]);
        
        WindowHandle hwnd = WH_FROM_HWND(FindWindowA(NULL, titles[i]));
        HWND win = WH_TO_HWND(hwnd);
        if (win != NULL && IsWindow(win) && IsWindowVisible(win)) {
            printf("[FOCUS-DEBUG] FOUND (exact): \"%s\" (HWND: %p)\n", titles[i], (void*)win);
            return hwnd;
        }
        
        hwnd = FindWindowByPartialTitle(titles[i]);
        win = WH_TO_HWND(hwnd);
        if (win != NULL && IsWindow(win) && IsWindowVisible(win)) {
            char window_title[256];
            GetWindowTextA(win, window_title, sizeof(window_title));
            printf("[FOCUS-DEBUG] FOUND (partial): \"%s\" (HWND: %p)\n", window_title, (void*)win);
            return hwnd;
        }
        
        printf("[FOCUS-DEBUG] NOT FOUND: \"%s\"\n", titles[i]);
    }
    printf("[FOCUS-DEBUG] No matching windows found.\n");
    return WH_NULL;
#else
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) return WH_NULL;

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    int cap = 64;
    int win_count = 0;
    Window* windows = malloc(cap * sizeof(Window));
    if (!windows) { XCloseDisplay(dpy); return WH_NULL; }
    GetX11ClientWindows(dpy, root, &windows, &win_count, &cap);

    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", True);
    WindowHandle found = WH_NULL;

    for (int i = 0; i < count; i++) {
        printf("[FOCUS-DEBUG] Looking for: \"%s\"\n", titles[i]);

        for (int j = 0; j < win_count; j++) {
            char window_title[256] = {0};
            if ((net_wm_name == None || !GetX11WindowPropertyString(dpy, windows[j], net_wm_name, window_title, sizeof(window_title)))) {
                GetX11WindowPropertyString(dpy, windows[j], XA_WM_NAME, window_title, sizeof(window_title));
            }

            if (strlen(window_title) > 0 && my_strcasestr(window_title, titles[i]) != NULL) {
                printf("[FOCUS-DEBUG] FOUND: \"%s\" (Window ID: %lu)\n", window_title, windows[j]);
                found = WH_FROM_HWND(windows[j]);
                break;
            }
        }
        // Assuming WH_NULL can be compared or checked via a helper macro if it's a struct, 
        // or check WH_TO_HWND(found) != NULL. Let's use WH_TO_HWND comparison to be safe:
        if (WH_TO_HWND(found) != NULL) break;
    }

    free(windows);
    XCloseDisplay(dpy);
    return found;
#endif
}

static bool IsBlacklisted(const char* title, const char* blacklist[], int count) {
    for (int i = 0; i < count; i++) {
        if (my_strcasestr(title, blacklist[i]) != NULL) {
            return true;
        }
    }
    return false;
}

static inline void focus_switcher_init(void) {
}

#endif
