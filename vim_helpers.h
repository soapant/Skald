#include <windows.h>
#include <stdio.h>
#include "types.h"

static inline char* GetSelectedText(void) {
    if (!hListBox || !IsWindow(hListBox)) return NULL;
    
    int sel = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) return NULL;
    
    static char text[512];
    SendMessageA(hListBox, LB_GETTEXT, sel, (LPARAM)text);
    return text;
}

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
    
    char* selected = GetSelectedText();
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
    if (!hListBox || !IsWindow(hListBox)) return NULL;
    
    SendMessage(hListBox, LB_SETCURSEL, 0, 0);

}
static inline const char* GoToLastListBoxItem(void) {
    if (!hListBox || !IsWindow(hListBox)) return NULL;
    int totalItems = (int)SendMessage(hListBox, LB_GETCOUNT, 0, 0);

    if (totalItems != LB_ERR && totalItems > 0) 
    {
        int lastIndex = totalItems - 1;
        
        SendMessage(hListBox, LB_SETCURSEL, lastIndex, 0);
    }
}
static inline void SetCursorToStartOfText(void) {
    if (hEditControl && IsWindow(hEditControl)) {
        SendMessageA(hEditControl, EM_SETSEL, 0, 0);
    }
}

static inline void SetCursorToEndOfText(void) {
    if (hEditControl && IsWindow(hEditControl)) {
        int len = GetWindowTextLengthA(hEditControl);
        SendMessageA(hEditControl, EM_SETSEL, len, len);
    }
}
