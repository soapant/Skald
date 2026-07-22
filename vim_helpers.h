#include <windows.h>
#include <stdio.h>
#include "types.h"

static inline void DeleteCharUnderCursor(void) {
    if (!hEditControl || !IsWindow(hEditControl)) return;
    
    // Get current text
    char text[512] = {0};
    GetWindowTextA(hEditControl, text, sizeof(text));
    
    // Get cursor position
    DWORD start, end;
    SendMessageA(hEditControl, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    
    int pos = (int)start;
    int len = (int)strlen(text);
    
    if (pos < len) {
        // Remove character at cursor position
        memmove(&text[pos], &text[pos + 1], len - pos);
        SetWindowTextA(hEditControl, text);
        
        // Set cursor back to same position
        SendMessageA(hEditControl, EM_SETSEL, pos, pos);
    }
}

static inline void ExecuteEnter(void) {
    char text[512] = {0};
    
    char* selected = g_populated_cache[g_SelectedIndex];
    if (selected != NULL) {
        strncpy(text, selected, sizeof(text) - 1);
    } else if (hEditControl && IsWindow(hEditControl)) {
        GetWindowTextA(hEditControl, text, sizeof(text));
    }
    
    if (g_enter_callback != NULL && strlen(text) > 0) {
        g_enter_callback(text);
    }
    selection_complete = true;
}

static inline const char* GetCurrentFeature(void) {

    if (g_title == NULL) return "Unknown";
    
    if (strstr(g_title, "LAUNCHER") != NULL) return "AppLauncher";
    if (strstr(g_title, "TODO") != NULL) return "Todo";
    if (strstr(g_title, "Calculator") != NULL) return "Calculator";
    if (strstr(g_title, "Terminal") != NULL) return "Terminal";
    if (strstr(g_title, "Google") != NULL) return "Google";
    if (strstr(g_title, "Explorer") != NULL) return "Explorer";
    if (strstr(g_title, "Zoxide") != NULL) return "Zoxide";
    if (strstr(g_title, "Neovide") != NULL) return "Neovide";
    if (strstr(g_title, "Nvim") != NULL) return "Nvim";
    if (strstr(g_title, "Yazi") != NULL) return "Yazi";
    if (strstr(g_title, "ProcessKiller") != NULL) return "ProcessKiller";
    if (strstr(g_title, "Volume") != NULL) return "Volume";
    if (strstr(g_title, "Timer") != NULL) return "Timer";
    if (strstr(g_title, "Switcher") != NULL) return "WindowSwitcher";
    
    return "Unknown";
}
static inline const char* GoToFirstListBoxItem(void) {
    if (!hListBox || !IsWindow(hListBox) || g_populated_count <= 0) return NULL;
    
    g_SelectedIndex = 0;
    g_view_offset = 0;

    SendMessageA(hListBox, LB_SETTOPINDEX, 0, 0);
    SendMessageA(hListBox, LB_SETCARETINDEX, 0, FALSE);
    InvalidateRect(hListBox, NULL, FALSE);

    return g_populated_cache[0];
}

static inline const char* GoToLastListBoxItem(void) {
    if (!hListBox || !IsWindow(hListBox) || g_populated_count <= 0) return NULL;
    
    // 1. Calculate the final valid index boundary
    int lastIndex = g_populated_count - 1;
    g_SelectedIndex = lastIndex;

    // 2. Compute sliding viewport constraints so it smoothly scrolls to the bottom
    RECT rect;
    GetClientRect(hListBox, &rect);
    int itemHeight = (int)SendMessageA(hListBox, LB_GETITEMHEIGHT, 0, 0);
    int visibleItems = (itemHeight > 0) ? (rect.bottom / itemHeight) : 1;
    
    int newTop = lastIndex - visibleItems + 1;
    if (newTop < 0) newTop = 0;
    g_view_offset = newTop; // Align the sliding tracker

    // 3. Force the Win32 list box window to scroll down
    SendMessageA(hListBox, LB_SETTOPINDEX, newTop, 0);
    SendMessageA(hListBox, LB_SETCARETINDEX, lastIndex, FALSE);
    InvalidateRect(hListBox, NULL, FALSE);

    // 4. FIXED: Return the actual string text sitting at the bottom of your list
    return g_populated_cache[lastIndex];
}

static inline void SetCursorToStartOfText(void) {
    if (hEditControl && IsWindow(hEditControl)) {
        // Explicitly set focus to the edit control so the caret shows up instantly
        SetFocus(hEditControl);
        SendMessageA(hEditControl, EM_SETSEL, 0, 0);
    }
}

static inline void SetCursorToEndOfText(void) {
    if (hEditControl && IsWindow(hEditControl)) {
        SetFocus(hEditControl);
        int len = GetWindowTextLengthA(hEditControl);
        SendMessageA(hEditControl, EM_SETSEL, len, len);
    }
}
