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
    
    int current_sel = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
    char current_text[256] = {0};
    if (current_sel != LB_ERR) {
        SendMessageA(hListBox, LB_GETTEXT, current_sel, (LPARAM)current_text);
    }
    
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
    
    int new_count = SendMessageA(hListBox, LB_GETCOUNT, 0, 0);
    
    if (strlen(current_text) > 0) {
        for (int i = 0; i < new_count; i++) {
            char text[256] = {0};
            SendMessageA(hListBox, LB_GETTEXT, i, (LPARAM)text);
            if (strcmp(text, current_text) == 0) {
                SendMessageA(hListBox, LB_SETCURSEL, i, 0);
                return;
            }
        }
    }
    
    if (current_sel != LB_ERR && current_sel < new_count) {
        SendMessageA(hListBox, LB_SETCURSEL, current_sel, 0);
    } else if (new_count > 0) {
        SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
    }
}

static inline bool IsPopupActive(HWND hPopupWnd) {
    return hPopupWnd != NULL && IsWindow(hPopupWnd);
}

static inline bool CheckAndBlockIfPopupActive(HWND hPopupWnd) {
    return IsPopupActive(hPopupWnd);
}

#endif 
