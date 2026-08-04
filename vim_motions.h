// vim_motions.h
#ifndef VIM_MOTIONS_H
#define VIM_MOTIONS_H

#include <windows.h>
#include <stdio.h>
#include <winsock.h>
#include <wow64apiset.h>
#include "types.h"
#include "helpers.h"
#include "config.h"
#include "modal.h"
#include "character_sequence.h"
#include "vim_helpers.h"
#include "calc.h"
#include "common_ui.h"
#include "config.h"

typedef enum {
    MOTION_NONE = 0,
    MOTION_ENTER_NORMAL,
    MOTION_DELETE_TILL_END,
    MOTION_PAGE_UP,
    MOTION_PAGE_DOWN,
    MOTION_HISTORY_TOGGLE,
    MOTION_SEARCH_TEXT_FORWARD,
    MOTION_SEARCH_TEXT_BACKWARD,
    MOTION_SHOW_FULL_TEXT,
    MOTION_ENTER,
    MOTION_COPY_INTO_SEARCH,
    MOTION_SEND_COPY_SHIFT,
    MOTION_SEND_COPY_CONTROL,
    MOTION_SEND_EDIT,
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
    MOTION_UNDO,
    MOTION_SEARCH_FORWARD,
    MOTION_SEARCH_BACKWARD,
    MOTION_EXIT_WINDOW,
    MOTION_DELETE_QUOTES_INSERT,
    MOTION_DELETE_QUOTES_NORMAL,
    MOTION_DELETE_SINGLE_QUOTES_INSERT,
    MOTION_DELETE_SINGLE_QUOTES_NORMAL,
    MOTION_DELETE_BRACKETS_INSERT,
    MOTION_DELETE_BRACKETS_NORMAL,
    MOTION_DELETE_CURLY_BRACKETS_INSERT,
    MOTION_DELETE_CURLY_BRACKETS_NORMAL,
    MOTION_DELETE_SQUARE_BRACKETS_INSERT,
    MOTION_DELETE_SQUARE_BRACKETS_NORMAL,
    MOTION_DELETE_WORD_NORMAL,
    MOTION_DELETE_WORD_INSERT,
} MotionAction;

typedef enum {
    VIM_SCOPE_ALL = 0,
    VIM_SCOPE_MODAL,
    VIM_SCOPE_POPUP,
} VimScope;

static inline bool ScopeMatches(VimScope scope, bool in_popup) {
    switch (scope) {
        case VIM_SCOPE_ALL:   return true;
        case VIM_SCOPE_MODAL: return !in_popup;
        case VIM_SCOPE_POPUP: return in_popup;
        default:              return false;
    }
}

typedef struct {
    int vk_code;
    int modifiers;
    MotionAction action_id;
    void (*handler)(void);
    EditorMode mode;
    VimScope scope;
} MotionBinding;

typedef struct {
    const char* sequence;
    MotionAction action_id;
    void (*handler)(void);
    bool is_prefix;
    EditorMode mode;
    VimScope scope;
} SequenceBinding;



static inline void HandleEnterNormal(void) { 
    SetMode(MODE_NORMAL); 
    printf("[VIM] Entered NORMAL mode\n");
}

static inline void HandleEnterInsertOut(void) { 
    if (!g_ActiveEditControl || !IsWindow(g_ActiveEditControl)) return;
    
    DWORD start = 0, end = 0;
    UI_GetCurrentCursorPosition(g_ActiveEditControl, &start, &end);
    
    int pos = (int)start + 1;
    
    UI_SetCursorPosition(g_ActiveEditControl,pos);
    
    SetMode(MODE_INSERT); 
    printf("[VIM] Entered INSERT mode\n");
}

static inline void HandleEnterInsertIn(void) { 
    if (!g_ActiveEditControl || !IsWindow(g_ActiveEditControl)) return;
    
    DWORD start = 0, end = 0;
    UI_GetCurrentCursorPosition(g_ActiveEditControl, &start, &end);
    
    int pos = (int)start;
    
    UI_SetCursorPosition(g_ActiveEditControl,pos);
    
    SetMode(MODE_INSERT); 
    
    printf("[VIM] Entered INSERT mode\n");
}

static inline void HandleEnterVisual(void) { 
  
    SetMode(MODE_VISUAL); 
    printf("[VIM] Entered VISUAL mode\n");
}


static inline void HandleMoveLeft(void) {
      UI_Edit_KeyDown(g_ActiveEditControl, VK_LEFT);
      UI_Edit_KeyUp(g_ActiveEditControl, VK_LEFT);
}

static inline void HandleEnter(void) {
    if (hListBox != NULL && UI_IsListBoxSelected()) {
        UI_Edit_KeyDown(g_ActiveEditControl, VK_RETURN);
        UI_Edit_KeyUp(g_ActiveEditControl, VK_RETURN);
    } else {
      UI_Edit_KeyDown(g_ActiveEditControl, VK_RIGHT);
      UI_Edit_KeyUp(g_ActiveEditControl, VK_RIGHT);
    }

}
static inline void HandleMoveUp(void) {
    if (g_ActiveEditControl) {
        UI_Edit_KeyDown(g_ActiveEditControl, VK_UP);
        UI_Edit_KeyUp(g_ActiveEditControl, VK_UP);
    }
}
static inline void HandleMoveRight(void) {
    if (g_ActiveEditControl) {
        UI_Edit_KeyDown(g_ActiveEditControl, VK_RIGHT);
        UI_Edit_KeyUp(g_ActiveEditControl, VK_RIGHT);
    }
}

static inline void HandleMoveDown(void) {
    if (g_ActiveEditControl) {
        UI_Edit_KeyDown(g_ActiveEditControl, VK_DOWN);
        UI_Edit_KeyUp(g_ActiveEditControl, VK_DOWN);
    }
}

static inline void HandlePersistentDelete(void) {
    char text[512] = {0};
    
    strncpy(text, g_populated_cache[g_SelectedIndex], sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';
    int index = g_SelectedIndex;
    
    if (g_ctrl_enter_callback != NULL) {
        g_ctrl_enter_callback(text);
    }
    UI_SetSelection(hListBox,index,index + 1);
    UI_Edit_KeyDown(g_ActiveEditControl, VK_DOWN);
    UI_Edit_KeyUp(g_ActiveEditControl, VK_DOWN);
    UI_Edit_KeyDown(g_ActiveEditControl, VK_UP);
    UI_Edit_KeyUp(g_ActiveEditControl, VK_UP);
    return;
    
}

static inline void HandleDelete(void) {
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
    DeleteCharUnderCursor();
    HandleEnterInsertIn(); 
}

static inline void HandleDeleteCharNormal(void) {
    if (g_ActiveEditControl) {
        DeleteCharUnderCursor();
        printf("[VIM] Deleted character under cursor\n");
    }
}

static inline void HandleDeleteLine(void) {
    if (g_ActiveEditControl) {
        UI_SetText(g_ActiveEditControl,"");
        printf("[VIM] Deleted Entire Search Box \n");
    }
}

static inline void HandleDeleteLineInsert(void) {
    if (g_ActiveEditControl) {
        UI_SetText(g_ActiveEditControl,"");
        SetMode(MODE_INSERT);
        printf("[VIM] Deleted Entire Search Box & Switched to Insert\n");
    }
}

static inline void HandleDeleteLineNormal(void) {
    if (g_ActiveEditControl) {
        UI_SetText(g_ActiveEditControl,"");
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
  if (g_hEditPopup) {
    UI_Edit_KeyDown(g_ActiveEditControl, VK_ESCAPE);
    UI_Edit_KeyUp(g_ActiveEditControl, VK_ESCAPE);
  } else {
    selection_complete = true;
  }
}


static inline void HandleYankListBox(void) {
    int selected_index = g_SelectedIndex;
    char buffer[256];
    if (selected_index >= 0 && selected_index < g_populated_count) {
        char text[512] = {0};
        
        strncpy(text, g_populated_cache[selected_index], sizeof(text) - 1);
        text[sizeof(text) - 1] = '\0';
        
        if (g_copy_callback != NULL) {
          g_copy_callback(text, UI_LookupTrueIndex(text), 0);
        } else {
          CopyToClipboard(text);
        }
        return;
    } else {
        printf("[VIM] Yank failed: No valid list item selected.\n");
    }
}
static inline void HandleYankListBoxExtra(void) {
    int selected_index = g_SelectedIndex;
    char buffer[256];
    if (selected_index >= 0 && selected_index < g_populated_count) {
        char text[512] = {0};
        
        strncpy(text, g_populated_cache[selected_index], sizeof(text) - 1);
        text[sizeof(text) - 1] = '\0';
        
        if (g_copy_callback != NULL) {
          g_copy_callback(text, UI_LookupTrueIndex(text), 1);
        } else {
          CopyToClipboard(text);
        }
        return;
    } else {
        printf("[VIM] Yank failed: No valid list item selected.\n");
    }
}

static inline void HandleYankLine(void) {
    if (g_ActiveEditControl) {
        char textBuffer[256] = {0};
        char buffer[256];
        UI_GetCurrentText(g_ActiveEditControl,textBuffer);
        
        textBuffer[strcspn(textBuffer, "\r\n")] = '\0';
        
        if (strlen(textBuffer) > 0) {
            CopyToClipboard(textBuffer);
        } else {
            printf("[VIM] Yank failed: Search box is currently empty.\n");
        }
    }
}

static inline void HandleInsertRegularEnter(void) {
  UI_Edit_KeyDown(g_ActiveEditControl, VK_RETURN);
  UI_Edit_KeyUp(g_ActiveEditControl, VK_RETURN);
}

static inline void HandleInsertControlEnter(void) {
  char text[256];
  if (UI_IsListBoxSelected()) {
    snprintf(text,sizeof(text),"%s",g_populated_cache[g_SelectedIndex]);
  } else {
    UI_GetCurrentText(g_ActiveEditControl,text);
  }
  printf("TRYING TO DO CTRL ENTER WITH TEXT %s \n",text);
  if (g_ctrl_enter_callback != NULL) {
    g_ctrl_enter_callback(text);
  }
  selection_complete = false;
}

static inline void HandleMoveWordForward(void) {
    char text[256];
      
    UI_GetCurrentText(g_ActiveEditControl,text);
    
    DWORD start = 0;
    DWORD end = 0;

    UI_GetCurrentCursorPosition(g_ActiveEditControl, &start, &end);
    int len = strlen(text);

    for (int i = start + 1;i <= len;i++) {
        if (i == len) {
          UI_SetCursorPosition(g_ActiveEditControl,len - 1);
          break;
        }
        if (text[i] == ' ') {
          UI_SetCursorPosition(g_ActiveEditControl,i + 1);
          break;
        }
    }
}

static inline void HandleMoveWordBackward(void) {
    char text[512];
    UI_GetCurrentText(g_ActiveEditControl, text);
    
    DWORD start = 0;
    DWORD end = 0;
    UI_GetCurrentCursorPosition(g_ActiveEditControl, &start, &end);
    
    int pos = (int)start;

    for (int i = pos - 1; i >= -1; i--) {
      char current_char = (i >= 0) ? text[i] : ' ';
      if (current_char != ' ') {
        continue;
      } else {
        for (int j = i - 1; j >= -1; j--) {
          char next_char = (j >= 0) ? text[j] : ' ';

          if (next_char != ' ') {
            UI_SetCursorPosition(g_ActiveEditControl, j + 1);
            return; 
          } else {
            continue;
          }
        }
        
        UI_SetCursorPosition(g_ActiveEditControl, 0);
        return;
      }
    }
}
static inline void HandleDeleteSingleQuotesInsert(void) {
    DeleteInsideDelimiter('\'', '\'', true);
}
static inline void HandleDeleteSingleQuotesNormal(void) {
    DeleteInsideDelimiter('\'', '\'', false);
}

static inline void HandleDeleteQuotesInsert(void) {
    DeleteInsideDelimiter('\"', '\"', true);
}
static inline void HandleDeleteQuotesNormal(void) {
    DeleteInsideDelimiter('\"', '\"', false);
}

static inline void HandleDeleteBracketsInsert(void) {
    DeleteInsideDelimiter('(', ')', true);
}
static inline void HandleDeleteBracketsNormal(void) {
    DeleteInsideDelimiter('(', ')', false);
}

static inline void HandleDeleteCurlyBracketsInsert(void) {
    DeleteInsideDelimiter('{', '}', true);
}
static inline void HandleDeleteCurlyBracketsNormal(void) {
    DeleteInsideDelimiter('{', '}', false);
}

static inline void HandleDeleteSquareBracketsInsert(void) {
    DeleteInsideDelimiter('[', ']', true);
}
static inline void HandleDeleteSquareBracketsNormal(void) {
    DeleteInsideDelimiter('[', ']', false);
}
static inline void HandleDeleteWordInsert(void) {
    DeleteInnerWord(true);   
}

static inline void HandleDeleteWordNormal(void) {
    DeleteInnerWord(false); 
}
static inline void HandleSendEdit(void) {
    UI_Edit_KeyDown(g_ActiveEditControl,EDIT_TEXT_KEY);
    UI_Edit_KeyUp(g_ActiveEditControl, EDIT_TEXT_KEY);
}

static inline void HandleSendCopyShift(void) {
    UI_Edit_KeyUp(g_ActiveEditControl,VK_SHIFT);
    UI_Edit_KeyDown(g_ActiveEditControl,COPY_SELECTION_KEY);
    UI_Edit_KeyUp(g_ActiveEditControl, COPY_SELECTION_KEY);
    UI_Edit_KeyDown(g_ActiveEditControl,VK_SHIFT);
}
static inline void HandleSendCopyControl(void) {
    UI_Edit_KeyUp(g_ActiveEditControl,VK_CONTROL);
    UI_Edit_KeyDown(g_ActiveEditControl,COPY_SELECTION_KEY);
    UI_Edit_KeyUp(g_ActiveEditControl, COPY_SELECTION_KEY);
    UI_Edit_KeyDown(g_ActiveEditControl,VK_CONTROL);
}
static inline void HandleCopyToSearch(void) {
  if (UI_IsListBoxSelected()) {
    char* text = g_populated_cache[g_SelectedIndex];
    UI_SetText(g_ActiveEditControl,text);
    UI_SetCursorPosition(g_ActiveEditControl,strlen(text));
  }
}
static inline void HandleToggleHistory(void) {
  if (UI_IsListBoxSelected()) {
    UI_SetSelection(hListBox, -1, g_SelectedIndex);
  }
  UI_Edit_KeyDown(g_ActiveEditControl, VK_UP);
  UI_Edit_KeyUp(g_ActiveEditControl, VK_UP);
}

static inline void HandleShowFullText(void) {
  if (FullTextNeedsKeyPress) {
      if (UI_IsListBoxSelected()) {
        SubjectiveMessageDisplay(g_populated_cache[g_SelectedIndex], 140, 6969,COLOR_OF_SELECTION);
      }
      
  }
}

void SearchTextForward(int key) {
  char text[512];
  UI_GetCurrentText(g_ActiveEditControl, text);
  DWORD start_pos, end_pos;
  UI_Edit_GetSel(g_ActiveEditControl, &start_pos, &end_pos);
  int cursor = (int)start_pos;

  for (int i = 0; i < strlen(text);i++) {
    if (tolower(text[i]) == tolower((char)key)) {
      if (i <= start_pos ) continue;
      UI_SetCursorPosition(g_ActiveEditControl,i);
      return;
    }
  }
}
void SearchTextBackward(int key) {
  char text[512];
  UI_GetCurrentText(g_ActiveEditControl, text);
  DWORD start_pos, end_pos;
  UI_Edit_GetSel(g_ActiveEditControl, &start_pos, &end_pos);
  int cursor = (int)start_pos;

  for (int i = 0; i < strlen(text);i++) {
    if (tolower(text[i]) == tolower((char)key)) {
      if (i >= start_pos ) continue;
      UI_SetCursorPosition(g_ActiveEditControl,i);
      return;
    }
  }
}

static inline void HandleSearchTextForward(void ) {
  if (!UI_IsListBoxSelected()) {
    WaitForKeypressAsync(SearchTextForward);
  }
}

static inline void HandleSearchTextBackward(void) {
  if (!UI_IsListBoxSelected()) {
    WaitForKeypressAsync(SearchTextBackward);
  }
}
static inline void HandleDeleteTillEnd(void) {
  if (!UI_IsListBoxSelected()) {
      char text[512];
      UI_GetCurrentText(g_ActiveEditControl, text);
      DWORD start_pos, end_pos;
      UI_Edit_GetSel(g_ActiveEditControl, &start_pos, &end_pos);
      int cursor = (int)start_pos;
      for (int i = cursor; i < strlen(text); ++i){
          text[i] = '\0';
      }
      UI_SetText(g_ActiveEditControl,text);
  }
}
static inline void HandlePageUp(void) {
  int current_sel = g_SelectedIndex;
  int wanted_sel = current_sel - PageUpDownLen;
  if (wanted_sel <= 0) wanted_sel = 0;
  UI_SetSelection(hListBox,wanted_sel,g_SelectedIndex);
  UI_LB_SetTopIndex(hListBox,wanted_sel);
}
static inline void HandlePageDown(void) {
  int current_sel = g_SelectedIndex;
  int wanted_sel = current_sel + PageUpDownLen;
  if (wanted_sel >= g_TotalMatches) wanted_sel = g_TotalMatches;
  UI_SetSelection(hListBox,wanted_sel - 1,g_SelectedIndex);
  UI_LB_SetTopIndex(hListBox,current_sel);
}

#endif
