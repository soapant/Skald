#include <windows.h>
#include <stdio.h>
#include "common_ui.h"
#include "types.h"

static inline void DeleteCharUnderCursor(void) {
    if (!hEditControl || !IsWindow(hEditControl)) return;
    
    char text[512] = {0};
    GetWindowTextA(hEditControl, text, sizeof(text));
    
    DWORD start, end;
    UI_Edit_GetSel(hEditControl, &start, &end);
    
    int pos = (int)start;
    int len = (int)strlen(text);
    
    if (pos < len) {
        memmove(&text[pos], &text[pos + 1], len - pos);
        
        UI_SetText(hEditControl,text);
        
        UI_SetCursorPosition(hEditControl,pos);
    }
}

static inline void ExecuteEnter(void) {
    char text[512] = {0};
    
    char* selected = g_populated_cache[g_SelectedIndex];
    if (selected != NULL) {
        strncpy(text, selected, sizeof(text) - 1);
    } else if (hEditControl && IsWindow(hEditControl)) {
        UI_GetCurrentText(hEditControl,text);
    }
    
    if (g_enter_callback != NULL && strlen(text) > 0) {
        g_enter_callback(text);
    }
    selection_complete = true;
}

static inline const char* GoToFirstListBoxItem(void) {
    if (!hListBox || !IsWindow(hListBox) || g_populated_count <= 0) return NULL;
    
    g_SelectedIndex = 0;
    g_view_offset = 0;

    UI_LB_SetTopIndex(hListBox, 0);
    UI_LB_SetCaretIndex(hListBox, 0, FALSE);
    UI_Redraw(hListBox);

    return g_populated_cache[0];
}

static inline const char* GoToLastListBoxItem(void) {
    if (!hListBox || !IsWindow(hListBox) || g_populated_count <= 0) return NULL;
    
    int lastIndex = g_populated_count - 1;
    g_SelectedIndex = lastIndex;

    RECT rect;
    GetClientRect(hListBox, &rect);
    int itemHeight = UI_LB_GetItemHeight(hListBox);
    int visibleItems = (itemHeight > 0) ? (rect.bottom / itemHeight) : 1;
    
    int newTop = lastIndex - visibleItems + 1;
    if (newTop < 0) newTop = 0;
    g_view_offset = newTop;

    UI_LB_SetTopIndex(hListBox, newTop);
    UI_LB_SetCaretIndex(hListBox, lastIndex, FALSE);
    
    UI_Redraw(hListBox);

    return g_populated_cache[lastIndex];
}

static inline void SetCursorToStartOfText(void) {
    if (hEditControl && IsWindow(hEditControl)) {
        SetFocus(hEditControl);
        UI_SetCursorPosition(hEditControl,0);
    }
}

static inline void SetCursorToEndOfText(void) {
    if (hEditControl && IsWindow(hEditControl)) {
        SetFocus(hEditControl);
        int len = GetWindowTextLengthA(hEditControl);
        UI_SetCursorPosition(hEditControl,len);
    }
}
