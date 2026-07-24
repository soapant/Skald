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
    return sel;
}


static inline void UI_ResetContent(HWND hList) {
    g_populated_count = 0;
    SendMessageA(hList, LB_SETCOUNT, 0, 0);
}

static inline void UI_SetSelection(HWND hList, int index) {
    g_SelectedIndex = index;
    InvalidateRect(hList, NULL, TRUE);
}

static inline int UI_GetCount(HWND hList) {
    return g_populated_count;
}
static inline bool UI_IsListBoxSelected(void) {
  return g_SelectedIndex != -1;
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


#include "modal.h"


static inline void PopulateListBox(HWND hListBox, const char* filter, 
                                   const char** items, int item_count,
                                   bool (*filter_match)(const char* item, const char* filter)) {
    if (!hListBox) return;
    
    int current_sel = g_SelectedIndex; 
    char current_text[256] = {0};
    if (current_sel >= 0 && current_sel < g_populated_count) {
        printf("[DEBUG] Restoring selection: index=%d, count=%d\n", current_sel, g_populated_count);
        strncpy(current_text, g_populated_cache[current_sel], sizeof(current_text) - 1);
        current_text[sizeof(current_text) - 1] = '\0';
        printf("[DEBUG] Restored text: '%s'\n", current_text);
    } else {
        if (current_sel < 0) {
            printf("[DEBUG] No selection to restore (current_sel=%d)\n", current_sel);
        } else {
            printf("[DEBUG] Selection out of range: index=%d, count=%d\n", current_sel, g_populated_count);
        }
    }
    
    int total_matches = 0;
    static int matching_indices[4000];
    char clean_filter[256] = {0};
    if (filter) {
        strncpy(clean_filter, filter, sizeof(clean_filter) - 1);
    }
    int found_in_history = 0;
    if (!found_in_history) {
        int use_previous = 0;
        if (strlen(clean_filter) > 0 && 
            g_last_items == items && 
            g_last_item_count == item_count &&
            g_history_count > 0) {
            
            char* last_filter = g_filter_history[g_history_count - 1];
            if (strlen(clean_filter) > strlen(last_filter) &&
                strncmp(clean_filter, last_filter, strlen(last_filter)) == 0) {
                
                use_previous = 1;
                printf("[OPTIMIZE] Incremental: '%s' -> '%s'\n", last_filter, clean_filter);
            }
        }
        
        if (use_previous && g_match_count_history[g_history_count - 1] > 0) {
            int prev_count = g_match_count_history[g_history_count - 1];
            int* prev_matches = g_match_history[g_history_count - 1];
            
            for (int i = 0; i < prev_count && total_matches < 4000; i++) {
                int idx = prev_matches[i];
                if (!items[idx]) continue;
                
                bool match = false;
                if (filter_match) {
                    match = filter_match(items[idx], clean_filter);
                } else {
                    match = (StrStrIA(items[idx], clean_filter) != NULL);
                }
                
                if (match) {
                    matching_indices[total_matches] = idx;
                    total_matches++;
                }
            }
        } else {
            for (int i = 0; i < item_count && total_matches < 4000; i++) {
                if (!items[i]) continue;
                
                bool match = false;
                if (strlen(clean_filter) == 0) {
                    match = true;
                } else if (filter_match) {
                    match = filter_match(items[i], clean_filter);
                } else {
                    match = (StrStrIA(items[i], clean_filter) != NULL);
                }
                
                if (match) {
                    matching_indices[total_matches] = i;
                    total_matches++;
                }
            }
        }
        
        if (strlen(clean_filter) > 0 && g_history_count < 20) {
            strncpy(g_filter_history[g_history_count], clean_filter, sizeof(g_filter_history[0]) - 1);
            g_match_count_history[g_history_count] = total_matches;
            memcpy(g_match_history[g_history_count], matching_indices, total_matches * sizeof(int));
            g_history_count++;
            printf("[HISTORY] Saved '%s' (%d matches)\n", clean_filter, total_matches);
        }
        
    }
    if (total_matches == 0 && item_count > 0 && strlen(clean_filter) == 0) {
        int force_count = (item_count < 30) ? item_count : 30;
        for (int i = 0; i < force_count; i++) {
            matching_indices[total_matches] = i;
            total_matches++;
        }
        g_history_count = 0;
    }
    
    
    int pool_max = 60;
    
    if (current_sel == -1 || current_sel == LB_ERR) {
        g_view_offset = 0;
    } else {
        if (current_sel < g_view_offset) {
            g_view_offset = current_sel; 
        } else if (current_sel >= g_view_offset + pool_max) {
            g_view_offset = current_sel - pool_max + 1; 
        }
    }
    
    if (g_view_offset + pool_max > total_matches) {
        g_view_offset = total_matches - pool_max;
    }
    if (g_view_offset < 0) g_view_offset = 0;
    
    int display_count = total_matches - g_view_offset;
    if (display_count > pool_max) display_count = pool_max;
    
    UI_ResetContent(hListBox);
    
    for (int i = 0; i < display_count; i++) {
        int raw_data_index = matching_indices[g_view_offset + i];
        if (!items[raw_data_index]) continue;
        
        strncpy(g_populated_cache[g_populated_count], items[raw_data_index], 511);
        
        g_populated_cache[g_populated_count][511] = '\0';
        if (!found_in_history) {
            g_populated_cache[g_populated_count][strcspn(g_populated_cache[g_populated_count], "\r\n")] = '\0';
        } else {
            g_populated_cache[g_populated_count - 1][strcspn(g_populated_cache[g_populated_count], "\r\n")] = '\0';
        }
        
        g_populated_count++;
    }

    int window_allocation_count = (g_populated_count < 30) ? 30 : g_populated_count;
    SendMessageA(hListBox, LB_SETCOUNT, window_allocation_count, 0);
    if (total_matches == 1) {
      UI_SetSelection(hListBox,0);
    } else {
      UI_SetSelection(hListBox,-1);
    }
    
    
    g_last_items = items;
    g_last_item_count = item_count;
    g_TotalMatches = total_matches;
    
}


static inline bool IsPopupActive(HWND hPopupWnd) {
    return hPopupWnd != NULL && IsWindow(hPopupWnd);
}

static inline bool CheckAndBlockIfPopupActive(HWND hPopupWnd) {
    return IsPopupActive(hPopupWnd);
}
static inline void UI_GetCurrentCursorPosition(HWND hEditControl, DWORD *start, DWORD *end) {
    if (start && end) {
        SendMessageA(hEditControl, EM_GETSEL, (WPARAM)start, (LPARAM)end);
    }
}
static inline void UI_SetCursorPosition(HWND hEditControl,int position) {
    SendMessageA(hEditControl, EM_SETSEL, position, position);
}
static inline char* UI_GetCurrentText(HWND hEditControl,char *text) {
   GetWindowTextA(hEditControl, text, sizeof(text));
}
static inline void UI_SetText(HWND hEditControl,char* text) {
  SetWindowTextA(hEditControl,text);
}


#endif 
