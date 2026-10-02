#ifndef TYPES_MISC_H
#define TYPES_MISC_H

#if !defined(_WIN32)
#include <time.h>
#endif
#include "core/types.h"

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
    const char* last_selections[2048];
    int selection_count;
};

typedef struct {
   char* alias;
   char* text;
   char* feature;
} AliasEntry;

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

typedef enum {
    DEFAULT_BROWSER,
    DEFAULT_TERMINAL,
    DEFAULT_SHELL,
    DEFAULT_APP_COUNT
} DefaultAppKind;


#if defined(_WIN32)
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK SettingsEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
#endif

#endif
