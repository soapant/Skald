#ifndef COMMON_UI_H
#define COMMON_UI_H

#include <sec_api/stdio_s.h>
#include <windows.h>
#include <shlwapi.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "types.h"
#include "config.h"

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

static inline void UI_SetSelection(HWND hList, int index, int old_sel) {
    g_SelectedIndex = index;
    
    RECT itemRect;
    if (old_sel >= 0 && old_sel != index) {
        SendMessageA(hList, LB_GETITEMRECT, old_sel, (LPARAM)&itemRect);
        InvalidateRect(hList, &itemRect, FALSE);
    }
    if (index >= 0 && index != old_sel) {
        SendMessageA(hList, LB_GETITEMRECT, index, (LPARAM)&itemRect);
        InvalidateRect(hList, &itemRect, FALSE);
    }
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
    InvalidateRect(hList, NULL,FALSE);
}

// === LISTBOX WRAPPERS ===
static inline void UI_LB_SetCount(HWND hList, int count) {
    g_populated_count = count;
    SendMessageA(hList, LB_SETCOUNT, count, 0);
}

static inline void UI_LB_GetItemRect(HWND hList, int index, RECT* rect) {
    SendMessageA(hList, LB_GETITEMRECT, index, (LPARAM)rect);
}

static inline void UI_LB_SetHorizontalExtent(HWND hList, int width) {
    SendMessageA(hList, LB_SETHORIZONTALEXTENT, width, 0);
}

static inline void UI_LB_SetItemHeight(HWND hList, int height) {
    SendMessageA(hList, LB_SETITEMHEIGHT, 0, height);
}

static inline void UI_LB_SetCaretIndex(HWND hList, int index, BOOL scroll) {
    SendMessageA(hList, LB_SETCARETINDEX, index, scroll);
}

static inline void UI_LB_SetTopIndex(HWND hList, int index) {
    SendMessageA(hList, LB_SETTOPINDEX, index, 0);
}

static inline void UI_LB_SetFont(HWND hList, HFONT hFont) {
    SendMessageA(hList, WM_SETFONT, (WPARAM)hFont, FALSE);
}

static inline int UI_LB_GetItemHeight(HWND hList) {
    return (int)SendMessageA(hList, LB_GETITEMHEIGHT, 0, 0);
}

static inline int UI_LB_GetTextLen(HWND hList) {
    return (int)SendMessageA(hList, LB_GETTEXTLEN, 0, 0);
}

static inline int UI_LB_GetText(HWND hList, int index, char* buffer) {
    return (int)SendMessageA(hList, LB_GETTEXT, index, (LPARAM)buffer);
}

// === EDIT CONTROL WRAPPERS ===
static inline void UI_Edit_SetFont(HWND hEdit, HFONT hFont) {
    SendMessageA(hEdit, WM_SETFONT, (WPARAM)hFont, FALSE);
}

static inline void UI_Edit_SetSel(HWND hEdit, DWORD start, DWORD end) {
    SendMessageA(hEdit, EM_SETSEL, start, end);
}

static inline void UI_Edit_GetSel(HWND hEdit, DWORD* start, DWORD* end) {
    SendMessageA(hEdit, EM_GETSEL, (WPARAM)start, (LPARAM)end);
}

static inline void UI_Edit_UpdateUIState(HWND hEdit, UINT action, UINT uiFlags) {
    SendMessageA(hEdit, WM_UPDATEUISTATE, MAKEWPARAM(action, uiFlags), 0);
}

// === WINDOW FONT WRAPPER ===
static inline HFONT UI_GetFont(HWND hWnd) {
    return (HFONT)SendMessageA(hWnd, WM_GETFONT, 0, 0);
}

// === KEYBOARD SIMULATION ===
static inline void UI_Edit_KeyDown(HWND hEdit, WPARAM vk) {
    SendMessageA(hEdit, WM_KEYDOWN, vk, 0);
}
static inline void UI_Edit_KeyUp(HWND hEdit, WPARAM vk) {
    SendMessageA(hEdit, WM_KEYUP, vk, 0);
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
        current_text[sizeof(current_text) - 1] = '\0';
    }
    
    int total_matches = 0;
    char clean_filter[256] = {0};
    if (filter) {
        strncpy(clean_filter, filter, sizeof(clean_filter) - 1);
        clean_filter[sizeof(clean_filter) - 1] = '\0';
    }
    
    // === INCREMENTAL FILTERING ===
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
                g_matching_indices[total_matches] = idx;
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
                g_matching_indices[total_matches] = i;
                total_matches++;
            }
        }
    }
    
    if (strlen(clean_filter) > 0 && g_history_count < 20) {
        strncpy(g_filter_history[g_history_count], clean_filter, sizeof(g_filter_history[0]) - 1);
        g_match_count_history[g_history_count] = total_matches;
        memcpy(g_match_history[g_history_count], g_matching_indices, total_matches * sizeof(int));
        g_history_count++;
        printf("[HISTORY] Saved '%s' (%d matches)\n", clean_filter, total_matches);
    }
    
    if (total_matches == 0 && item_count > 0 && strlen(clean_filter) == 0) {
        int force_count = (item_count < 30) ? item_count : 30;
        for (int i = 0; i < force_count; i++) {
            g_matching_indices[total_matches] = i;
            total_matches++;
        }
        g_history_count = 0;
    }
    
int pool_max = MAX_VISIBLE_ITEMS;
    
    // Cap total matches at pool_max - no virtual scrolling, just show first N
    if (total_matches > pool_max) {
        total_matches = pool_max;
    }
    
    g_populated_count = 0;
    
    for (int i = 0; i < total_matches; i++) {
        int raw_data_index = g_matching_indices[i];
        if (!items[raw_data_index]) continue;
        
        strncpy(g_populated_cache[g_populated_count], items[raw_data_index], 511);
        g_populated_cache[g_populated_count][511] = '\0';
        g_populated_cache[g_populated_count][strcspn(g_populated_cache[g_populated_count], "\r\n")] = '\0';
        g_populated_count++;
    }
    
    UI_LB_SetCount(hListBox, total_matches);
    if (total_matches == 1) {
        UI_SetSelection(hListBox, 0, g_SelectedIndex);
    } else {
        UI_SetSelection(hListBox, -1, g_SelectedIndex);
    }
    if (total_matches == 0) {
      if (hDisplayMessage != NULL) {
        KillTimer(hDisplayMessage, 999);
        DestroyWindow(hDisplayMessage);
        hDisplayMessage = NULL;
      }
    }
    
g_last_items = items;
    g_last_item_count = item_count;
    g_TotalMatches = total_matches;
}

COLORREF GetRainbowColor(float t, float s, float v) {
    if (RainbowText) {
        float hue = t * 240.0f;
        float h = hue / 60.0f;
        int i = ((int)h) % 6;
        float f = h - (int)h;
        float p = v * (1 - s);
        float q = v * (1 - s * f);
        float t_ = v * (1 - s * (1 - f));
        float r, g, b;
        switch (i) {
            case 0: r = v; g = t_; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t_; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t_; g = p; b = v; break;
            case 5: r = v; g = p; b = q; break;
        }
        return RGB((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255));
    } else {
      return COLOR_TEXT;
    }
}

HFONT CreateCustomNerdFont(int height, const char* face_name) {
    return CreateFontA(
        height, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        face_name
    );
}

void UpdateTextCaret(HWND hwndEditControl)
{
    int caretWidth = 1;   
    int caretHeight = FontSize; 

    if (IsInsertMode()) {
        caretWidth = 2;   
    } 
    else if (IsNormalMode()) {
        caretWidth = 12;  
    } 
    else {
        caretWidth = 12;  
    }

    static int lastWidth = -1;
    static HWND lastHwnd = NULL;
    
    if (caretWidth != lastWidth || hwndEditControl != lastHwnd) {
        DestroyCaret();
        CreateCaret(hwndEditControl, NULL, caretWidth, caretHeight);
        ShowCaret(hwndEditControl);
        
        SetCaretBlinkTime(INFINITE);
        
        lastWidth = caretWidth;
        lastHwnd = hwndEditControl;
    }
}
POINT ptPrimary = { 0, 0 }; 
void CalculateWindowMetrics(int count, WindowLayout* layout,int height , int width,int item_height) {
    POINT ptPrimary = { 0, 0 }; 
    HMONITOR hMonitor = MonitorFromPoint(ptPrimary, MONITOR_DEFAULTTOPRIMARY);

    MONITORINFO mi = {0};
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfoA(hMonitor, &mi);

    int monitorWidth = mi.rcMonitor.right - mi.rcMonitor.left;
    int monitorHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
    
    layout->w = 900;          
    layout->item_height = item_height; 

    layout->h = 95 + (count > 0 ? (count * layout->item_height) : 0); 

    if (layout->h < height) layout->h = height;
    if (layout->h > width) layout->h = width; 

    layout->x = mi.rcMonitor.left + (monitorWidth - layout->w) / 2;
    layout->y = mi.rcMonitor.top + (monitorHeight - layout->h) / 2;
}

void CreateChildControls(HWND hwndParent, HFONT hFont, const char* list[], int count, const WindowLayout* layout) {
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(hwndParent, GWLP_HINSTANCE);
    
    hEditControl = CreateWindowExA(
        0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 50, layout->w - 40, 40, hwndParent, (HMENU)IDC_INPUT_BOX, hInstance, NULL
    );
    if (!hEditControl) return;
    
    g_ActiveEditControl = hEditControl;
    UI_Edit_SetFont(hEditControl, hFont);
    SetWindowSubclass(hEditControl, EditSubclassProc, 0, 0);



    if (count > 0) {

        hListBox = CreateWindowExA(
            0, "LISTBOX", "", 
            WS_CHILD | WS_VISIBLE | LBS_NODATA | LBS_OWNERDRAWFIXED,
            20, 105, layout->w - 40, layout->h - 115, hwndParent, (HMENU)IDC_LIST_BOX, hInstance, NULL
        );

        if (!hListBox) return;
        
        SetWindowSubclass(hListBox, ListBoxSubclassProc, 0, 0);
        UI_LB_SetFont(hListBox, hFont);
        UI_LB_SetItemHeight(hListBox, layout->item_height);

        int max_width = layout->w - 40;
        HDC hdc = GetDC(hListBox);
        if (!hdc) return;
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        
        g_initial_items = list;        
        g_initial_count = count;
        
        UI_SetItemCount(hListBox, count);
        int countCheck = UI_GetCount(hListBox);
        
        UI_Redraw(hListBox);
        UpdateWindow(hListBox); 
        
        GdiFlush(); 
        
        SelectObject(hdc, hOldFont);
        ReleaseDC(hListBox, hdc);
        UI_SetHorizontalExtent(hListBox, max_width);
        
        g_refresh_callback("", hListBox);
    }
}

void SimpleCreateChildControls(HWND hwndParent, HFONT hFont, const char* string, int count, const WindowLayout* layout) { HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(hwndParent, GWLP_HINSTANCE);
    
    hEditControl = CreateWindowExA(
        0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 50, layout->w - 40, 40, hwndParent, (HMENU)IDC_INPUT_BOX, hInstance, NULL
    );
    
    if (!hEditControl) return;
    
    g_ActiveEditControl = hEditControl;
    UI_Edit_SetFont(hEditControl, hFont);
    SetWindowSubclass(hEditControl, SimpleEditSubclassProc, 0, 0);

    if (count > 0) {
        hListBox = CreateWindowExA(
            0, "LISTBOX", "", 
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOSEL, 
            20, 105, layout->w - 40, layout->h - 115, hwndParent, (HMENU)IDC_LIST_BOX, hInstance, NULL
        );

        if (!hListBox) return;
        
        SetWindowSubclass(hListBox, SimpleListBoxSubclassProc, 0, 0);
        UI_LB_SetFont(hListBox, hFont);
        UI_LB_SetItemHeight(hListBox, layout->item_height);

        int max_width = layout->w - 40;
        HDC hdc = GetDC(hListBox);
        if (!hdc) return;
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        
        SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)string);
        SendMessage(hListBox, LB_SETCURSEL, (WPARAM)-1, 0);
        UpdateWindow(hListBox); 
        
        GdiFlush(); 
        
        SelectObject(hdc, hOldFont);
        ReleaseDC(hListBox, hdc);
        
    }
}
void ForceWindowFocus(HWND hwndTarget, HWND hwndInput) {
    ShowWindow(hwndTarget, SW_SHOW);

    SetWindowPos(hwndTarget, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW | SWP_ASYNCWINDOWPOS);

    HWND hForeground = GetForegroundWindow();
    DWORD foregroundThreadID = GetWindowThreadProcessId(hForeground, NULL);
    DWORD targetThreadID = GetWindowThreadProcessId(hwndTarget, NULL);
    DWORD currentThreadID = GetCurrentThreadId();

    if (foregroundThreadID != currentThreadID)
        AttachThreadInput(currentThreadID, foregroundThreadID, TRUE);
    if (targetThreadID != currentThreadID)
        AttachThreadInput(currentThreadID, targetThreadID, TRUE);

    SetForegroundWindow(hwndTarget);
    SetActiveWindow(hwndTarget);
    BringWindowToTop(hwndTarget);

    if (hwndInput != NULL) {
        SetFocus(hwndInput);
        UI_Edit_UpdateUIState(hwndInput, UIS_CLEAR, UISF_HIDEFOCUS);
        UI_Edit_SetSel(hwndInput, 0, 0);
    }

    if (foregroundThreadID != currentThreadID)
        AttachThreadInput(currentThreadID, foregroundThreadID, FALSE);
    if (targetThreadID != currentThreadID)
        AttachThreadInput(currentThreadID, targetThreadID, FALSE);
}


static inline bool IsPopupActive(HWND hPopupWnd) {
    return hPopupWnd != NULL && IsWindow(hPopupWnd);
}

static inline bool CheckAndBlockIfPopupActive(HWND hPopupWnd) {
    return IsPopupActive(hPopupWnd);
}
static inline void UI_GetCurrentCursorPosition(HWND hEditControl, DWORD *start, DWORD *end) {
    if (start && end) {
        UI_Edit_GetSel(hEditControl, start, end);
    }
}
static inline void UI_SetCursorPosition(HWND hEditControl,int position) {
    UI_Edit_SetSel(hEditControl, position, position);
}
static inline char* UI_GetCurrentText(HWND hEditControl,char *text) {
   GetWindowTextA(hEditControl, text, sizeof(text));
}
static inline void UI_SetText(HWND hEditControl,char* text) {
  SetWindowTextA(hEditControl,text);
}
static inline int UI_GetText(HWND hEdit, char* buffer, int bufferSize) {
    if (!hEdit || !buffer || bufferSize <= 0) return 0;
    int i = GetWindowTextA(hEdit, buffer, bufferSize);
    return i;
}


#endif 
