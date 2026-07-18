#ifndef COMMON_UI_H
#define COMMON_UI_H

#include <sec_api/stdio_s.h>
#include <windows.h>
#include <shlwapi.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "types.h"



typedef struct {
    LARGE_INTEGER start;
    LARGE_INTEGER frequency;
} WinTimer;

static inline WinTimer TimerStart() {
    WinTimer t;
    QueryPerformanceFrequency(&t.frequency);
    QueryPerformanceCounter(&t.start);
    return t;
}

static inline void TimerEnd(WinTimer t, const char* operation_name) {
    LARGE_INTEGER end;
    QueryPerformanceCounter(&end);

    double elapsed_us = (double)(end.QuadPart - t.start.QuadPart) * 1000000.0 / (double)t.frequency.QuadPart;

    if (elapsed_us < 1000.0) {
        printf("[ TIMER] %-25s : %8.2f µs\n", operation_name, elapsed_us);
    } else if (elapsed_us < 1000000.0) {
        printf("[ TIMER] %-25s : %8.2f ms\n", operation_name, elapsed_us / 1000.0);
    } else {
        printf("[ TIMER] %-25s : %8.2f s\n", operation_name, elapsed_us / 1000000.0);
    }
}

static inline int UI_GetSelection(HWND hList) {
    int sel = g_SelectedIndex;
    printf("[DEBUG-UI] UI_GetSelection called. g_SelectedIndex is currently: %d\n", sel);
    return sel;
}


static inline void UI_ResetContent(HWND hList) {
    g_populated_count = 0;
    g_SelectedIndex = -1;
    SendMessageA(hList, LB_SETCOUNT, 0, 0);
}

static inline void UI_AddString(HWND hList, const char* text) {
    if (!hList || !text || g_populated_count >= 100) return;
    
    strncpy(g_populated_cache[g_populated_count], text, 511);
    g_populated_cache[g_populated_count][511] = '\0';
    
    g_populated_count++;
    
    SendMessageA(hList, LB_SETCOUNT, g_populated_count, 0);
}


static inline void UI_SetSelection(HWND hList, int index) {
    g_SelectedIndex = index;
    InvalidateRect(hList, NULL, TRUE);
}

static inline int UI_GetCount(HWND hList) {
    return g_populated_count;
}

static inline void UI_GetText(HWND hList, int index, char* buffer, int bufferSize) {
    if (!buffer || bufferSize <= 0) return;
    
    printf("[DEBUG-UI] UI_GetText requesting index: %d (g_populated_count is %d)\n", index, g_populated_count);
    
    if (index >= 0 && index < g_populated_count) {
        // Log the raw string sitting inside the global cache slot before copying it
        printf("[DEBUG-UI] Cache slot [%d] contains string: \"%s\"\n", index, g_populated_cache[index]);
        
        strncpy(buffer, g_populated_cache[index], bufferSize - 1);
    } else {
        printf("[DEBUG-UI] Index %d out of bounds! Clearing buffer.\n", index);
        buffer[0] = '\0';
    }
    buffer[bufferSize - 1] = '\0';
}

static inline void UI_SetHorizontalExtent(HWND hList, int width) {
    SendMessageA(hList, LB_SETHORIZONTALEXTENT, width, 0);
}

static inline void UI_SetItemCount(HWND hList, int count) {
    g_populated_count = count;
    SendMessageA(hList, LB_SETCOUNT, count, 0);
}

static inline void UI_Redraw(HWND hList) {
    InvalidateRect(hList, NULL, TRUE);
}

#define MAX_UI_ITEMS 10000

static inline void UI_UpdateList(HWND hList, const char* filter, const char** items, int item_count) {
    static const char* filtered[MAX_UI_ITEMS];
    int filtered_count = 0;

    if (!filter || !*filter) {
        for (int i = 0; i < item_count && filtered_count < MAX_UI_ITEMS; i++) {
            filtered[filtered_count++] = items[i];
        }
    } else {
        for (int i = 0; i < item_count && filtered_count < MAX_UI_ITEMS; i++) {
            if (StrStrIA(items[i], filter)) {
                filtered[filtered_count++] = items[i];
            }
        }
    }

    g_initial_items = filtered;
    g_initial_count = filtered_count;

    UI_SetItemCount(hList, filtered_count);
    UI_Redraw(hList);
}

#include "modal.h"


static inline void PopulateListBox(HWND hListBox, const char* filter, 
                                   const char** items, int item_count,
                                   bool (*filter_match)(const char* item, const char* filter)) {
  
    if (!hListBox) return;
    
    int current_sel = g_SelectedIndex; 
    char current_text[256] = {0};
    
    if (current_sel >= 0 && current_sel < g_populated_count) {
        strncpy(current_text, g_populated_cache[current_sel], sizeof(current_text) - 1);
    }

    UI_ResetContent(hListBox);
    g_populated_count = 0;

    for (int i = 0; i < item_count && g_populated_count < MAX_ITEMS; i++) {
        if (!items[i]) continue;

        bool match = false;
        if (!filter || strlen(filter) == 0) {
            match = true;
        } else if (filter_match) {
            match = filter_match(items[i], filter);
        } else {
            match = (StrStrIA(items[i], filter) != NULL);
        }

        if (match) {
            strncpy(g_populated_cache[g_populated_count], items[i], 511);
            g_populated_cache[g_populated_count][511] = '\0';
            
            g_populated_cache[g_populated_count][strcspn(g_populated_cache[g_populated_count], "\r\n")] = '\0';
            
            UI_AddString(hListBox, g_populated_cache[g_populated_count]);
        }
    }

    if (g_populated_count > 0 && IsNormalMode()) {
        if (strlen(current_text) > 0) {
            for (int i = 0; i < g_populated_count; i++) {
                if (strcmp(g_populated_cache[i], current_text) == 0) {
                    UI_SetSelection(hListBox, i);
                    return;
                }
            }
        }
        
        if (current_sel != LB_ERR && current_sel < g_populated_count) {
            UI_SetSelection(hListBox, current_sel);
        } else {
            g_SelectedIndex = -1; 
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
