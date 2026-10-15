#include "core/vim.h"
#include "platform/platform.h"
#include "deps/log.h"
#include "core/context.h"
#include "platform/ui.h"
#include "core/window_manager.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
SequenceState seq_state = {0};



ListBoxInfo* Vim_GetListBox(FlintContext* ctx) {
      (void)ctx;
      WindowManager* wm = WindowManager_Get();
      if (wm) {
          Popup* top = WindowManager_GetTopPopup(wm);
          if (top) {
              if (top->type == POPUP_EDIT) return NULL;
              if (top->type == POPUP_PROMPT && top->user_data) {
                  PromptState* s = (PromptState*)top->user_data;
                  if (s && s->has_listbox) return &s->listbox;
              }
          }
      }
      return UI_GetPopupListBox(ctx);
  }

void Vim_GetText(FlintContext* ctx, char* buf, size_t cap) {
    if (!ctx || !buf || cap==0) return;
    UI_SyncFilterFromEdit(ctx);
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (top && top->type == POPUP_EDIT) {
        WMEditPopupData* e = (WMEditPopupData*)top;
        if (e->text) {
            strncpy(buf, e->text, cap-1);
            buf[cap-1]='\0';
            return;
        }
    }
    strncpy(buf, ctx->window.filter, cap-1);
    buf[cap-1]='\0';
}

static void SyncFilterToActiveState(FlintContext* ctx) {
    WindowManager* wm = WindowManager_Get();
    if (wm) {
        Popup* top = WindowManager_GetTopPopup(wm);
        if (top && top->user_data) {
            if (top->type == POPUP_PROMPT) {
                PromptState* s = (PromptState*)top->user_data;
                strncpy(s->filter, ctx->window.filter, sizeof(s->filter)-1);
                s->filter[sizeof(s->filter)-1]='\0';
                s->cursor = ctx->window.filter_cursor;
                return;
            }
        }
    }
}

void Vim_SetText(FlintContext* ctx, const char* text) {
    if (!ctx || !text) return;
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (top && top->type == POPUP_EDIT) {
        WMEditPopupData* e = (WMEditPopupData*)top;
        if (e->text && e->text_capacity > 1) {
            strncpy(e->text, text, e->text_capacity - 1);
            e->text[e->text_capacity - 1] = '\0';
            e->cursor = (int)strlen(e->text);
        }
        return;
    }
    strncpy(ctx->window.filter, text, sizeof(ctx->window.filter)-1);
    ctx->window.filter[sizeof(ctx->window.filter)-1]='\0';
    ctx->window.filter_cursor = (int)strlen(ctx->window.filter);
    SyncFilterToActiveState(ctx);
    if (ctx->prompt_callbacks.refresh) {
        ListBoxInfo* lb = Vim_GetListBox(ctx);
        ctx->prompt_callbacks.refresh(ctx->window.filter, lb);
    }
    UI_ApplyFilterToEdit(ctx);
}

int Vim_GetCursor(FlintContext* ctx) {
    if (!ctx) return 0;
    UI_SyncFilterFromEdit(ctx);
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (top && top->type == POPUP_EDIT) {
        WMEditPopupData* e = (WMEditPopupData*)top;
        if (e->text) {
            int len = (int)strlen(e->text);
            if (e->cursor < 0) return 0;
            if (e->cursor > len) return len;
            return e->cursor;
        }
    }
    int len = (int)strlen(ctx->window.filter);
    if (ctx->window.filter_cursor <0) return 0;
    if (ctx->window.filter_cursor > len) return len;
    return ctx->window.filter_cursor;
}

void Vim_SetCursor(FlintContext* ctx, int pos) {
    if (!ctx) return;
    UI_SyncFilterFromEdit(ctx);
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (top && top->type == POPUP_EDIT) {
        WMEditPopupData* e = (WMEditPopupData*)top;
        if (e->text) {
            int len = (int)strlen(e->text);
            if (pos < 0) pos = 0;
            if (pos > len) pos = len;
            e->cursor = pos;
        }
        return;
    }
    int len = (int)strlen(ctx->window.filter);
    if (pos<0) pos=0;
    if (pos>len) pos=len;
    ctx->window.filter_cursor = pos;
    SyncFilterToActiveState(ctx);
    UI_SyncEditCursor(ctx, pos);
}

static long long Sequence_NowMs(void) {
    return Platform_GetWallClockMs();
}

static void SequenceCheckExpiry(void) {
    if (seq_state.waiting && seq_state.deadline_ms != 0 && Sequence_NowMs() >= seq_state.deadline_ms) {
        seq_state.waiting = false;
        seq_state.timed_out = true;
        seq_state.deadline_ms = 0;
        log_info("Timer expired! Sequence: '%s'", seq_state.keys);
    }
}

void SequenceAddKey(char key, bool was_typed) {
    if (seq_state.len < MAX_SEQUENCE_LEN - 1) {
        seq_state.keys[seq_state.len++] = key;
        seq_state.keys[seq_state.len] = '\0';
        seq_state.waiting = true;
        seq_state.timed_out = false;
        if (was_typed) {
            seq_state.typed_count++;
        }
    }
}

void SequenceReset(void) {
    memset(seq_state.keys, 0, MAX_SEQUENCE_LEN);
    seq_state.len = 0;
    seq_state.typed_count = 0;
    seq_state.waiting = false;
    seq_state.timed_out = false;
    seq_state.deadline_ms = 0;
}

int SequenceTypedCount(void) {
    return seq_state.typed_count;
}

int SequenceLength(void) {
    return seq_state.len;
}

bool SequenceMatches(const char* pattern) {
    int seq_len = seq_state.len;
    int pat_len = strlen(pattern);
    if (seq_len != pat_len) return false;
    if (seq_len == 0) return false;
    for (int i = 0; i < seq_len; i++) {
        if (tolower((unsigned char)pattern[i]) != tolower((unsigned char)seq_state.keys[i])) {
            return false;
        }
    }
    return true;
}

bool SequenceStartsWith(const char* pattern) {
    int seq_len = seq_state.len;
    int pat_len = strlen(pattern);
    if (seq_len > pat_len) return false;
    if (seq_len == 0) return false;
    for (int i = 0; i < seq_len; i++) {
        char pattern_char = tolower((unsigned char)pattern[i]);
        char seq_char = tolower((unsigned char)seq_state.keys[i]);
        if (pattern_char != seq_char) return false;
    }
    return true;
}

void SequenceStartTimer(void) {
    seq_state.deadline_ms = Sequence_NowMs() + SEQUENCE_TIMEOUT_MS;
}

bool SequenceIsWaiting(void) {
    SequenceCheckExpiry();
    return seq_state.waiting;
}
bool SequenceTimedOut(void) {
    SequenceCheckExpiry();
    return seq_state.timed_out;
}

bool SequenceHasPendingWait(void) {
    return seq_state.waiting || seq_state.timed_out;
}

bool SequenceConsumeTimeout(void) {
    SequenceCheckExpiry();
    if (!seq_state.timed_out) return false;
    log_info("SequenceConsumeTimeout keys='%s'", seq_state.keys);
    SequenceReset();
    return true;
}

void DeleteLastTypedChar(FlintContext* ctx) {
    if (!ctx) return;
    UI_SyncFilterFromEdit(ctx);
    // Try to delete from active prompt state if it holds the typed char (for insert JK)
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (top && top->type == POPUP_EDIT) {
        WMEditPopupData* e = (WMEditPopupData*)top;
        if (e->text) {
            int elen = (int)strlen(e->text);
            int ecur = e->cursor;
            if (ecur > elen) ecur = elen;
            if (ecur > 0 && elen > 0) {
                memmove(e->text + ecur - 1, e->text + ecur, elen - ecur + 1);
                e->cursor = ecur - 1;
            }
        }
        return;
    }
    if (top && top->user_data && top->type == POPUP_PROMPT) {
        PromptState* s = (PromptState*)top->user_data;
        int len = (int)strlen(s->filter);
        int cur = s->cursor;
        if (cur > 0 && len > 0) {
            int del = cur - 1;
            memmove(s->filter + del, s->filter + del + 1, len - del);
            s->cursor = del;
            strncpy(ctx->window.filter, s->filter, sizeof(ctx->window.filter)-1);
            ctx->window.filter[sizeof(ctx->window.filter)-1]='\0';
            ctx->window.filter_cursor = s->cursor;
            if (ctx->prompt_callbacks.refresh) {
                ListBoxInfo* lb = Vim_GetListBox(ctx);
                ctx->prompt_callbacks.refresh(ctx->window.filter, lb);
            }
            UI_ApplyFilterToEdit(ctx);
            return;
        }
    }
    if (UI_ApplyBackspaceToPopup(ctx)) return;
    char text[512];
    Vim_GetText(ctx, text, sizeof(text));
    int len = (int)strlen(text);
    int cursor_pos = Vim_GetCursor(ctx);
    if (cursor_pos == 0 || len == 0) return;
    int delete_pos = cursor_pos - 1;
    memmove(&text[delete_pos], &text[delete_pos + 1], len - delete_pos);
    strncpy(ctx->window.filter, text, sizeof(ctx->window.filter)-1);
    ctx->window.filter[sizeof(ctx->window.filter)-1]='\0';
    ctx->window.filter_cursor = delete_pos;
    SyncFilterToActiveState(ctx);
    if (ctx->prompt_callbacks.refresh) {
        ListBoxInfo* lb = Vim_GetListBox(ctx);
        ctx->prompt_callbacks.refresh(ctx->window.filter, lb);
    }
    UI_ApplyFilterToEdit(ctx);
}
