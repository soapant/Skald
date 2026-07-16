// vim_motions.h
#ifndef VIM_MOTIONS_H
#define VIM_MOTIONS_H

#include <windows.h>
#include <stdio.h>
#include "types.h"
#include "helpers.h"
#include "config.h"
#include "modal.h"
#include "character_sequence.h"
#include "vim_helpers.h"
#include "calc.h"

// ============================================================
// MOTION ACTION ENUM
// ============================================================
typedef enum {
    MOTION_NONE = 0,
    MOTION_ENTER_NORMAL,
    MOTION_DELETE_LINE_INSERT,
    MOTION_DELETE_CHAR_INSERT,
    MOTION_ENTER_INSERT_OUT,
    MOTION_ENTER_INSERT_IN,
    MOTION_ENTER_INSERT_OUT_END,
    MOTION_ENTER_INSERT_IN_START,
    MOTION_ENTER_VISUAL,
    MOTION_ENTER_VISUAL_LINE,
    MOTION_MOVE_LEFT,
    MOTION_MOVE_RIGHT,
    MOTION_MOVE_UP,
    MOTION_MOVE_DOWN,
    MOTION_MOVE_WORD_FORWARD,
    MOTION_MOVE_WORD_BACK,
    MOTION_MOVE_LINE_START,
    MOTION_MOVE_LINE_END,
    MOTION_MOVE_LISTBOX_START,
    MOTION_MOVE_LISTBOX_END,
    MOTION_DELETE_CHAR,
    MOTION_DELETE_CHAR_PERSISTENT,
    MOTION_DELETE_LINE,
    MOTION_DELETE_WORD,
    MOTION_CHANGE_TO_END,
    MOTION_CHANGE_LINE,
    MOTION_YANK_LINE,
    MOTION_YANK_LIST_BOX,
    MOTION_YANK_WORD,
    MOTION_YANK_TO_END,
    MOTION_UNDO,
    MOTION_SEARCH_FORWARD,
    MOTION_SEARCH_BACKWARD,
    MOTION_EXIT_WINDOW,
} MotionAction;

typedef struct {
    int vk_code;
    int modifiers;
    MotionAction action_id;
    const char* name;
    void (*handler)(void);
    EditorMode mode; 
} MotionBinding;

typedef struct {
    const char* sequence;
    MotionAction action_id;
    const char* name;
    void (*handler)(void);
    bool is_prefix;
    EditorMode mode; 
} SequenceBinding;



static inline void HandleEnterNormal(void) { 
    SetMode(MODE_NORMAL); 
    printf("[VIM] Entered NORMAL mode\n");
}

static inline void HandleEnterInsertOut(void) { 
    if (!hEditControl || !IsWindow(hEditControl)) return;
    
    DWORD start, end;
    SendMessageA(hEditControl, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    
    int pos = (int)start + 1;
    
    SendMessageA(hEditControl, EM_SETSEL, pos, pos);
    
    SetMode(MODE_INSERT); 
    printf("[VIM] Entered INSERT mode\n");
}

static inline void HandleEnterInsertIn(void) { 
    if (!hEditControl || !IsWindow(hEditControl)) return;
    
    DWORD start, end;
    SendMessageA(hEditControl, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    
    int pos = (int)start - 1;
    
    SendMessageA(hEditControl, EM_SETSEL, pos, pos);
    
    SetMode(MODE_INSERT); 
    
    printf("[VIM] Entered INSERT mode\n");
}

static inline void HandleEnterVisual(void) { 
  
    SetMode(MODE_VISUAL); 
    printf("[VIM] Entered VISUAL mode\n");
}


static inline void HandleMoveLeft(void) {
    if (hEditControl && IsWindow(hEditControl)) {
        SendMessageA(hEditControl, WM_KEYDOWN, VK_LEFT, 0);
        SendMessageA(hEditControl, WM_KEYUP, VK_LEFT, 0);
    }
}

static inline void HandleMoveRight(void) {
    if (hEditControl && IsWindow(hEditControl)) {
        SendMessageA(hEditControl, WM_KEYDOWN, VK_RIGHT, 0);
        SendMessageA(hEditControl, WM_KEYUP, VK_RIGHT, 0);
    }
}

static inline void HandleMoveUp(void) {
    if (hEditControl && IsWindow(hEditControl)) {
        SendMessageA(hEditControl, WM_KEYDOWN, VK_UP, 0);
        SendMessageA(hEditControl, WM_KEYUP, VK_UP, 0);
    }
}

static inline void HandleMoveDown(void) {
    if (hEditControl && IsWindow(hEditControl)) {
        SendMessageA(hEditControl, WM_KEYDOWN, VK_DOWN, 0);
        SendMessageA(hEditControl, WM_KEYUP, VK_DOWN, 0);
    }
}

static inline void HandlePersistentDelete(void) {
    const char* feature = GetCurrentFeature();
    int selected_index = LB_ERR;
    
    if (hListBox && IsWindow(hListBox)) {
        selected_index = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
    }
    
    if (selected_index != LB_ERR) {
        if (strstr(feature, "Todo") != NULL) {
            char text[512] = {0};
            SendMessageA(hListBox, LB_GETTEXT, selected_index, (LPARAM)text);
            if (g_ctrl_enter_callback != NULL) {
                g_ctrl_enter_callback(text);
            }
            printf("[VIM] Deleted todo item: %s\n", text);
            return;
        }
    }
    if (hEditControl && IsWindow(hEditControl)) {
        DeleteCharUnderCursor();
        printf("[VIM] Deleted character under cursor\n");
    }
}
static inline void HandleDelete(void) {
    const char* feature = GetCurrentFeature();
    int selected_index = LB_ERR;
    
    if (hListBox && IsWindow(hListBox)) {
        selected_index = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
    }
    
    if (selected_index != LB_ERR) {
        if (strstr(feature, "Todo") != NULL) {
            char text[512] = {0};
            SendMessageA(hListBox, LB_GETTEXT, selected_index, (LPARAM)text);
            if (g_enter_callback != NULL) {
                g_enter_callback(text);
            }
            selection_complete = true;
            return;
        }
    }
    if (hEditControl && IsWindow(hEditControl)) {
        DeleteCharUnderCursor();
        printf("[VIM] Deleted character under cursor\n");
    }
}
static inline void HandleDeleteInsert(void) {
    if (hEditControl && IsWindow(hEditControl)) {
        DeleteCharUnderCursor();
        HandleEnterInsertIn();
        printf("[VIM] Deleted character under cursor\n");
    }
}
static inline void HandleDeleteLine(void) {
    const char* feature = GetCurrentFeature();
    int selected_index = LB_ERR;
    
    if (hListBox && IsWindow(hListBox)) {
        selected_index = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
    }
    
    if (selected_index != LB_ERR) {
        if (strstr(feature, "Todo") != NULL) {
            char text[512] = {0};
            SendMessageA(hListBox, LB_GETTEXT, selected_index, (LPARAM)text);
            if (g_enter_callback != NULL) {
                g_enter_callback(text);
            }
            selection_complete = true;
            return;
        }
    }
    if (hEditControl && IsWindow(hEditControl)) {
        SetWindowTextA(hEditControl, "");
        printf("[VIM] Deleted Entire Search Box \n");
    }
}
static inline void HandleDeleteLineInsert(void) {
    const char* feature = GetCurrentFeature();
    int selected_index = LB_ERR;
    
    if (hListBox && IsWindow(hListBox)) {
        selected_index = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
    }
    
    if (selected_index != LB_ERR) {
        if (strstr(feature, "Todo") != NULL) {
            char text[512] = {0};
            SendMessageA(hListBox, LB_GETTEXT, selected_index, (LPARAM)text);
            if (g_enter_callback != NULL) {
                g_enter_callback(text);
            }
            selection_complete = true;
            return;
        }
    }
    if (hEditControl && IsWindow(hEditControl)) {
        SetWindowTextA(hEditControl, "");
        SetMode(MODE_INSERT);
        printf("[VIM] Deleted Entire Search Box \n");
    }
}
static inline void HandleMoveListboxStart(void) {
    GoToFirstListBoxItem();
}
static inline void HandleMoveListboxEnd(void) {
    GoToLastListBoxItem();
}
static inline void HandleMoveLineEnd(void) {
  SetCursorToEndOfText();
}
static inline void HandleMoveLineStart(void) {
  SetCursorToStartOfText();
}

static inline void HandleEnterInsertInStart(void) {
  SetCursorToStartOfText();
  HandleEnterInsertIn();
}

static inline void HandleEnterInsertOutEnd(void) {
  SetCursorToEndOfText();
  HandleEnterInsertOut();
}
static inline void HandleExitWindow(void) {
  selection_complete = true;
}

static inline void HandleYankListBox(void) {
  int selected_index = LB_ERR;
  
  if (hListBox && IsWindow(hListBox)) {
      selected_index = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
  }
  if (selected_index != LB_ERR) {
        char text[512] = {0};
        SendMessageA(hListBox, LB_GETTEXT, selected_index, (LPARAM)text);
        CopyToClipboard(text);
        printf("[VIM] COPIED %s TO THE CLIPBOARD",text);
        return;
  }
}
static inline void HandleYankLine(void) {
  if (hEditControl && IsWindow(hEditControl)) {
      char textBuffer[256];
      GetWindowText(hEditControl, textBuffer, sizeof(textBuffer));
      printf("[VIM] COPIED %s TO THE CLIPBOARD",textBuffer);
  }
}

#endif
