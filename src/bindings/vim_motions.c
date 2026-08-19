#include "common.h"
#include "bindings/vim_motions.h"
#include "bindings/vim_helpers.h"
#include "core/common_ui.h"
#include "helpers.h"
#include <ctype.h>

void HandleEnterNormal(void) { 
    SetMode(MODE_NORMAL); 
    printf("[VIM] Entered NORMAL mode\n");
}

void HandleEnterInsertOut(void) { 
    if (!g_ActiveEditControl || !IsWindow(g_ActiveEditControl)) return;
    
    DWORD start = 0, end = 0;
    UI_GetCurrentCursorPosition(&start, &end);
    
    int pos = (int)start + 1;
    
    UI_SetCursorPosition(g_ActiveEditControl,pos);
    
    SetMode(MODE_INSERT); 
    printf("[VIM] Entered INSERT mode\n");
}

void HandleEnterInsertIn(void) { 
    if (!g_ActiveEditControl || !IsWindow(g_ActiveEditControl)) return;
    
    DWORD start = 0, end = 0;
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
    if (hListBox != NULL && UI_IsListBoxSelected()) {
        UI_Edit_KeyDown(g_ActiveEditControl, VK_RETURN);
        UI_Edit_KeyUp(g_ActiveEditControl, VK_RETURN);
    } else {
      UI_Edit_KeyDown(g_ActiveEditControl, VK_RIGHT);
      UI_Edit_KeyUp(g_ActiveEditControl, VK_RIGHT);
    }

}
void HandleMoveUp(void) {
    UI_NavigateUp();
}
void HandleMoveRight(void) {
    if (g_ActiveEditControl) {
        UI_Edit_KeyDown(g_ActiveEditControl, VK_RIGHT);
        UI_Edit_KeyUp(g_ActiveEditControl, VK_RIGHT);
    }
}

void HandleMoveDown(void) {
    UI_NavigateDown();
}

void HandlePersistentDelete(void) {
    if (!UI_IsListBoxSelected() || !g_ActiveEditControl) return;
    if (g_ctrl_enter_callback != NULL) {
        g_ctrl_enter_callback(g_populated_cache[g_SelectedIndex]);
    }
}

void HandleDeleteCharInsert(void) {
    DeleteCharUnderCursor();
    HandleEnterInsertIn(); 
}

void HandleDeleteCharNormal(void) {
    if (g_ActiveEditControl) {
        DeleteCharUnderCursor();
        printf("[VIM] Deleted character under cursor\n");
    }
}

void HandleDeleteLine(void) {
    if (g_ActiveEditControl) {
        UI_SetText(g_ActiveEditControl,"");
        printf("[VIM] Deleted Entire Search Box \n");
    }
}

void HandleDeleteLineInsert(void) {
    if (g_ActiveEditControl) {
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
  if (g_hEditPopup) {
    UI_Edit_KeyDown(g_ActiveEditControl, VK_ESCAPE);
    UI_Edit_KeyUp(g_ActiveEditControl, VK_ESCAPE);
  } else {
    selection_complete = true;
  }
}

void HandleYankLine(void) {
    if (g_ActiveEditControl) {
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

void HandleMoveWordForward(void) {
    char text[256];
      
    UI_GetCurrentText(g_ActiveEditControl,text);
    
    DWORD start = 0;
    DWORD end = 0;

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
    
    DWORD start = 0;
    DWORD end = 0;
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
  if (UI_IsListBoxSelected()) {
    char* text = g_populated_cache[g_SelectedIndex];
    UI_SetText(g_ActiveEditControl,text);
    UI_SetCursorPosition(g_ActiveEditControl,strlen(text));
  }
}
void HandleToggleHistory(void) {
  if (UI_IsListBoxSelected()) {
    UI_SetSelection(hListBox, -1, g_SelectedIndex);
  }
  GetFromHistory();
}

void HandleShowFullText(void) {
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

void HandleSearchTextForward(void ) {
  if (!UI_IsListBoxSelected()) {
    Platform_WaitForKeypressAsync(SearchTextForward);
  }
}

void HandleSearchTextBackward(void) {
  if (!UI_IsListBoxSelected()) {
    Platform_WaitForKeypressAsync(SearchTextBackward);
  }
}
void HandleDeleteTillEnd(void) {
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
void HandlePageUp(void) {
  int current_sel = g_SelectedIndex;
  int wanted_sel = current_sel - PageUpDownLen;
  if (wanted_sel <= 0) wanted_sel = 0;
  UI_SetSelection(hListBox,wanted_sel,g_SelectedIndex);
  UI_LB_SetTopIndex(hListBox,wanted_sel);
}
void HandlePageDown(void) {
  int current_sel = g_SelectedIndex;
  int wanted_sel = current_sel + PageUpDownLen;
  if (wanted_sel >= g_TotalMatches) wanted_sel = g_TotalMatches;
  UI_SetSelection(hListBox,wanted_sel - 1,g_SelectedIndex);
  UI_LB_SetTopIndex(hListBox,current_sel);
}
void HandleNormalEdit(void) {
  UI_SetSelection(hListBox,-1,g_SelectedIndex);
}
