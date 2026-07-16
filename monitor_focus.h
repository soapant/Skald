#ifndef MONITOR_FOCUS_H
#define MONITOR_FOCUS_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "focus_switcher.h"

// Dedicated structures used only for monitor focus matching
typedef struct {
    HWND hwnd;
    char title[256];
} MonitorWindowData;

typedef struct {
    HMONITOR targetMonitor;
    int count;
    int capacity;
    MonitorWindowData items[64]; // Tracks up to 64 visible windows per screen
} MonitorWindowList;

typedef struct {
    int targetIndex;
    int currentIndex;
    HMONITOR foundMonitor;
} MonitorSearch;

// Dedicated inline window enumeration callback
static inline BOOL CALLBACK EnumMonitorWindowsProc(HWND hwnd, LPARAM lParam) {
    MonitorWindowList* myList = (MonitorWindowList*)lParam;

    // 1. Skip invisible windows or windows with no title text
    if (!IsWindowVisible(hwnd) || GetWindowTextLengthA(hwnd) == 0) {
        return TRUE;
    }

    // 2. Skip common background shells and system managers
    char className[256];
    GetClassNameA(hwnd, className, sizeof(className));
    if (strcmp(className, "Progman") == 0 || strcmp(className, "WorkerW") == 0 || strcmp(className, "Shell_TrayWnd") == 0) {
        return TRUE; 
    }

    // 3. Confirm the window resides on our targeted monitor screen
    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    if (hMonitor != myList->targetMonitor) {
        return TRUE; // Window is on a different screen, keep looking
    }

    // 4. Bound checking for our list capacity
    if (myList->count >= myList->capacity) {
        return FALSE; // Array is completely full, stop enumerating
    }

    // 5. Store the valid window data
    MonitorWindowData* currentItem = &myList->items[myList->count];
    currentItem->hwnd = hwnd;
    GetWindowTextA(hwnd, currentItem->title, sizeof(currentItem->title));
    

    myList->count++;
    return TRUE;
}

// Callback to locate the physical monitor index
static BOOL CALLBACK EnumMonitorsProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM lParam) {
    MonitorSearch *search = (MonitorSearch *)lParam;
    search->currentIndex++;
    
    if (search->currentIndex == search->targetIndex) {
        search->foundMonitor = hMonitor;
        return FALSE; // Found our target side monitor, stop looking
    }
    return TRUE;
}

// Main execution loop using our custom list type
static void ScanWindowsOnMonitor(HMONITOR targetMonitor) {
    if (!targetMonitor) {
        return;
    }

    MonitorWindowList myList;
    myList.targetMonitor = targetMonitor;
    myList.count = 0;
    myList.capacity = 64;

    // Call our dedicated inline window callback
    EnumWindows(EnumMonitorWindowsProc, (LPARAM)&myList);

    if (myList.count == 0) {
    } else {
        // Automatically focus the first real window found on this display
        FocusWindow(myList.items[0].hwnd);
    }
}

// Function 1: Target Main (Primary) Monitor
static inline void TriggerMonitorFocusMain() {
    POINT zeroPoint = {0, 0};
    HMONITOR mainMonitor = MonitorFromPoint(zeroPoint, MONITOR_DEFAULTTOPRIMARY);

    ScanWindowsOnMonitor(mainMonitor);
}

// Function 2: Target Side Monitor (Monitor 2)
static inline void TriggerMonitorFocusSide() {
    MonitorSearch search = {0};
    search.targetIndex = 2; 
    search.currentIndex = 0;
    search.foundMonitor = NULL;

    EnumDisplayMonitors(NULL, NULL, EnumMonitorsProc, (LPARAM)&search);

    if (search.foundMonitor != NULL) {
        ScanWindowsOnMonitor(search.foundMonitor);
    } else {
    }
}

#endif
