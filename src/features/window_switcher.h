#ifndef WINDOW_SWITCHER_H
#define WINDOW_SWITCHER_H

#define MAX_WINDOWS 256
#define MAX_SWITCHER_WINDOWS 100
#define MAX_EXE_CACHE 128

typedef struct {
    WindowHandle hwnd;
    char title[256];
    char exe[256];
} WindowInfo;

typedef struct {
    WindowInfo items[MAX_WINDOWS];
    int count;
} WindowList;

typedef struct {
    uint32_t processId;
    char exeName[64];
    uint32_t lastAccessTime;
} ExeCacheEntry;

typedef struct {
    WindowList* list;
    const char* pattern;
    const char** blacklist;
    int blacklistCount;
    int mode;
} EnumContext;

void FetchExeNameOnDemand(WindowHandle hwnd, char* outExe, size_t maxLen);
void WindowUtils_GetAll(WindowList* list);
void TriggerWindowSwitcher();

#endif
