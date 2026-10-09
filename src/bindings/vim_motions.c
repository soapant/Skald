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
#include "bindings/vim_state.h"
#include <ctype.h>
#include <stdio.h>

static char vim_pending_mark;

static void vim_get_text(FlintContext* ctx, char* buf, int cap);
static void vim_clamp_cursor(FlintContext* ctx, int pos);

void vim_clamp_cursor(FlintContext* ctx, int pos);
void vim_find_record(char target, bool till, bool backward);
void vim_mark_set(char name, int cursor);
void vim_mark_jump(char name);

static const char* lb_selected_text(ListBoxInfo* lb) {
    const char* s = ListBoxInfo_Row(lb, lb ? lb->selected_index : -1);
    return s ? s : "";
}

void HandleEnterNormal(FlintContext* ctx) { 
    (void)ctx;
    SetMode(MODE_NORMAL); 
}

void HandleEnterInsertOut(FlintContext* ctx) { 
    if (!ctx) return;
    int pos = Vim_GetCursor(ctx);
    Vim_SetCursor(ctx, pos + 1);
    VimState_SetLastInsert(pos + 1);
    SetMode(MODE_INSERT); 
}

void HandleEnterInsertIn(FlintContext* ctx) { 
    if (!ctx) return;
    int pos = Vim_GetCursor(ctx);
    Vim_SetCursor(ctx, pos);
    VimState_SetLastInsert(pos);
    SetMode(MODE_INSERT); 
}

void HandleEnterVisual(FlintContext* ctx) {
    (void)ctx;
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (lb && UI_IsListBoxSelected(lb)) return;
    SetMode(MODE_VISUAL);
}

void HandleExitVisual(FlintContext* ctx) {
    (void)ctx;
    SetMode(MODE_NORMAL);
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
        const char* sel = lb_selected_text(lb);
        if (ctx->prompt_callbacks.enter) {
            bool should_close = ctx->prompt_callbacks.enter(sel, lb);
            ctx->window.selection_complete = should_close;
        } else {
            ctx->window.selection_complete = true;
        }
        return;
    }
    int pos = Vim_GetCursor(ctx);
    char text[VIM_TEXT_BUF];
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
    char text[VIM_TEXT_BUF];
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
        const char* txt = lb_selected_text(lb);
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
    char textBuffer[VIM_TEXT_BUF] = {0};
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
    snprintf(text,sizeof(text),"%s",lb_selected_text(lb));
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
    char text[VIM_TEXT_BUF];
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
    char text[VIM_TEXT_BUF];
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
void HandleDeleteWordForward(FlintContext* ctx) {
  DeleteWordForward(ctx, false);
}
void HandleChangeWordForward(FlintContext* ctx) {
  DeleteWordForward(ctx, true);
}
void HandleSendEdit(FlintContext* ctx) {
    //temporary no idea if this works on windows 
    FlintContext* _c = FlintContext_Get();
    ListBoxInfo* lb = Vim_GetListBox(_c);
    if (_c->prompt_callbacks.edit && lb && UI_IsListBoxSelected(lb)) {
        const char* sel = lb_selected_text(lb);
        _c->prompt_callbacks.edit(sel, lb);
    }
}

void HandleSendCopyShift(FlintContext* ctx) {
    //same with this 
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (!lb) return;
    const char* sel = UI_IsListBoxSelected(lb) ? lb_selected_text(lb) : "";
    if (ctx->prompt_callbacks.copy) ctx->prompt_callbacks.copy(sel, lb->selected_index, 0);
    else Platform_CopyToClipboard(sel);
}
void HandleSendCopyControl(FlintContext* ctx) {
    // same
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (!lb) return;
    const char* sel = UI_IsListBoxSelected(lb) ? lb_selected_text(lb) : "";
    if (ctx->prompt_callbacks.copy) ctx->prompt_callbacks.copy(sel, lb->selected_index, 1);
    else Platform_CopyToClipboard(sel);
}
void HandleCopyToSearch(FlintContext* ctx) {
  if (!ctx) return;
  ListBoxInfo* lb = Vim_GetListBox(ctx);
  if (lb && UI_IsListBoxSelected(lb)) {
    const char* text = lb_selected_text(lb);
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
        DisplayMessageSlot(lb_selected_text(lb), TOP_CENTER_RELATIVE, 0, 0, 0, 6969, COLOR_TEXT);
      }
  }
}

void SearchTextForward(int key) {
  FlintContext* ctx = FlintContext_Get();
  if (!ctx) return;
  char text[VIM_TEXT_BUF];
  vim_get_text(ctx, text, sizeof(text));
  int len = (int)strlen(text);
  int cursor = Vim_GetCursor(ctx);
  for (int i = cursor + 1; i < len; i++) {
    if (tolower((unsigned char)text[i]) == tolower((unsigned char)(char)key)) {
      vim_clamp_cursor(ctx, i);
      VimState_SetFind((char)key, VIM_FIND_F);
      return;
    }
  }
  for (int i = 0; i <= cursor && i < len; i++) {
    if (tolower((unsigned char)text[i]) == tolower((unsigned char)(char)key)) {
      vim_clamp_cursor(ctx, i);
      VimState_SetFind((char)key, VIM_FIND_F);
      return;
    }
  }
}
void SearchTextBackward(int key) {
  FlintContext* ctx = FlintContext_Get();
  if (!ctx) return;
  char text[VIM_TEXT_BUF];
  vim_get_text(ctx, text, sizeof(text));
  int len = (int)strlen(text);
  int cursor = Vim_GetCursor(ctx);
  for (int i = cursor - 1; i >= 0; i--) {
    if (tolower((unsigned char)text[i]) == tolower((unsigned char)(char)key)) {
      vim_clamp_cursor(ctx, i);
      VimState_SetFind((char)key, VIM_FIND_BACKWARD);
      return;
    }
  }
  for (int i = len - 1; i >= 0; i--) {
    if (i >= cursor) continue;
    if (tolower((unsigned char)text[i]) == tolower((unsigned char)(char)key)) {
      vim_clamp_cursor(ctx, i);
      VimState_SetFind((char)key, VIM_FIND_BACKWARD);
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
  char text[VIM_TEXT_BUF];
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

static void vim_get_text(FlintContext* ctx, char* buf, int cap) {
    if (!ctx) { buf[0] = '\0'; return; }
    Vim_GetText(ctx, buf, cap);
}

static void vim_clamp_cursor(FlintContext* ctx, int pos) {
    if (!ctx) return;
    char text[VIM_TEXT_BUF];
    vim_get_text(ctx, text, sizeof(text));
    int len = (int)strlen(text);
    if (len == 0) { Vim_SetCursor(ctx, 0); return; }
    if (pos < 0) pos = 0;
    if (pos > len - 1) pos = len - 1;
    Vim_SetCursor(ctx, pos);
}

void vim_mark_set(char name, int cursor) {
    FlintContext* ctx = FlintContext_Get();
    ListBoxInfo* lb = ctx ? Vim_GetListBox(ctx) : NULL;
    int sel = lb ? lb->selected_index : -1;
    if (!VimState_MarkIsSet(VIM_MARK_PREV)) {
        VimState_SetMark(VIM_MARK_PREV, cursor, sel);
    }
    VimState_SetMark(name, cursor, sel);
    VimState_PushJump(cursor, sel);
}

void vim_mark_jump(char name) {
    FlintContext* ctx = FlintContext_Get();
    if (!ctx) return;
    int cursor = 0, sel = 0;
    if (!VimState_GetMark(name, &cursor, &sel)) return;
    vim_clamp_cursor(ctx, cursor);
}

void HandleWordEndForward(FlintContext* ctx) {
    if (!ctx) return;
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (lb && UI_IsListBoxSelected(lb)) return;
    char text[VIM_TEXT_BUF];
    vim_get_text(ctx, text, sizeof(text));
    int pos = Vim_WordStep(text, (int)strlen(text), Vim_GetCursor(ctx), false, true, true);
    vim_clamp_cursor(ctx, pos);
}

void HandleWordEndBackward(FlintContext* ctx) {
    if (!ctx) return;
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (lb && UI_IsListBoxSelected(lb)) return;
    char text[VIM_TEXT_BUF];
    vim_get_text(ctx, text, sizeof(text));
    int pos = Vim_WordStep(text, (int)strlen(text), Vim_GetCursor(ctx), false, true, false);
    vim_clamp_cursor(ctx, pos);
}

void HandleWordEndForwardBig(FlintContext* ctx) {
    if (!ctx) return;
    char text[VIM_TEXT_BUF];
    vim_get_text(ctx, text, sizeof(text));
    int pos = Vim_WordStep(text, (int)strlen(text), Vim_GetCursor(ctx), true, true, true);
    vim_clamp_cursor(ctx, pos);
}

void HandleWordEndBackwardBig(FlintContext* ctx) {
    if (!ctx) return;
    char text[VIM_TEXT_BUF];
    vim_get_text(ctx, text, sizeof(text));
    int pos = Vim_WordStep(text, (int)strlen(text), Vim_GetCursor(ctx), true, true, false);
    vim_clamp_cursor(ctx, pos);
}

void HandleWordForwardBig(FlintContext* ctx) {
    if (!ctx) return;
    char text[VIM_TEXT_BUF];
    vim_get_text(ctx, text, sizeof(text));
    int pos = Vim_WordStep(text, (int)strlen(text), Vim_GetCursor(ctx), true, false, true);
    vim_clamp_cursor(ctx, pos);
}

void HandleWordBackwardBig(FlintContext* ctx) {
    if (!ctx) return;
    char text[VIM_TEXT_BUF];
    vim_get_text(ctx, text, sizeof(text));
    int pos = Vim_WordStep(text, (int)strlen(text), Vim_GetCursor(ctx), true, false, false);
    vim_clamp_cursor(ctx, pos);
}

void HandleFirstNonBlank(FlintContext* ctx) {
    if (!ctx) return;
    char text[VIM_TEXT_BUF];
    vim_get_text(ctx, text, sizeof(text));
    vim_clamp_cursor(ctx, Vim_FirstNonBlank(text, (int)strlen(text)));
}

void HandleGotoLineStart(FlintContext* ctx) {
    if (!ctx) return;
    Vim_SetCursor(ctx, 0);
}

void HandleGotoLineEnd(FlintContext* ctx) {
    SetCursorToEndOfText(ctx);
    if (!ctx) return;
    char text[VIM_TEXT_BUF];
    vim_get_text(ctx, text, sizeof(text));
    int len = (int)strlen(text);
    vim_clamp_cursor(ctx, len);
}

void HandleBracketMatchPercent(FlintContext* ctx) {
    if (!ctx) return;
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (lb && UI_IsListBoxSelected(lb)) return;
    char text[VIM_TEXT_BUF];
    vim_get_text(ctx, text, sizeof(text));
    int match = Vim_BracketMatch(text, (int)strlen(text), Vim_GetCursor(ctx));
    if (match < 0) return;
    vim_clamp_cursor(ctx, match);
}

static void vim_do_word_search(FlintContext* ctx, bool backward) {
    char text[VIM_TEXT_BUF];
    vim_get_text(ctx, text, sizeof(text));
    int len = (int)strlen(text);
    int cursor = Vim_GetCursor(ctx);
    char word[128];
    if (!Vim_WordUnder(text, len, cursor, word, sizeof(word))) return;
    int from = cursor;
    if (backward) {
        while (from > 0 && !isspace((unsigned char)text[from - 1])) from--;
        from--;
    }
    int pos = Vim_FindPatternPos(text, len, from, word, backward);
    if (pos < 0) return;
    vim_clamp_cursor(ctx, pos);
}

void HandleStarSearch(FlintContext* ctx) {
    (void)ctx;
    vim_do_word_search(ctx, false);
}

void HandleHashSearch(FlintContext* ctx) {
    (void)ctx;
    vim_do_word_search(ctx, true);
}

void HandleGotoLastInsert(FlintContext* ctx) {
    if (!ctx) return;
    int pos = VimState_GetLastInsert();
    if (pos < 0) return;
    vim_clamp_cursor(ctx, pos);
    SetMode(MODE_INSERT);
}

void vim_find_record(char target, bool till, bool backward) {
    VimState_SetFind(target, backward ? VIM_FIND_BACKWARD : (till ? VIM_FIND_T : VIM_FIND_F));
}

static void vim_find_with(FlintContext* ctx, char target, bool till, bool backward) {
    if (!ctx) return;
    char text[VIM_TEXT_BUF];
    vim_get_text(ctx, text, sizeof(text));
    int pos = Vim_FindChar(text, (int)strlen(text), Vim_GetCursor(ctx), target, till, backward);
    if (pos < 0) return;
    vim_clamp_cursor(ctx, pos);
    VimState_SetFind(target, backward ? VIM_FIND_BACKWARD : (till ? VIM_FIND_T : VIM_FIND_F));
}

static void vim_show_pending(const char* prefix) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s--", prefix);
    DisplayMessageSlot(buf, TOP_CENTER_RELATIVE, 0, 0, 0, 1200, COLOR_TIP);
}

static void vim_clear_pending(void) {
    DisplayMessageSlot("", TOP_CENTER_RELATIVE, 0, 0, 0, 0, COLOR_TIP);
}

static void vim_till_forward_cb(int key) {
    vim_clear_pending();
    vim_find_with(FlintContext_Get(), (char)key, true, false);
}

static void vim_till_backward_cb(int key) {
    vim_clear_pending();
    vim_find_with(FlintContext_Get(), (char)key, true, true);
}

void HandleFindTillForward(FlintContext* ctx) {
    if (!ctx) return;
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (lb && UI_IsListBoxSelected(lb)) return;
    vim_show_pending("t");
    Platform_WaitForKeypressAsync(vim_till_forward_cb);
}

void HandleFindTillBackward(FlintContext* ctx) {
    if (!ctx) return;
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (lb && UI_IsListBoxSelected(lb)) return;
    vim_show_pending("T");
    Platform_WaitForKeypressAsync(vim_till_backward_cb);
}

static void vim_repeat_find(bool invert) {
    VimFindState fs;
    if (!VimState_GetFind(&fs)) return;
    bool backward = (fs.dir == VIM_FIND_BACKWARD);
    if (invert) backward = !backward;
    vim_find_with(FlintContext_Get(), fs.target, fs.dir == VIM_FIND_T, backward);
}

void HandleRepeatFindSame(FlintContext* ctx) {
    (void)ctx;
    vim_repeat_find(false);
}

void HandleRepeatFindOpp(FlintContext* ctx) {
    (void)ctx;
    vim_repeat_find(true);
}

static bool vim_repeat_pattern(bool backward) {
    char pattern[VIM_TEXT_BUF];
    if (!VimState_GetLastPattern(pattern, sizeof(pattern))) return false;
    FlintContext* ctx = FlintContext_Get();
    if (!ctx) return false;
    char text[VIM_TEXT_BUF];
    vim_get_text(ctx, text, sizeof(text));
    int pos = Vim_FindPatternPos(text, (int)strlen(text), Vim_GetCursor(ctx), pattern, backward);
    if (pos < 0) return false;
    vim_clamp_cursor(ctx, pos);
    return true;
}

static void vim_search_next(bool backward) {
    if (vim_repeat_pattern(backward)) return;
    vim_do_word_search(FlintContext_Get(), backward);
}

void HandleFindNext(FlintContext* ctx) {
    (void)ctx;
    vim_search_next(false);
}

void HandleFindPrev(FlintContext* ctx) {
    (void)ctx;
    vim_search_next(true);
}

void HandleCaseLower(FlintContext* ctx) { Vim_TransformCase(ctx, 0); }
void HandleCaseUpper(FlintContext* ctx) { Vim_TransformCase(ctx, 1); }
void HandleCaseToggle(FlintContext* ctx) { Vim_TransformCase(ctx, 2); }

static void vim_scroll_to(FlintContext* ctx, int anchor) {
    if (!ctx) return;
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (!lb || lb->populated_count <= 0) return;
    if (!UI_IsListBoxSelected(lb)) return;
    int visible = UI_LB_GetVisibleCount();
    if (visible <= 0) visible = 1;
    int sel = lb->selected_index;
    int top;
    if (anchor == 0) top = sel;
    else if (anchor == 1) top = sel - visible / 2;
    else top = sel - visible + 1;
    UI_LB_ScrollToTop(lb, top);
}

void HandleScrollTop(FlintContext* ctx) { vim_scroll_to(ctx, 0); }
void HandleScrollCenter(FlintContext* ctx) { vim_scroll_to(ctx, 1); }
void HandleScrollBottom(FlintContext* ctx) { vim_scroll_to(ctx, 2); }

void HandleScreenTop(FlintContext* ctx) {
    if (!ctx) return;
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (!lb || !UI_IsListBoxSelected(lb)) return;
    int visible = UI_LB_GetVisibleCount();
    if (visible <= 0) visible = 1;
    UI_NavigateTo(lb, ctx->window.hListBox, lb->view_offset);
}

void HandleScreenMiddle(FlintContext* ctx) {
    if (!ctx) return;
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (!lb || !UI_IsListBoxSelected(lb)) return;
    int visible = UI_LB_GetVisibleCount();
    if (visible <= 0) visible = 1;
    UI_NavigateTo(lb, ctx->window.hListBox, lb->view_offset + visible / 2);
}

void HandleScreenBottom(FlintContext* ctx) {
    if (!ctx) return;
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (!lb || !UI_IsListBoxSelected(lb)) return;
    int visible = UI_LB_GetVisibleCount();
    if (visible <= 0) visible = 1;
    int bottom = lb->view_offset + visible - 1;
    if (bottom > lb->populated_count - 1) bottom = lb->populated_count - 1;
    UI_NavigateTo(lb, ctx->window.hListBox, bottom);
}

static void vim_move_lines(int delta) {
    FlintContext* ctx = FlintContext_Get();
    if (!ctx) return;
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (!lb || !UI_IsListBoxSelected(lb)) return;
    int target = lb->selected_index + delta;
    if (target < 0) target = 0;
    if (target > lb->populated_count - 1) target = lb->populated_count - 1;
    UI_NavigateTo(lb, ctx->window.hListBox, target);
}

void HandleHalfPageDown(FlintContext* ctx) {
    (void)ctx;
    int step = PageUpDownLen > 1 ? PageUpDownLen / 2 : 1;
    vim_move_lines(step);
}

void HandleHalfPageUp(FlintContext* ctx) {
    (void)ctx;
    int step = PageUpDownLen > 1 ? PageUpDownLen / 2 : 1;
    vim_move_lines(-step);
}

void HandleScrollLineDown(FlintContext* ctx) {
    (void)ctx;
    vim_move_lines(1);
}

void HandleScrollLineUp(FlintContext* ctx) {
    (void)ctx;
    vim_move_lines(-1);
}

static void vim_mark_position(bool set) {
    FlintContext* ctx = FlintContext_Get();
    if (!ctx) return;
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    char name = vim_pending_mark;
    vim_pending_mark = 0;
    if (set) {
        vim_mark_set(name, Vim_GetCursor(ctx));
        return;
    }
    int mcursor = 0, msel = 0;
    if (!VimState_GetMark(name, &mcursor, &msel)) return;
    if (msel >= 0 && lb && msel < lb->populated_count) UI_NavigateTo(lb, ctx->window.hListBox, msel);
    vim_clamp_cursor(ctx, mcursor);
}

static void vim_mark_set_cb(int key) {
    vim_clear_pending();
    vim_pending_mark = (char)key;
    vim_mark_position(true);
}

static void vim_mark_jump_cb(int key) {
    vim_clear_pending();
    vim_pending_mark = (char)key;
    vim_mark_position(false);
}

void HandleSetMark(FlintContext* ctx) {
    (void)ctx;
    vim_show_pending("m");
    Platform_WaitForKeypressAsync(vim_mark_set_cb);
}

void HandleJumpToMark(FlintContext* ctx) {
    (void)ctx;
    vim_show_pending("`");
    Platform_WaitForKeypressAsync(vim_mark_jump_cb);
}

void HandleJumpPrevContext(FlintContext* ctx) {
    (void)ctx;
    FlintContext* fc = FlintContext_Get();
    if (!fc) return;
    int cursor = 0, sel = 0;
    if (!VimState_GetMark(VIM_MARK_PREV, &cursor, &sel)) return;
    ListBoxInfo* lb = Vim_GetListBox(fc);
    if (sel >= 0 && lb && sel < lb->populated_count) UI_NavigateTo(lb, fc->window.hListBox, sel);
    vim_clamp_cursor(fc, cursor);
}

static void vim_jump_history(bool backward) {
    FlintContext* ctx = FlintContext_Get();
    if (!ctx) return;
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    int cursor = 0, sel = 0;
    bool ok = backward ? VimState_PopJump(&cursor, &sel) : VimState_PeekJump(&cursor, &sel);
    if (!ok) return;
    if (sel >= 0 && lb && sel < lb->populated_count) UI_NavigateTo(lb, ctx->window.hListBox, sel);
    vim_clamp_cursor(ctx, cursor);
}

void HandleJumpBack(FlintContext* ctx) {
    (void)ctx;
    vim_jump_history(true);
}

void HandleJumpForward(FlintContext* ctx) {
    (void)ctx;
    vim_jump_history(false);
}

static void vim_run_search(bool backward) {
    FlintContext* ctx = FlintContext_Get();
    const char* pattern = NULL;
    VimState_EndSearchPattern(&pattern);
    if (!ctx || !pattern || pattern[0] == '\0') return;
    char text[VIM_TEXT_BUF];
    vim_get_text(ctx, text, sizeof(text));
    int pos = Vim_FindPatternPos(text, (int)strlen(text), Vim_GetCursor(ctx), pattern, backward);
    if (pos < 0) return;
    vim_clamp_cursor(ctx, pos);
    VimState_SetLastPattern(pattern);
}

static void vim_collect_cb(int key);
static void vim_collect_backward_cb(int key);

void HandleSearchForward(FlintContext* ctx) {
    (void)ctx;
    VimState_BeginSearchPattern();
    Platform_WaitForKeypressAsync(vim_collect_cb);
}

void HandleSearchBackward(FlintContext* ctx) {
    (void)ctx;
    VimState_BeginSearchPattern();
    Platform_WaitForKeypressAsync(vim_collect_backward_cb);
}

static void vim_collect_cb(int key) {
    if (key == 13 || key == 10) { vim_run_search(false); return; }
    if (key == 27) { Vim_SearchCancel(key); return; }
    VimState_AppendSearchPattern((char)key);
    Platform_WaitForKeypressAsync(vim_collect_cb);
}

static void vim_collect_backward_cb(int key) {
    if (key == 13 || key == 10) { vim_run_search(true); return; }
    if (key == 27) { Vim_SearchCancel(key); return; }
    VimState_AppendSearchPattern((char)key);
    Platform_WaitForKeypressAsync(vim_collect_backward_cb);
}

void Vim_SearchCollect(int key) {
    VimState_AppendSearchPattern((char)key);
}

void Vim_SearchCommitForward(int key) {
    (void)key;
    vim_run_search(false);
}

void Vim_SearchCommitBackward(int key) {
    (void)key;
    vim_run_search(true);
}

void Vim_SearchCancel(int key) {
    (void)key;
    const char* ignored = NULL;
    VimState_EndSearchPattern(&ignored);
}

