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
#include "common_ui.h"

typedef enum {
    MOTION_NONE = 0,
    MOTION_ENTER_NORMAL,
    MOTION_NORMALENTER,
    MOTION_NORMALCTRL_ENTER,
    MOTION_INSERT_ENTER,
    MOTION_INSERTCTRL_ENTER,
    MOTION_DELETE_LINE_INSERT,
    MOTION_DELETE_LINE_NORMAL,
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
    MOTION_MOVE_WORD_BACKWARD,
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
    
    DWORD start = 0, end = 0;
    UI_GetCurrentCursorPosition(hEditControl, &start, &end);
    
    int pos = (int)start + 1;
    
    UI_SetCursorPosition(hEditControl,pos);
    
    SetMode(MODE_INSERT); 
    printf("[VIM] Entered INSERT mode\n");
}

static inline void HandleEnterInsertIn(void) { 
    if (!hEditControl || !IsWindow(hEditControl)) return;
    
    DWORD start = 0, end = 0;
    UI_GetCurrentCursorPosition(hEditControl, &start, &end);
    
    int pos = (int)start;
    
    UI_SetCursorPosition(hEditControl,pos);
    
    SetMode(MODE_INSERT); 
    
    printf("[VIM] Entered INSERT mode\n");
}

static inline void HandleEnterVisual(void) { 
  
    SetMode(MODE_VISUAL); 
    printf("[VIM] Entered VISUAL mode\n");
}


static inline void HandleMoveLeft(void) {
    if (hEditControl && IsWindow(hEditControl) && g_SelectedIndex == -1) {
        SendMessageA(hEditControl, WM_KEYDOWN, VK_LEFT, 0);
        SendMessageA(hEditControl, WM_KEYUP, VK_LEFT, 0);
    }
}

static inline void HandleMoveRight(void) {
    if (hEditControl && IsWindow(hEditControl) && g_SelectedIndex == -1) {
        SendMessageA(hEditControl, WM_KEYDOWN, VK_RIGHT, 0);
        SendMessageA(hEditControl, WM_KEYUP, VK_RIGHT, 0);
    } else {
      if (hListBox != NULL && UI_IsListBoxSelected()) {
        SendMessageA(hEditControl, WM_KEYDOWN, VK_RETURN, 0);
        SendMessageA(hEditControl, WM_KEYUP, VK_RETURN, 0);
      }
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
    char text[512] = {0};
    
    strncpy(text, g_populated_cache[g_SelectedIndex], sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';
    
    if (g_ctrl_enter_callback != NULL) {
        g_ctrl_enter_callback(text);
    }
    
    return;
    
}

static inline void HandleDelete(void) {
    const char* feature = GetCurrentFeature();
    
      char text[512] = {0};
      
      strncpy(text, g_populated_cache[g_SelectedIndex], sizeof(text) - 1);
      text[sizeof(text) - 1] = '\0';
      
      if (g_enter_callback != NULL) {
          g_enter_callback(text);
      }
      selection_complete = true;
      return;
    
}

static inline void HandleDeleteCharInsert(void) {
    if (hEditControl && IsWindow(hEditControl)) {
        DeleteCharUnderCursor();
        HandleEnterInsertIn(); 
        printf("[VIM] Deleted character under cursor & Switched to Insert\n");
    }
}

static inline void HandleDeleteCharNormal(void) {
    if (hEditControl && IsWindow(hEditControl)) {
        DeleteCharUnderCursor();
        printf("[VIM] Deleted character under cursor\n");
    }
}

static inline void HandleDeleteLine(void) {
    const char* feature = GetCurrentFeature();
    
    if (IsNormalMode() && g_SelectedIndex >= 0 && g_SelectedIndex < g_populated_count) {
        if (strstr(feature, "Todo") != NULL) {
            char text[512] = {0};
            
            strncpy(text, g_populated_cache[g_SelectedIndex], sizeof(text) - 1);
            text[sizeof(text) - 1] = '\0';
            
            if (g_enter_callback != NULL) {
                g_enter_callback(text);
            }
            selection_complete = true;
            return;
        }
    }
    
    if (hEditControl && IsWindow(hEditControl)) {
        UI_SetText(hEditControl,"");
        printf("[VIM] Deleted Entire Search Box \n");
    }
}

static inline void HandleDeleteLineInsert(void) {
    if (hEditControl && IsWindow(hEditControl)) {
        UI_SetText(hEditControl,"");
        SetMode(MODE_INSERT);
        printf("[VIM] Deleted Entire Search Box & Switched to Insert\n");
    }
}

static inline void HandleDeleteLineNormal(void) {
    if (hEditControl && IsWindow(hEditControl)) {
        UI_SetText(hEditControl,"");
        SetMode(MODE_NORMAL); 
        printf("[VIM] Deleted Entire Search Box (Normal Mode Kept)\n");
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
    int selected_index = g_SelectedIndex;
    char buffer[256];
    if (selected_index >= 0 && selected_index < g_populated_count) {
        char text[512] = {0};
        
        strncpy(text, g_populated_cache[selected_index], sizeof(text) - 1);
        text[sizeof(text) - 1] = '\0';
        
        CopyToClipboard(text);
        return;
    } else {
        printf("[VIM] Yank failed: No valid list item selected.\n");
    }
}

static inline void HandleYankLine(void) {
    if (hEditControl && IsWindow(hEditControl)) {
        char textBuffer[256] = {0};
        char buffer[256];
        UI_GetCurrentText(hEditControl,textBuffer);
        
        textBuffer[strcspn(textBuffer, "\r\n")] = '\0';
        
        if (strlen(textBuffer) > 0) {
            CopyToClipboard(textBuffer);
        } else {
            printf("[VIM] Yank failed: Search box is currently empty.\n");
        }
    }
}

static inline void HandleInsertRegularEnter(void) {
  SendMessageA(hEditControl, WM_KEYDOWN, VK_RETURN, 0);
  SendMessageA(hEditControl, WM_KEYUP, VK_RETURN, 0);
}

static inline void HandleInsertControlEnter(void) {
  SendMessageA(hEditControl, WM_KEYDOWN, VK_CONTROL, 0);
  SendMessageA(hEditControl, WM_KEYDOWN, VK_RETURN, 0);
  SendMessageA(hEditControl, WM_KEYUP, VK_RETURN, 0);
  SendMessageA(hEditControl, WM_KEYUP, VK_CONTROL, 0);
  selection_complete = false;
}

static inline void HandleMoveWordForward(void) {
if (!UI_IsListBoxSelected()) {
    char text[512];
      
    UI_GetCurrentText(hEditControl,text);
    
    DWORD start = 0;
    DWORD end = 0;

    UI_GetCurrentCursorPosition(hEditControl, &start, &end);
    
    int pos = (int)start;
    int len = (int)strlen(text);

    if (len < (int)sizeof(text) - 1) {
        text[len] = ' ';
        text[len + 1] = '\0';
        len++; 
    }
    
    if (pos < len) {
      for (int i = pos + 1; i < len; i++) {
        if (text[i] != ' ') {
          continue;
        } else {
          for (int j = i; j < len; j++) {
            if (text[j] != ' ') {
              UI_SetCursorPosition(hEditControl,j);
              break;
            } else {
              continue;
            }
          }
          break;
        }
      }
    }
  }
}

static inline void HandleMoveWordBackward(void) {
  if (!UI_IsListBoxSelected()) {
    char text[512];
    UI_GetCurrentText(hEditControl, text);
    
    DWORD start = 0;
    DWORD end = 0;
    UI_GetCurrentCursorPosition(hEditControl, &start, &end);
    
    int pos = (int)start;

    for (int i = pos - 1; i >= -1; i--) {
      
      char current_char = (i >= 0) ? text[i] : ' ';

      if (current_char != ' ') {
        continue;
      } else {
        for (int j = i - 1; j >= -1; j--) {
          char next_char = (j >= 0) ? text[j] : ' ';

          if (next_char != ' ') {
            UI_SetCursorPosition(hEditControl, j + 1);
            return; 
          } else {
            continue;
          }
        }
        
        UI_SetCursorPosition(hEditControl, 0);
        return;
      }
    }
  }
}

#endif
