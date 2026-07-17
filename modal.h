// modal.h
#ifndef MODAL_H
#define MODAL_H

#include <windows.h>
#include <stdbool.h>
#include <stdio.h>

typedef enum {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_VISUAL,
} EditorMode;

static EditorMode g_current_mode = MODE_INSERT;
static bool g_vim_mode_enabled = true;
static const char* g_mode_names[] = { "NORMAL", "INSERT", "VISUAL" };
const char* g_vim_mode = "INSERT";

// Function to update cursor
static inline void UpdateCursorForMode(EditorMode mode) {
    extern HWND hPopupWnd;
    if (!hPopupWnd || !IsWindow(hPopupWnd)) return;
    
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(hPopupWnd, &pt);
    SendMessageA(hPopupWnd, WM_SETCURSOR, (WPARAM)hPopupWnd, MAKELPARAM(HTCLIENT, 0));
}

static inline void SetMode(EditorMode mode) {
    g_current_mode = mode;
    g_vim_mode = g_mode_names[mode];
    printf("[VIM] Mode: %s\n", g_mode_names[mode]);
    
    extern HWND hListBox;
    if (mode == MODE_INSERT && hListBox && IsWindow(hListBox)) {
        SendMessageA(hListBox, LB_SETCURSEL, -1, 0);  // -1 = no selection
        printf("[VIM] Cleared listbox selection (entering INSERT)\n");
    }
    
    UpdateCursorForMode(mode);
    
    extern HWND hPopupWnd;
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        InvalidateRect(hPopupWnd, NULL, TRUE);
        UpdateWindow(hPopupWnd);
    }
}

static inline const char* GetModeName(void) {
    return g_mode_names[g_current_mode];
}

static inline EditorMode GetMode(void) { return g_current_mode; }
static inline bool IsNormalMode(void) { return g_current_mode == MODE_NORMAL; }
static inline bool IsInsertMode(void) { return g_current_mode == MODE_INSERT; }
static inline bool IsVisualMode(void) { return g_current_mode == MODE_VISUAL; }

#endif
