#include "core/context.h"
#include "core/modal.h"
#include "bindings/vim_motions.h"
#include "bindings/vim_helpers.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include "core/vim.h"
#include "core/window_manager.h"
#include "config.h"
#include "platform/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_ui.h"
#include <ctype.h>
#include <stdio.h>

void HandleEnterNormal(FlintContext* ctx) { 
    (void)ctx;
    SetMode(MODE_NORMAL); 
}

void HandleEnterInsertOut(FlintContext* ctx) { 
    if (!ctx) return;
    int pos = Vim_GetCursor(ctx);
    Vim_SetCursor(ctx, pos + 1);
    SetMode(MODE_INSERT); 
}

void HandleEnterInsertIn(FlintContext* ctx) { 
    if (!ctx) return;
    int pos = Vim_GetCursor(ctx);
    Vim_SetCursor(ctx, pos);
    SetMode(MODE_INSERT); 
}

void HandleEnterVisual(FlintContext* ctx) { 
    (void)ctx;
    SetMode(MODE_VISUAL); 
}

void HandleMoveLeft(FlintContext* ctx) {
    if (!ctx) return;
    int pos = Vim_GetCursor(ctx);
    if (pos > 0) Vim_SetCursor(ctx, pos - 1);
}

void HandleEnter(FlintContext* ctx) {
    if (!ctx) return;
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (lb && UI_IsListBoxSelected(lb)) {
        const char* sel = lb->populated_cache[lb->selected_index];
        if (ctx->prompt_callbacks.enter) {
            bool should_close = ctx->prompt_callbacks.enter(sel, lb);
            ctx->window.selection_complete = should_close;
        } else {
            ctx->window.selection_complete = true;
        }
        return;
    }
    int pos = Vim_GetCursor(ctx);
    char text[512];
    Vim_GetText(ctx, text, sizeof(text));
    int len = (int)strlen(text);
    if (pos < len) Vim_SetCursor(ctx, pos + 1);
}
void HandleMoveUp(FlintContext* ctx) {
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (!ctx || !lb) return;
    UI_NavigateUp(lb, ctx->window.hListBox);
}
void HandleMoveRight(FlintContext* ctx) {
    if (!ctx) return;
    int pos = Vim_GetCursor(ctx);
    char text[512];
    Vim_GetText(ctx, text, sizeof(text));
    int len = (int)strlen(text);
    if (pos < len) Vim_SetCursor(ctx, pos + 1);
}

void HandleMoveDown(FlintContext* ctx) {
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (!ctx || !lb) return;
    UI_NavigateDown(lb, ctx->window.hListBox);
}

void HandlePersistentDelete(FlintContext* ctx) {
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (!ctx || !lb) return;
    if (!UI_IsListBoxSelected(lb)) return;
    if (ctx->prompt_callbacks.ctrl_enter != NULL) {
        const char* txt = lb->populated_cache[lb->selected_index];
        bool should_close = ctx->prompt_callbacks.ctrl_enter(txt, lb);
        ctx->window.selection_complete = should_close;
    }
}

void HandleDeleteCharInsert(FlintContext* ctx) {
    DeleteCharUnderCursor(ctx);
    HandleEnterInsertIn(ctx); 
}

void HandleDeleteCharNormal(FlintContext* ctx) {
    if (!ctx) return;
    DeleteCharUnderCursor(ctx);
}

void HandleDeleteLine(FlintContext* ctx) {
    if (!ctx) return;
    Vim_SetText(ctx, "");
}

void HandleDeleteLineInsert(FlintContext* ctx) {
    if (!ctx) return;
    Vim_SetText(ctx, "");
    SetMode(MODE_INSERT);
}

void HandleMoveListboxStart(FlintContext* ctx) {
    GoToFirstListBoxItem(ctx);
}
void HandleMoveListboxEnd(FlintContext* ctx) {
    GoToLastListBoxItem(ctx);
}
void HandleMoveLineEnd(FlintContext* ctx) {
  SetCursorToEndOfText(ctx);
}
void HandleMoveLineStart(FlintContext* ctx) {
  SetCursorToStartOfText(ctx);
}

void HandleEnterInsertInStart(FlintContext* ctx) {
  SetCursorToStartOfText(ctx);
  HandleEnterInsertIn(ctx);
}

void HandleEnterInsertOutEnd(FlintContext* ctx) {
  SetCursorToEndOfText(ctx);
  HandleEnterInsertOut(ctx);
}
void HandleExitWindow(FlintContext* ctx) {
    (void)ctx;
    WindowManager* wm = WindowManager_Get();
    if (!wm) return;
    Popup* top = WindowManager_GetTopPopup(wm);
    if (top && top->manager->popup_count > 1) {
        SetMode(MODE_NORMAL);
    } else {
        FlintContext_Get()->window.selection_complete = true;
    }
}
void HandleYankLine(FlintContext* ctx) {
    if (!ctx) return;
    char textBuffer[512] = {0};
    Vim_GetText(ctx, textBuffer, sizeof(textBuffer));
    textBuffer[strcspn(textBuffer, "\r\n")] = '\0';
    if (strlen(textBuffer) > 0) {
        Platform_CopyToClipboard(textBuffer);
    }
}

void HandleInsertControlEnter(FlintContext* ctx) {
  ListBoxInfo* lb = Vim_GetListBox(ctx);
  if (!ctx || !lb) return;
  char text[256];
  if (UI_IsListBoxSelected(lb)) {
    snprintf(text,sizeof(text),"%s",lb->populated_cache[lb->selected_index]);
  } else {
    Vim_GetText(ctx, text, sizeof(text));
  }
  if (ctx->prompt_callbacks.ctrl_enter != NULL) {
    bool should_close = ctx->prompt_callbacks.ctrl_enter(text, lb);
    ctx->window.selection_complete = should_close;
  } else {
    ctx->window.selection_complete = false;
  }
}

void HandleMoveWordForward(FlintContext* ctx) {
    if (!ctx) return;
    char text[512];
    Vim_GetText(ctx, text, sizeof(text));
    int start = Vim_GetCursor(ctx);
    int len = strlen(text);
    for (int i = start + 1;i <= len;i++) {
        if (i == len) {
          Vim_SetCursor(ctx, len - 1);
          break;
        }
        if (text[i] == ' ') {
          Vim_SetCursor(ctx, i + 1);
          break;
        }
    }
}

void HandleMoveWordBackward(FlintContext* ctx) {
    if (!ctx) return;
    char text[512];
    Vim_GetText(ctx, text, sizeof(text));
    int pos = Vim_GetCursor(ctx);
    for (int i = pos - 1; i >= -1; i--) {
      char current_char = (i >= 0) ? text[i] : ' ';
      if (current_char != ' ') {
        continue;
      } else {
        for (int j = i - 1; j >= -1; j--) {
          char next_char = (j >= 0) ? text[j] : ' ';
          if (next_char != ' ') {
            Vim_SetCursor(ctx, j + 1);
            return; 
          } else {
            continue;
          }
        }
        Vim_SetCursor(ctx, 0);
        return;
      }
    }
}
void HandleDeleteSingleQuotesInsert(FlintContext* ctx) {
  DeleteInsideDelimiter(ctx, '\'', '\'', true); 
}
void HandleDeleteSingleQuotesNormal(FlintContext* ctx) {
  DeleteInsideDelimiter(ctx, '\'', '\'', false); 
}
void HandleDeleteQuotesInsert(FlintContext* ctx) {
  DeleteInsideDelimiter(ctx, '\"', '\"', true); 
}
void HandleDeleteQuotesNormal(FlintContext* ctx) {
  DeleteInsideDelimiter(ctx, '\"', '\"', false); 
}
void HandleDeleteBracketsInsert(FlintContext* ctx) {
  DeleteInsideDelimiter(ctx, '(', ')', true); 
}
void HandleDeleteBracketsNormal(FlintContext* ctx) {
  DeleteInsideDelimiter(ctx, '(', ')', false); 
}
void HandleDeleteCurlyBracketsInsert(FlintContext* ctx) { DeleteInsideDelimiter(ctx, '{', '}', true); }
void HandleDeleteCurlyBracketsNormal(FlintContext* ctx) { DeleteInsideDelimiter(ctx, '{', '}', false); }
void HandleDeleteSquareBracketsInsert(FlintContext* ctx) {
  DeleteInsideDelimiter(ctx, '[', ']', true); 
}
void HandleDeleteSquareBracketsNormal(FlintContext* ctx) {
  DeleteInsideDelimiter(ctx, '[', ']', false); 
}
void HandleDeleteWordInsert(FlintContext* ctx) {
  DeleteInnerWord(ctx, true); 
}
void HandleDeleteWordNormal(FlintContext* ctx) {
  DeleteInnerWord(ctx, false); 
}
void HandleSendEdit(FlintContext* ctx) {
    //temporary no idea if this works on windows 
    FlintContext* _c = FlintContext_Get();
    ListBoxInfo* lb = Vim_GetListBox(_c);
    if (_c->prompt_callbacks.edit && lb && UI_IsListBoxSelected(lb)) {
        const char* sel = lb->populated_cache[lb->selected_index];
        _c->prompt_callbacks.edit(sel, lb);
    }
}

void HandleSendCopyShift(FlintContext* ctx) {
    //same with this 
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (!lb) return;
    const char* sel = UI_IsListBoxSelected(lb) ? lb->populated_cache[lb->selected_index] : "";
    if (ctx->prompt_callbacks.copy) ctx->prompt_callbacks.copy(sel, lb->selected_index, 0);
    else Platform_CopyToClipboard(sel);
}
void HandleSendCopyControl(FlintContext* ctx) {
    // same
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (!lb) return;
    const char* sel = UI_IsListBoxSelected(lb) ? lb->populated_cache[lb->selected_index] : "";
    if (ctx->prompt_callbacks.copy) ctx->prompt_callbacks.copy(sel, lb->selected_index, 1);
    else Platform_CopyToClipboard(sel);
}
void HandleCopyToSearch(FlintContext* ctx) {
  if (!ctx) return;
  ListBoxInfo* lb = Vim_GetListBox(ctx);
  if (lb && UI_IsListBoxSelected(lb)) {
    const char* text = lb->populated_cache[lb->selected_index];
    Vim_SetText(ctx, text);
    Vim_SetCursor(ctx, (int)strlen(text));
  }
}
void HandleToggleHistory(FlintContext* ctx) {
  ListBoxInfo* lb = Vim_GetListBox(ctx);
  if (!ctx || !lb) return;
  if (UI_IsListBoxSelected(lb)) {
    UI_SetSelection(lb, ctx->window.hListBox, -1);
  }
  GetFromHistory(lb);
}

void HandleShowFullText(FlintContext* ctx) {
  if (FullTextNeedsKeyPress) {
      ListBoxInfo* lb = Vim_GetListBox(ctx);
      if (lb && UI_IsListBoxSelected(lb)) {
        DisplayMessageSlot(lb->populated_cache[lb->selected_index], TOP_CENTER_RELATIVE, 0, 0, 0, 6969, COLOR_TEXT);
      }
  }
}

void SearchTextForward(int key) {
  FlintContext* ctx = FlintContext_Get();
  char text[512];
  Vim_GetText(ctx, text, sizeof(text));
  int cursor = Vim_GetCursor(ctx);
  for (int i = 0; i < (int)strlen(text);i++) {
    if (tolower((unsigned char)text[i]) == tolower((unsigned char)(char)key)) {
      if (i <= cursor ) continue;
      Vim_SetCursor(ctx,i);
      return;
    }
  }
}
void SearchTextBackward(int key) {
  FlintContext* ctx = FlintContext_Get();
  char text[512];
  Vim_GetText(ctx, text, sizeof(text));
  int cursor = Vim_GetCursor(ctx);
  for (int i = 0; i < (int)strlen(text);i++) {
    if (tolower((unsigned char)text[i]) == tolower((unsigned char)(char)key)) {
      if (i >= cursor ) continue;
      Vim_SetCursor(ctx,i);
      return;
    }
  }
}

void HandleSearchTextForward(FlintContext* ctx) {
  ListBoxInfo* lb = Vim_GetListBox(ctx);
  if (lb && UI_IsListBoxSelected(lb)) return;
  Platform_WaitForKeypressAsync(SearchTextForward);
}

void HandleSearchTextBackward(FlintContext* ctx) {
  ListBoxInfo* lb = Vim_GetListBox(ctx);
  if (lb && UI_IsListBoxSelected(lb)) return;
  Platform_WaitForKeypressAsync(SearchTextBackward);
}
void HandleDeleteTillEnd(FlintContext* ctx) {
  if (!ctx) return;
  ListBoxInfo* lb = Vim_GetListBox(ctx);
  if (lb && UI_IsListBoxSelected(lb)) return;
  char text[512];
  Vim_GetText(ctx, text, sizeof(text));
  int cursor = Vim_GetCursor(ctx);
  for (int i = cursor; i < (int)strlen(text); ++i){
      text[i] = '\0';
  }
  Vim_SetText(ctx, text);
  Vim_SetCursor(ctx, cursor);
}
void HandlePageUp(FlintContext* ctx) {
  ListBoxInfo* lb = Vim_GetListBox(ctx);
  if (!ctx || !lb) return;
  int current_sel = lb->selected_index;
  int wanted_sel = current_sel - PageUpDownLen;
  if (wanted_sel <= 0) wanted_sel = 0;
  UI_SetSelection(lb, ctx->window.hListBox, wanted_sel);
  UI_LB_SetTopIndex(ctx->window.hListBox,wanted_sel);
}
void HandlePageDown(FlintContext* ctx) {
  ListBoxInfo* lb = Vim_GetListBox(ctx);
  if (!ctx || !lb) return;
  int current_sel = lb->selected_index;
  int wanted_sel = current_sel + PageUpDownLen;
  if (wanted_sel >= lb->total_matches) wanted_sel = lb->total_matches;
  UI_SetSelection(lb, ctx->window.hListBox, wanted_sel - 1);
  UI_LB_SetTopIndex(ctx->window.hListBox,current_sel);
}
void HandleNormalEdit(FlintContext* ctx) {
  ListBoxInfo* lb = Vim_GetListBox(ctx);
  if (!ctx || !lb) return;
  UI_SetSelection(lb, ctx->window.hListBox, -1);
}
