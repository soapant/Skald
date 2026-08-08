#ifndef WINDOW_SWITCHER_H
#define WINDOW_SWITCHER_H

#include <windows.h>

#define MAX_WINDOWS 256
#define MAX_SWITCHER_WINDOWS 100
#define MAX_EXE_CACHE 128

// ============================================================
// WINDOW DATA STRUCTURES
// ============================================================

typedef struct {
    HWND hwnd;
    char title[256];
    char exe[256];
} WindowInfo;

typedef struct {
    WindowInfo items[MAX_WINDOWS];
    int count;
} WindowList;

typedef struct {
    DWORD processId;
    char exeName[64];
    DWORD lastAccessTime;
} ExeCacheEntry;

typedef struct {
    WindowList* list;
    const char* pattern;
    const char** blacklist;
    int blacklistCount;
    int mode;
} EnumContext;

void FetchExeNameOnDemand(HWND hwnd, char* outExe, DWORD maxLen);
void WindowUtils_GetAll(WindowList* list);
void TriggerWindowSwitcher();

#endif
