#include "common.h"
#include "bindings/vim_motions.h"
#include "bindings/vim_helpers.h"
#include "core/common_ui.h"
#include "core/vim_context.h"
#include "helpers.h"
#include <ctype.h>
#if !defined(_WIN32)
#include <sys/time.h>
#endif
#if defined(_WIN32)
#include <windows.h>
#endif

void HandleEnterNormal(VimContext* ctx) { 
    (void)ctx;
    SetMode(MODE_NORMAL); 
    printf("[VIM] Entered NORMAL mode\n");
}

void HandleEnterInsertOut(VimContext* ctx) { 
    if (!ctx) return;
    int pos = VimCtx_GetCursor(ctx);
    VimCtx_SetCursor(ctx, pos + 1);
    SetMode(MODE_INSERT); 
    printf("[VIM] Entered INSERT mode\n");
}

void HandleEnterInsertIn(VimContext* ctx) { 
    if (!ctx) return;
    int pos = VimCtx_GetCursor(ctx);
    VimCtx_SetCursor(ctx, pos);
    SetMode(MODE_INSERT); 
    printf("[VIM] Entered INSERT mode\n");
}

void HandleEnterVisual(VimContext* ctx) { 
    (void)ctx;
    SetMode(MODE_VISUAL); 
    printf("[VIM] Entered VISUAL mode\n");
}

void HandleMoveLeft(VimContext* ctx) {
    if (!ctx) return;
    int pos = VimCtx_GetCursor(ctx);
    if (pos > 0) VimCtx_SetCursor(ctx, pos - 1);
}

void HandleEnter(VimContext* ctx) {
    if (!ctx) return;
    if (ctx->lb && UI_IsListBoxSelected(ctx->lb)) {
        // simulate enter on selection - use g_enter_callback via state
        // For abstraction, just move cursor right as fallback
        int pos = VimCtx_GetCursor(ctx);
        VimCtx_SetCursor(ctx, pos + 1);
    } else {
        int pos = VimCtx_GetCursor(ctx);
        char text[512];
        VimCtx_GetText(ctx, text, sizeof(text));
        int len = (int)strlen(text);
        if (pos < len) VimCtx_SetCursor(ctx, pos + 1);
    }
}
void HandleMoveUp(VimContext* ctx) {
    if (!ctx || !ctx->lb) return;
    UI_NavigateUp(ctx->lb, ctx->hListBox);
}
void HandleMoveRight(VimContext* ctx) {
    if (!ctx) return;
    int pos = VimCtx_GetCursor(ctx);
    char text[512];
    VimCtx_GetText(ctx, text, sizeof(text));
    int len = (int)strlen(text);
    if (pos < len) VimCtx_SetCursor(ctx, pos + 1);
}

void HandleMoveDown(VimContext* ctx) {
    if (!ctx || !ctx->lb) return;
    UI_NavigateDown(ctx->lb, ctx->hListBox);
}

void HandlePersistentDelete(VimContext* ctx) {
    if (!ctx || !ctx->lb) return;
    if (!UI_IsListBoxSelected(ctx->lb)) return;
    if (g_ctrl_enter_callback != NULL && ctx->lb) {
        const char* txt = ctx->lb->populated_cache[ctx->lb->selected_index];
        g_ctrl_enter_callback(txt, ctx->lb);
    }
}

void HandleDeleteCharInsert(VimContext* ctx) {
    DeleteCharUnderCursor(ctx);
    HandleEnterInsertIn(ctx); 
}

void HandleDeleteCharNormal(VimContext* ctx) {
    if (!ctx) return;
    DeleteCharUnderCursor(ctx);
    printf("[VIM] Deleted character under cursor\n");
}

void HandleDeleteLine(VimContext* ctx) {
    if (!ctx) return;
    VimCtx_SetText(ctx, "");
    printf("[VIM] Deleted Entire Search Box \n");
}

void HandleDeleteLineInsert(VimContext* ctx) {
    if (!ctx) return;
    VimCtx_SetText(ctx, "");
    SetMode(MODE_INSERT);
    printf("[VIM] Deleted Entire Search Box & Switched to Insert\n");
}

void HandleMoveListboxStart(VimContext* ctx) {
    GoToFirstListBoxItem(ctx);
}
void HandleMoveListboxEnd(VimContext* ctx) {
    GoToLastListBoxItem(ctx);
}
void HandleMoveLineEnd(VimContext* ctx) {
  SetCursorToEndOfText(ctx);
}
void HandleMoveLineStart(VimContext* ctx) {
  SetCursorToStartOfText(ctx);
}

void HandleEnterInsertInStart(VimContext* ctx) {
  SetCursorToStartOfText(ctx);
  HandleEnterInsertIn(ctx);
}

void HandleEnterInsertOutEnd(VimContext* ctx) {
  SetCursorToEndOfText(ctx);
  HandleEnterInsertOut(ctx);
}
void HandleExitWindow(VimContext* ctx) {
  (void)ctx;
#if defined(_WIN32)
  if (WH_TO_HWND(g_hEditPopup) != NULL) {
    UI_Edit_KeyDown(g_ActiveEditControl, VK_ESCAPE);
    UI_Edit_KeyUp(g_ActiveEditControl, VK_ESCAPE);
  } else {
    selection_complete = true;
  }
#else
  {
    extern bool g_PromptJustClosed;
    extern long g_PromptCloseTimeMs;
    g_PromptJustClosed = true;
    struct timeval tv; gettimeofday(&tv, NULL);
    g_PromptCloseTimeMs = tv.tv_sec*1000 + tv.tv_usec/1000;
  }
  selection_complete = true;
#endif
}

void HandleYankLine(VimContext* ctx) {
    if (!ctx) return;
    char textBuffer[512] = {0};
    VimCtx_GetText(ctx, textBuffer, sizeof(textBuffer));
    textBuffer[strcspn(textBuffer, "\r\n")] = '\0';
    if (strlen(textBuffer) > 0) {
        Platform_CopyToClipboard(textBuffer);
    } else {
        printf("[VIM] Yank failed: Search box is currently empty.\n");
    }
}

void HandleInsertControlEnter(VimContext* ctx) {
  if (!ctx || !ctx->lb) return;
  char text[256];
  if (UI_IsListBoxSelected(ctx->lb) && ctx->lb) {
    snprintf(text,sizeof(text),"%s",ctx->lb->populated_cache[ctx->lb->selected_index]);
  } else {
    VimCtx_GetText(ctx, text, sizeof(text));
  }
  printf("TRYING TO DO CTRL ENTER WITH TEXT %s \n",text);
  if (g_ctrl_enter_callback != NULL) {
    g_ctrl_enter_callback(text, ctx->lb);
  }
  selection_complete = false;
}

void HandleMoveWordForward(VimContext* ctx) {
    if (!ctx) return;
    char text[512];
    VimCtx_GetText(ctx, text, sizeof(text));
    int start = VimCtx_GetCursor(ctx);
    int len = strlen(text);
    for (int i = start + 1;i <= len;i++) {
        if (i == len) {
          VimCtx_SetCursor(ctx, len - 1);
          break;
        }
        if (text[i] == ' ') {
          VimCtx_SetCursor(ctx, i + 1);
          break;
        }
    }
}

void HandleMoveWordBackward(VimContext* ctx) {
    if (!ctx) return;
    char text[512];
    VimCtx_GetText(ctx, text, sizeof(text));
    int pos = VimCtx_GetCursor(ctx);
    for (int i = pos - 1; i >= -1; i--) {
      char current_char = (i >= 0) ? text[i] : ' ';
      if (current_char != ' ') {
        continue;
      } else {
        for (int j = i - 1; j >= -1; j--) {
          char next_char = (j >= 0) ? text[j] : ' ';
          if (next_char != ' ') {
            VimCtx_SetCursor(ctx, j + 1);
            return; 
          } else {
            continue;
          }
        }
        VimCtx_SetCursor(ctx, 0);
        return;
      }
    }
}
void HandleDeleteSingleQuotesInsert(VimContext* ctx) {
    DeleteInsideDelimiter(ctx, '\'', '\'', true);
}
void HandleDeleteSingleQuotesNormal(VimContext* ctx) {
    DeleteInsideDelimiter(ctx, '\'', '\'', false);
}

void HandleDeleteQuotesInsert(VimContext* ctx) {
    DeleteInsideDelimiter(ctx, '\"', '\"', true);
}
void HandleDeleteQuotesNormal(VimContext* ctx) {
    DeleteInsideDelimiter(ctx, '\"', '\"', false);
}

void HandleDeleteBracketsInsert(VimContext* ctx) {
    DeleteInsideDelimiter(ctx, '(', ')', true);
}
void HandleDeleteBracketsNormal(VimContext* ctx) {
    DeleteInsideDelimiter(ctx, '(', ')', false);
}

void HandleDeleteCurlyBracketsInsert(VimContext* ctx) {
    DeleteInsideDelimiter(ctx, '{', '}', true);
}
void HandleDeleteCurlyBracketsNormal(VimContext* ctx) {
    DeleteInsideDelimiter(ctx, '{', '}', false);
}

void HandleDeleteSquareBracketsInsert(VimContext* ctx) {
    DeleteInsideDelimiter(ctx, '[', ']', true);
}
void HandleDeleteSquareBracketsNormal(VimContext* ctx) {
    DeleteInsideDelimiter(ctx, '[', ']', false);
}
void HandleDeleteWordInsert(VimContext* ctx) {
    DeleteInnerWord(ctx, true);   
}

void HandleDeleteWordNormal(VimContext* ctx) {
    DeleteInnerWord(ctx, false); 
}
void HandleSendEdit(VimContext* ctx) {
    (void)ctx;
#if defined(_WIN32)
    UI_Edit_KeyDown(g_ActiveEditControl,EDIT_TEXT_KEY);
    UI_Edit_KeyUp(g_ActiveEditControl, EDIT_TEXT_KEY);
#else
    if (g_edit_callback && ctx && ctx->lb && UI_IsListBoxSelected(ctx->lb)) {
        const char* sel = ctx->lb->populated_cache[ctx->lb->selected_index];
        g_edit_callback(sel, ctx->lb);
    }
#endif
}

void HandleSendCopyShift(VimContext* ctx) {
#if defined(_WIN32)
    (void)ctx;
    UI_Edit_KeyUp(g_ActiveEditControl,VK_SHIFT);
    UI_Edit_KeyDown(g_ActiveEditControl,COPY_SELECTION_KEY);
    UI_Edit_KeyUp(g_ActiveEditControl, COPY_SELECTION_KEY);
    UI_Edit_KeyDown(g_ActiveEditControl,VK_SHIFT);
#else
    if (!ctx || !ctx->lb) return;
    const char* sel = UI_IsListBoxSelected(ctx->lb) ? ctx->lb->populated_cache[ctx->lb->selected_index] : "";
    if (g_copy_callback) g_copy_callback(sel, ctx->lb->selected_index, 0);
    else Platform_CopyToClipboard(sel);
#endif
}
void HandleSendCopyControl(VimContext* ctx) {
#if defined(_WIN32)
    (void)ctx;
    UI_Edit_KeyUp(g_ActiveEditControl,VK_CONTROL);
    UI_Edit_KeyDown(g_ActiveEditControl,COPY_SELECTION_KEY);
    UI_Edit_KeyUp(g_ActiveEditControl, COPY_SELECTION_KEY);
    UI_Edit_KeyDown(g_ActiveEditControl,VK_CONTROL);
#else
    if (!ctx || !ctx->lb) return;
    const char* sel = UI_IsListBoxSelected(ctx->lb) ? ctx->lb->populated_cache[ctx->lb->selected_index] : "";
    if (g_copy_callback) g_copy_callback(sel, ctx->lb->selected_index, 1);
    else Platform_CopyToClipboard(sel);
#endif
}
void HandleCopyToSearch(VimContext* ctx) {
  if (!ctx) return;
  if (ctx->lb && UI_IsListBoxSelected(ctx->lb)) {
    const char* text = ctx->lb->populated_cache[ctx->lb->selected_index];
    VimCtx_SetText(ctx, text);
    VimCtx_SetCursor(ctx, (int)strlen(text));
  }
}
void HandleToggleHistory(VimContext* ctx) {
  if (!ctx || !ctx->lb) return;
  if (UI_IsListBoxSelected(ctx->lb)) {
    UI_SetSelection(ctx->lb, ctx->hListBox, -1);
  }
  GetFromHistory(ctx->lb);
}

void HandleShowFullText(VimContext* ctx) {
  if (FullTextNeedsKeyPress) {
      if (ctx && ctx->lb && UI_IsListBoxSelected(ctx->lb)) {
        SubjectiveMessageDisplay(ctx->lb->populated_cache[ctx->lb->selected_index], 140, 6969,COLOR_TEXT);
      }
  }
}

void SearchTextForward(int key) {
  VimContext ctx = VimCtx_FromGlobals();
  char text[512];
  VimCtx_GetText(&ctx, text, sizeof(text));
  int cursor = VimCtx_GetCursor(&ctx);
  for (int i = 0; i < (int)strlen(text);i++) {
    if (tolower((unsigned char)text[i]) == tolower((unsigned char)(char)key)) {
      if (i <= cursor ) continue;
      VimCtx_SetCursor(&ctx,i);
      return;
    }
  }
}
void SearchTextBackward(int key) {
  VimContext ctx = VimCtx_FromGlobals();
  char text[512];
  VimCtx_GetText(&ctx, text, sizeof(text));
  int cursor = VimCtx_GetCursor(&ctx);
  for (int i = 0; i < (int)strlen(text);i++) {
    if (tolower((unsigned char)text[i]) == tolower((unsigned char)(char)key)) {
      if (i >= cursor ) continue;
      VimCtx_SetCursor(&ctx,i);
      return;
    }
  }
}

void HandleSearchTextForward(VimContext* ctx) {
  (void)ctx;
  if (ctx && ctx->lb && UI_IsListBoxSelected(ctx->lb)) return;
  Platform_WaitForKeypressAsync(SearchTextForward);
}

void HandleSearchTextBackward(VimContext* ctx) {
  (void)ctx;
  if (ctx && ctx->lb && UI_IsListBoxSelected(ctx->lb)) return;
  Platform_WaitForKeypressAsync(SearchTextBackward);
}
void HandleDeleteTillEnd(VimContext* ctx) {
  if (!ctx) return;
  if (ctx->lb && UI_IsListBoxSelected(ctx->lb)) return;
  char text[512];
  VimCtx_GetText(ctx, text, sizeof(text));
  int cursor = VimCtx_GetCursor(ctx);
  for (int i = cursor; i < (int)strlen(text); ++i){
      text[i] = '\0';
  }
  VimCtx_SetText(ctx, text);
  VimCtx_SetCursor(ctx, cursor);
}
void HandlePageUp(VimContext* ctx) {
  if (!ctx || !ctx->lb) return;
  int current_sel = ctx->lb->selected_index;
  int wanted_sel = current_sel - PageUpDownLen;
  if (wanted_sel <= 0) wanted_sel = 0;
  UI_SetSelection(ctx->lb, ctx->hListBox, wanted_sel);
  UI_LB_SetTopIndex(ctx->hListBox,wanted_sel);
}
void HandlePageDown(VimContext* ctx) {
  if (!ctx || !ctx->lb) return;
  int current_sel = ctx->lb->selected_index;
  int wanted_sel = current_sel + PageUpDownLen;
  if (wanted_sel >= ctx->lb->total_matches) wanted_sel = ctx->lb->total_matches;
  UI_SetSelection(ctx->lb, ctx->hListBox, wanted_sel - 1);
  UI_LB_SetTopIndex(ctx->hListBox,current_sel);
}
void HandleNormalEdit(VimContext* ctx) {
  if (!ctx || !ctx->lb) return;
  UI_SetSelection(ctx->lb, ctx->hListBox, -1);
}
