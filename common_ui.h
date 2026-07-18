#ifndef COMMON_UI_H
#define COMMON_UI_H

#include <windows.h>
#include <shlwapi.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

static inline int UI_GetSelection(HWND hList) {
    return (int)SendMessageA(hList, LB_GETCURSEL, 0, 0);
}

static inline void UI_ResetContent(HWND hList) {
    SendMessageA(hList, LB_RESETCONTENT, 0, 0);
}

static inline void UI_AddString(HWND hList, const char* text) {
    if (!hList || !text) return;
    SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)text);
}

static inline void UI_SetSelection(HWND hList, int index) {
    SendMessageA(hList, LB_SETCURSEL, index, 0);
}

static inline int UI_GetCount(HWND hList) {
    return (int)SendMessageA(hList, LB_GETCOUNT, 0, 0);
}

static inline void UI_GetText(HWND hList, int index, char* buffer, int bufferSize) {
    SendMessageA(hList, LB_GETTEXT, index, (LPARAM)buffer);
    buffer[bufferSize - 1] = '\0';
}

static inline void UI_SetHorizontalExtent(HWND hList, int width) {
    SendMessageA(hList, LB_SETHORIZONTALEXTENT, width, 0);
}

#include "modal.h"


static inline void PopulateListBox(HWND hListBox, const char* filter, 
                                   const char** items, int item_count,
                                   bool (*filter_match)(const char* item, const char* filter)) {
    if (!hListBox) return;
    
    // Save current selection
    int current_sel = UI_GetSelection(hListBox);
    char current_text[256] = {0};
    if (current_sel != LB_ERR) {
        UI_GetText(hListBox, current_sel, current_text, sizeof(current_text));
    }
    
    UI_ResetContent(hListBox);
    
    if (!filter || strlen(filter) == 0) {
        for (int i = 0; i < item_count; i++) {
            UI_AddString(hListBox, items[i]);
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
                UI_AddString(hListBox, items[i]);
            }
        }
    }
    
    int new_count = UI_GetCount(hListBox);
    
    if (new_count > 0 && IsNormalMode()) {
        // Try to restore by text
        if (strlen(current_text) > 0) {
            for (int i = 0; i < new_count; i++) {
                char text[256] = {0};
                UI_GetText(hListBox, i, text, sizeof(text));
                if (strcmp(text, current_text) == 0) {
                    UI_SetSelection(hListBox, i);
                    return;
                }
            }
        }
        
        // Restore by index
        if (current_sel != LB_ERR && current_sel < new_count) {
            UI_SetSelection(hListBox, current_sel);
        } else {
            UI_SetSelection(hListBox, 0);
        }
    }
}

static inline bool IsPopupActive(HWND hPopupWnd) {
    return hPopupWnd != NULL && IsWindow(hPopupWnd);
}

static inline bool CheckAndBlockIfPopupActive(HWND hPopupWnd) {
    return IsPopupActive(hPopupWnd);
}


#endif 
