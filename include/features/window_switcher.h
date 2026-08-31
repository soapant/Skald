#ifndef WINDOW_SWITCHER_H
#define WINDOW_SWITCHER_H

typedef struct {
    WindowHandle hwnd;
    char title[256];
    char exe[256];
} WindowInfo;

typedef struct {
    WindowInfo items[100];
    int count;
} WindowList;

void FetchExeNameOnDemand(WindowHandle hwnd, char* outExe, size_t maxLen);
void WindowUtils_GetAll(WindowList* list);
void TriggerWindowSwitcher();

#endif
