#ifndef TYPES_MISC_H
#define TYPES_MISC_H

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif
#include "core/types/window.h"
#include "core/types/prompt.h"

typedef struct {
#if defined(_WIN32)
    LARGE_INTEGER start;
    LARGE_INTEGER frequency;
#else
    struct timespec start;
#endif
} WinTimer;

struct feature_data {
    char* name;
    const char** items;
    const char* last_selections[2048];
    int selection_count;
    int item_count;    
    RefreshCallback refresh_callback;
    EnterCallback enter_callback;
    CtrlEnterCallback ctrl_enter_callback;
    TabCallback tab_callback;
    CopyCallback copy_callback;
    RenderCallback render_callback;
    EditCallback edit_callback;
    FilterCallback filter_callback;
    HelpCallback help_callback;
    bool (*filter_match)(const char*, const char*);
};

typedef struct {
    char* initial_text;
    char* result;
#if defined(_WIN32)
    HFONT hFont;
#else
    void* hFont;
#endif
    int alpha;
    WindowHandle hEdit;
} EditPopupData;


#define MIDDLE_POS 960, 540, 1000, 300
#define TOP_POS 960, 540, 1000, 300
#define BOTTOM_POS 960, 540, 1000, 300
#define LEFT_POS 960, 540, 1000, 300
#define RIGHT_POS 960, 540, 1000, 300

#define MSG_LEFT(msg, msg_width, timeout, color) \
    Platform_DisplayMessage((msg), \
                   (960 - 450 - GAP - ((msg_width) / 2)), \
                   540, \
                   (msg_width), 650, (timeout), (color))

#define MSG_RIGHT(msg, msg_width, timeout, color) \
    Platform_DisplayMessage((msg), \
                   (960 + 450 + GAP + ((msg_width) / 2)), \
                   540, \
                   (msg_width), 650, (timeout), (color))

#define MSG_TOP(msg, msg_height, timeout, color) \
    Platform_DisplayMessage((msg), \
                   960, \
                   (960 - 540  - GAP * 10 - ((msg_height) / 2)), \
                   900, (msg_height), (timeout), (color))

#define MSG_BOTTOM(msg, msg_height, timeout, color) \
    Platform_DisplayMessage((msg), \
                   960, \
                   (540 + (960 / 2) + GAP + ((msg_height) / 2)), \
                   900, (msg_height), (timeout), (color))
#define MSG_MIDDLE_BOTTOM(msg,msg_height,timeout,color) \
    Platform_DisplayMessage((msg), \
                   960, \
                   1080 - msg_height, \
                   900,(msg_height), (timeout), (color))

#if defined(_WIN32)
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK SettingsEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK SimpleEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK SimpleListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
#endif

#endif