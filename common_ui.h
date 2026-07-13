// common_ui.h
#ifndef COMMON_UI_H
#define COMMON_UI_H

#include <windows.h>
#include <shlwapi.h>
#include <stdbool.h>
#include <string.h>

static inline void PopulateListBox(HWND hListBox, const char* filter, 
                                   const char** items, int item_count,
                                   bool (*filter_match)(const char* item, const char* filter)) {
    if (!hListBox) return;
    SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
    
    if (!filter || strlen(filter) == 0) {
        for (int i = 0; i < item_count; i++) {
            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)items[i]);
        }
    } else {
        for (int i = 0; i < item_count; i++) {
            bool match = false;
            if (filter_match) {
                match = filter_match(items[i], filter);
            } else {
                match = (StrStrIA(items[i], filter) != NULL);
            }
            if (match) {
                SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)items[i]);
            }
        }
    }
    SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
}

static inline bool IsPopupActive(HWND hPopupWnd) {
    return hPopupWnd != NULL && IsWindow(hPopupWnd);
}

static inline bool CheckAndBlockIfPopupActive(HWND hPopupWnd) {
    return IsPopupActive(hPopupWnd);
}

#endif // COMMON_UI_H
