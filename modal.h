// modal.h
#ifndef MODAL_H
#define MODAL_H

#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include "common_ui.h"
#include "types.h"

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
    g_vim_mode = g_mode_names[mode];
    
    extern HWND g_hEditPopup;
    bool in_popup = (g_hEditPopup != NULL && IsWindow(g_hEditPopup));
    
    if (!in_popup) {
        if (strcmp(g_vim_mode,"INSERT") == 0 ) {
          UI_SetSelection(hListBox,-1, g_SelectedIndex);
        } else if (strcmp(g_vim_mode,"NORMAL") == 0) {
          if (g_TotalMatches != 0) {
              UI_SetSelection(hListBox,0, g_SelectedIndex);
          }
        }
    }
    
    if (g_ActiveEditControl) {
        UpdateTextCaret(g_ActiveEditControl);
    }
    
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        InvalidateRect(hPopupWnd, NULL, FALSE);
        UpdateWindow(hPopupWnd);
    }
    if (g_hEditPopup != NULL && IsWindow(g_hEditPopup)) {
        InvalidateRect(g_hEditPopup, NULL, FALSE);
        UpdateWindow(g_hEditPopup);
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
