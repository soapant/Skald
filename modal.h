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

static inline void SetMode(EditorMode mode) {
    g_current_mode = mode;
    g_vim_mode = g_mode_names[mode];  // <-- Update the global string
    
    printf("[VIM] Mode: %s\n", g_mode_names[mode]);
    
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
