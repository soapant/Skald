#include "features/api.h"
#include "common.h"
#include "features/focus_switcher.h"
#if defined(_WIN32)
static BOOL CALLBACK EnumMonitorWindowsProc(HWND hwnd, LPARAM lParam) {
    MonitorWindowList* myList = (MonitorWindowList*)lParam;

    if (!IsWindowVisible(hwnd) || GetWindowTextLengthA(hwnd) == 0) {
        return TRUE;
    }

    char className[256];
    GetClassNameA(hwnd, className, sizeof(className));
    if (strcmp(className, "Progman") == 0 || strcmp(className, "WorkerW") == 0 || strcmp(className, "Shell_TrayWnd") == 0) {
        return TRUE; 
    }

    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    if (hMonitor != myList->targetMonitor) {
        return TRUE; 
    }

    if (myList->count >= myList->capacity) {
        return FALSE;
    }

    MonitorWindowData* currentItem = &myList->items[myList->count];
    currentItem->hwnd = WH_FROM_HWND(hwnd);
    GetWindowTextA(hwnd, currentItem->title, sizeof(currentItem->title));
    

    myList->count++;
    return TRUE;
}

static BOOL CALLBACK EnumMonitorsProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM lParam) {
    MonitorSearch *search = (MonitorSearch *)lParam;
    search->currentIndex++;
    
    if (search->currentIndex == search->targetIndex) {
        search->foundMonitor = hMonitor;
        return FALSE;
    }
    return TRUE;
}

static void ScanWindowsOnMonitor(HMONITOR targetMonitor) {
    if (!targetMonitor) {
        return;
    }

    MonitorWindowList myList;
    myList.targetMonitor = targetMonitor;
    myList.count = 0;
    myList.capacity = 64;

    EnumWindows(EnumMonitorWindowsProc, (LPARAM)&myList);

    if (myList.count == 0) {
    } else {
        FocusWindow(myList.items[0].hwnd);
    }
}
#endif

void TriggerMonitorFocusMain() {
#if defined(_WIN32)
    MonitorSearch search = {0};
    search.targetIndex = 1; 
    search.currentIndex = 0;
    search.foundMonitor = NULL;

    EnumDisplayMonitors(NULL, NULL, EnumMonitorsProc, (LPARAM)&search);

    if (search.foundMonitor != NULL) {
        ScanWindowsOnMonitor(search.foundMonitor);
    } else {
    }
#endif
}

void TriggerMonitorFocusSide() {
#if defined(_WIN32)
    MonitorSearch search = {0};
    search.targetIndex = 2; 
    search.currentIndex = 0;
    search.foundMonitor = NULL;

    EnumDisplayMonitors(NULL, NULL, EnumMonitorsProc, (LPARAM)&search);

    if (search.foundMonitor != NULL) {
        ScanWindowsOnMonitor(search.foundMonitor);
    } else {
    }
#endif
}

FLINT_FEATURE_INIT {
    // no auto init for monitor_focus.h
}
