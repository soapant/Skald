#include "common.h"
#include "bindings/vim_motions.h"
#include "bindings/vim_helpers.h"
#include "core/common_ui.h"
#include "helpers.h"
#include <ctype.h>
#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(_WIN32)
static ListBoxInfo* GetActiveListBoxForMotions(void) {
    if (WH_TO_HWND(hPopupWnd) && IsWindow(WH_TO_HWND(hPopupWnd))) {
        return (ListBoxInfo*)GetWindowLongPtrA(WH_TO_HWND(hPopupWnd), GWLP_USERDATA);
    }
    return NULL;
}
#else
extern PromptState* g_currentPromptState;
static ListBoxInfo* GetActiveListBoxForMotions(void) {
    if (g_currentPromptState && g_currentPromptState->has_listbox) return &g_currentPromptState->listbox;
    return NULL;
}
#endif

void HandleEnterNormal(void) { 
    SetMode(MODE_NORMAL); 
    printf("[VIM] Entered NORMAL mode\n");
}

void HandleEnterInsertOut(void) { 
    if (!IsWindowValid(g_ActiveEditControl)) return;
    
    uint32_t start = 0, end = 0;
    UI_GetCurrentCursorPosition(&start, &end);
    
    int pos = (int)start + 1;
    
    UI_SetCursorPosition(g_ActiveEditControl,pos);
    
    SetMode(MODE_INSERT); 
    printf("[VIM] Entered INSERT mode\n");
}

void HandleEnterInsertIn(void) { 
    if (!IsWindowValid(g_ActiveEditControl)) return;
    
    uint32_t start = 0, end = 0;
    UI_GetCurrentCursorPosition(&start, &end);
    
    int pos = (int)start;
    
    UI_SetCursorPosition(g_ActiveEditControl,pos);
    
    SetMode(MODE_INSERT); 
    
    printf("[VIM] Entered INSERT mode\n");
}

void HandleEnterVisual(void) { 
  
    SetMode(MODE_VISUAL); 
    printf("[VIM] Entered VISUAL mode\n");
}

void HandleMoveLeft(void) {
      UI_Edit_KeyDown(g_ActiveEditControl, VK_LEFT);
      UI_Edit_KeyUp(g_ActiveEditControl, VK_LEFT);
}

void HandleEnter(void) {
    ListBoxInfo* lb = GetActiveListBoxForMotions();
    if (WH_TO_HWND(hListBox) != NULL && UI_IsListBoxSelected(lb)) {
        UI_Edit_KeyDown(g_ActiveEditControl, VK_RETURN);
        UI_Edit_KeyUp(g_ActiveEditControl, VK_RETURN);
    } else {
      UI_Edit_KeyDown(g_ActiveEditControl, VK_RIGHT);
      UI_Edit_KeyUp(g_ActiveEditControl, VK_RIGHT);
    }

}
void HandleMoveUp(void) {
    ListBoxInfo* lb = GetActiveListBoxForMotions();
    UI_NavigateUp(lb, hListBox);
}
void HandleMoveRight(void) {
    if (WH_TO_HWND(g_ActiveEditControl) != NULL) {
        UI_Edit_KeyDown(g_ActiveEditControl, VK_RIGHT);
        UI_Edit_KeyUp(g_ActiveEditControl, VK_RIGHT);
    }
}

void HandleMoveDown(void) {
    ListBoxInfo* lb = GetActiveListBoxForMotions();
    UI_NavigateDown(lb, hListBox);
}

void HandlePersistentDelete(void) {
    ListBoxInfo* lb = GetActiveListBoxForMotions();
    if (!UI_IsListBoxSelected(lb) || WH_TO_HWND(g_ActiveEditControl) == NULL) return;
    if (g_ctrl_enter_callback != NULL && lb) {
        const char* txt = lb->populated_cache[lb->selected_index];
        g_ctrl_enter_callback(txt, lb);
    }
}

void HandleDeleteCharInsert(void) {
    DeleteCharUnderCursor();
    HandleEnterInsertIn(); 
}

void HandleDeleteCharNormal(void) {
    if (WH_TO_HWND(g_ActiveEditControl) != NULL) {
        DeleteCharUnderCursor();
        printf("[VIM] Deleted character under cursor\n");
    }
}

void HandleDeleteLine(void) {
    if (WH_TO_HWND(g_ActiveEditControl) != NULL) {
        UI_SetText(g_ActiveEditControl,"");
        printf("[VIM] Deleted Entire Search Box \n");
    }
}

void HandleDeleteLineInsert(void) {
    if (WH_TO_HWND(g_ActiveEditControl) != NULL) {
        UI_SetText(g_ActiveEditControl,"");
        SetMode(MODE_INSERT);
        printf("[VIM] Deleted Entire Search Box & Switched to Insert\n");
    }
}

void HandleMoveListboxStart(void) {
    GoToFirstListBoxItem();
}
void HandleMoveListboxEnd(void) {
    GoToLastListBoxItem();
}
void HandleMoveLineEnd(void) {
  SetCursorToEndOfText();
}
void HandleMoveLineStart(void) {
  SetCursorToStartOfText();
}

void HandleEnterInsertInStart(void) {
  SetCursorToStartOfText();
  HandleEnterInsertIn();
}

void HandleEnterInsertOutEnd(void) {
  SetCursorToEndOfText();
  HandleEnterInsertOut();
}
void HandleExitWindow(void) {
  if (WH_TO_HWND(g_hEditPopup) != NULL) {
    UI_Edit_KeyDown(g_ActiveEditControl, VK_ESCAPE);
    UI_Edit_KeyUp(g_ActiveEditControl, VK_ESCAPE);
  } else {
    selection_complete = true;
  }
}

void HandleYankLine(void) {
    if (WH_TO_HWND(g_ActiveEditControl) != NULL) {
        char textBuffer[256] = {0};
        char buffer[256];
        UI_GetCurrentText(g_ActiveEditControl,textBuffer);
        
        textBuffer[strcspn(textBuffer, "\r\n")] = '\0';
        
        if (strlen(textBuffer) > 0) {
            Platform_CopyToClipboard(textBuffer);
        } else {
            printf("[VIM] Yank failed: Search box is currently empty.\n");
        }
    }
}

void HandleInsertControlEnter(void) {
  ListBoxInfo* lb = GetActiveListBoxForMotions();
  char text[256];
  if (UI_IsListBoxSelected(lb) && lb) {
    snprintf(text,sizeof(text),"%s",lb->populated_cache[lb->selected_index]);
  } else {
    UI_GetCurrentText(g_ActiveEditControl,text);
  }
  printf("TRYING TO DO CTRL ENTER WITH TEXT %s \n",text);
  if (g_ctrl_enter_callback != NULL) {
    g_ctrl_enter_callback(text, lb);
  }
  selection_complete = false;
}

void HandleMoveWordForward(void) {
    char text[256];
      
    UI_GetCurrentText(g_ActiveEditControl,text);
    
    uint32_t start = 0;
    uint32_t end = 0;

    UI_GetCurrentCursorPosition(&start, &end);
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

void HandleMoveWordBackward(void) {
    char text[512];
    UI_GetCurrentText(g_ActiveEditControl, text);
    
    uint32_t start = 0;
    uint32_t end = 0;
    UI_GetCurrentCursorPosition(&start, &end);
    
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
void HandleDeleteSingleQuotesInsert(void) {
    DeleteInsideDelimiter('\'', '\'', true);
}
void HandleDeleteSingleQuotesNormal(void) {
    DeleteInsideDelimiter('\'', '\'', false);
}

void HandleDeleteQuotesInsert(void) {
    DeleteInsideDelimiter('\"', '\"', true);
}
void HandleDeleteQuotesNormal(void) {
    DeleteInsideDelimiter('\"', '\"', false);
}

void HandleDeleteBracketsInsert(void) {
    DeleteInsideDelimiter('(', ')', true);
}
void HandleDeleteBracketsNormal(void) {
    DeleteInsideDelimiter('(', ')', false);
}

void HandleDeleteCurlyBracketsInsert(void) {
    DeleteInsideDelimiter('{', '}', true);
}
void HandleDeleteCurlyBracketsNormal(void) {
    DeleteInsideDelimiter('{', '}', false);
}

void HandleDeleteSquareBracketsInsert(void) {
    DeleteInsideDelimiter('[', ']', true);
}
void HandleDeleteSquareBracketsNormal(void) {
    DeleteInsideDelimiter('[', ']', false);
}
void HandleDeleteWordInsert(void) {
    DeleteInnerWord(true);   
}

void HandleDeleteWordNormal(void) {
    DeleteInnerWord(false); 
}
void HandleSendEdit(void) {
    UI_Edit_KeyDown(g_ActiveEditControl,EDIT_TEXT_KEY);
    UI_Edit_KeyUp(g_ActiveEditControl, EDIT_TEXT_KEY);
}

void HandleSendCopyShift(void) {
    UI_Edit_KeyUp(g_ActiveEditControl,VK_SHIFT);
    UI_Edit_KeyDown(g_ActiveEditControl,COPY_SELECTION_KEY);
    UI_Edit_KeyUp(g_ActiveEditControl, COPY_SELECTION_KEY);
    UI_Edit_KeyDown(g_ActiveEditControl,VK_SHIFT);
}
void HandleSendCopyControl(void) {
    UI_Edit_KeyUp(g_ActiveEditControl,VK_CONTROL);
    UI_Edit_KeyDown(g_ActiveEditControl,COPY_SELECTION_KEY);
    UI_Edit_KeyUp(g_ActiveEditControl, COPY_SELECTION_KEY);
    UI_Edit_KeyDown(g_ActiveEditControl,VK_CONTROL);
}
void HandleCopyToSearch(void) {
  ListBoxInfo* lb = GetActiveListBoxForMotions();
  if (UI_IsListBoxSelected(lb) && lb) {
    char* text = lb->populated_cache[lb->selected_index];
    UI_SetText(g_ActiveEditControl,text);
    UI_SetCursorPosition(g_ActiveEditControl,strlen(text));
  }
}
void HandleToggleHistory(void) {
  ListBoxInfo* lb = GetActiveListBoxForMotions();
  if (UI_IsListBoxSelected(lb) && lb) {
    UI_SetSelection(lb, hListBox, -1);
  }
  GetFromHistory(lb);
}

void HandleShowFullText(void) {
  if (FullTextNeedsKeyPress) {
      ListBoxInfo* lb = GetActiveListBoxForMotions();
      if (UI_IsListBoxSelected(lb) && lb) {
        SubjectiveMessageDisplay(lb->populated_cache[lb->selected_index], 140, 6969, COLOR_OF_SELECTION_LB(lb));
      }
      
  }
}

void SearchTextForward(int key) {
  char text[512];
  UI_GetCurrentText(g_ActiveEditControl, text);
  uint32_t start_pos, end_pos;
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
  uint32_t start_pos, end_pos;
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

void HandleSearchTextForward(void ) {
  ListBoxInfo* lb = GetActiveListBoxForMotions();
  if (!UI_IsListBoxSelected(lb)) {
    Platform_WaitForKeypressAsync(SearchTextForward);
  }
}

void HandleSearchTextBackward(void) {
  ListBoxInfo* lb = GetActiveListBoxForMotions();
  if (!UI_IsListBoxSelected(lb)) {
    Platform_WaitForKeypressAsync(SearchTextBackward);
  }
}
void HandleDeleteTillEnd(void) {
  ListBoxInfo* lb = GetActiveListBoxForMotions();
  if (!UI_IsListBoxSelected(lb)) {
      char text[512];
      UI_GetCurrentText(g_ActiveEditControl, text);
      uint32_t start_pos, end_pos;
      UI_Edit_GetSel(g_ActiveEditControl, &start_pos, &end_pos);
      int cursor = (int)start_pos;
      for (int i = cursor; i < strlen(text); ++i){
          text[i] = '\0';
      }
      UI_SetText(g_ActiveEditControl,text);
  }
}
void HandlePageUp(void) {
  ListBoxInfo* lb = GetActiveListBoxForMotions();
  if (!lb) return;
  int current_sel = lb->selected_index;
  int wanted_sel = current_sel - PageUpDownLen;
  if (wanted_sel <= 0) wanted_sel = 0;
  UI_SetSelection(lb, hListBox, wanted_sel);
  UI_LB_SetTopIndex(hListBox,wanted_sel);
}
void HandlePageDown(void) {
  ListBoxInfo* lb = GetActiveListBoxForMotions();
  if (!lb) return;
  int current_sel = lb->selected_index;
  int wanted_sel = current_sel + PageUpDownLen;
  if (wanted_sel >= lb->total_matches) wanted_sel = lb->total_matches;
  UI_SetSelection(lb, hListBox, wanted_sel - 1);
  UI_LB_SetTopIndex(hListBox,current_sel);
}
void HandleNormalEdit(void) {
  ListBoxInfo* lb = GetActiveListBoxForMotions();
  if (!lb) return;
  UI_SetSelection(lb, hListBox, -1);
}
