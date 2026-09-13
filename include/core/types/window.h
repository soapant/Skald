#ifndef TYPES_WINDOW_H
#define TYPES_WINDOW_H

#include <stdbool.h>
#include <stdint.h>
#if defined(_WIN32)
#include <windows.h>
#endif
#if !defined(_WIN32)
typedef struct _XDisplay Display;
typedef struct _XftFont XftFont;
#endif

typedef struct WindowHandle {
#if defined(_WIN32)
    HWND hwnd;
#else
    unsigned long xid;
#endif
} WindowHandle;

typedef struct FontHandle {
#if defined(_WIN32)
    HFONT hfont;
#else
    XftFont* xfont;
    Display* dpy;
#endif
} FontHandle;

#define WH_NULL ((WindowHandle){0})

#if defined(_WIN32)
#define WH_FROM_HWND(h) ((WindowHandle){ .hwnd = (HWND)(h) })
#define WH_TO_HWND(h) ((h).hwnd)
#else
#define WH_FROM_HWND(h) ((void)(h), WH_NULL)
#define WH_TO_HWND(h) ((h).xid)
#endif

#define FH_NULL ((FontHandle){0})

#if defined(_WIN32)
#define FH_FROM_HFONT(h) ((FontHandle){ .hfont = (HFONT)(h) })
#define FH_TO_HFONT(h) ((h).hfont)
#else
#define FH_TO_XFT(h) ((h).xfont)
#define FH_TO_DPY(h) ((h).dpy)
#endif

typedef struct {
    int x, y, w, h;
    int item_height;
} WindowLayout;

typedef struct {
    int index;
    WindowHandle hwnd;
    char title[256];
    char exe[256];
} SavedWindow;

typedef struct {
    WindowHandle hwnd;
    char title[256];
    char exe[256];
} WindowData;

typedef struct {
    WindowHandle hwnd;
    char title[256];
} MonitorWindowData;

typedef struct {
    char** windows;
    int count;
} WindowList;

typedef struct {
#if defined(_WIN32)
    HMONITOR targetMonitor;
#else
    unsigned long targetMonitor;
#endif
    int count;
    int capacity;
    MonitorWindowData items[64];
} MonitorWindowList;

typedef struct {
    int targetIndex;
    int currentIndex;
#if defined(_WIN32)
    HMONITOR foundMonitor;
#else
    unsigned long foundMonitor;
#endif
} MonitorSearch;

#if defined(_WIN32)
#define MAIN_CENTER_X (GetSystemMetrics(SM_CXSCREEN) / 2)
#define MAIN_CENTER_Y (GetSystemMetrics(SM_CYSCREEN) / 2)
#endif

#define GAP 20
#define ID_TRAY_ICON 1001
#if defined(_WIN32)
#define WM_TRAYICON (WM_USER + 1)
#define WM_TRIGGER_IMG_CAPTURE (WM_USER + 2)
#define WM_TRIGGER_UNIVERSAL_MACRO (WM_USER + 3)
#endif
#define ID_EXIT 2001
#define ID_SETTINGS 2002
#define IDC_INPUT_BOX 101
#define IDC_LIST_BOX 102

#endif
