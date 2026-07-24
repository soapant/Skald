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
int last_selection = 0;

static inline void SetMode(EditorMode mode) {
    g_current_mode = mode;
    g_vim_mode = g_mode_names[mode];
    if (strcmp(g_vim_mode,"INSERT") == 0 ) {
      last_selection = UI_GetSelection(hListBox);
      UI_SetSelection(hListBox,-1, g_SelectedIndex);
    } else if (strcmp(g_vim_mode,"NORMAL") == 0) {
      if (g_TotalMatches != 0) {
          if (last_selection <= 0) {
              UI_SetSelection(hListBox,0, g_SelectedIndex);
          } else {
              UI_SetSelection(hListBox,last_selection, g_SelectedIndex);
          }
      }
    }
    UpdateTextCaret(hEditControl);
    
    extern HWND hPopupWnd;
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        InvalidateRect(hPopupWnd, NULL, FALSE);
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
