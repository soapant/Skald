#include "core/vim.h"
#include "deps/log.h"
#include "core/context.h"
#include "core/modal.h"
#include "core/common_ui.h"
#include "core/window_manager.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#if defined(_WIN32)
#include <windows.h>
#endif

SequenceState seq_state = {0};

ListBoxInfo* Vim_GetListBox(FlintContext* ctx) {
    (void)ctx;
    WindowManager* wm = WindowManager_Get();
    if (!wm) return NULL;
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) return NULL;
    if (top->type == POPUP_PROMPT && top->user_data) {
        PromptState* s = (PromptState*)top->user_data;
        if (s && s->has_listbox) return &s->listbox;
    }
    return NULL;
}

VimContext Vim_FromGlobals(void) {
    VimContext ctx = {0};
    FlintContext* fc = FlintContext_Get();
    ctx.hListBox = fc->window.hListBox;
    ctx.hEdit = fc->window.g_ActiveEditControl;
    ctx.lb = Vim_GetListBox(fc);
    return ctx;
}

void Vim_GetText(FlintContext* ctx, char* buf, size_t cap) {
    if (!ctx || !buf || cap==0) return;
    strncpy(buf, ctx->window.filter, cap-1);
    buf[cap-1]='\0';
}

static void SyncFilterToActiveState(FlintContext* ctx) {
    WindowManager* wm = WindowManager_Get();
    if (!wm) return;
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top || !top->user_data) return;
    if (top->type == POPUP_PROMPT) {
        PromptState* s = (PromptState*)top->user_data;
        strncpy(s->filter, ctx->window.filter, sizeof(s->filter)-1);
        s->filter[sizeof(s->filter)-1]='\0';
        s->cursor = ctx->window.filter_cursor;
    } else if (top->type == POPUP_SIMPLE) {
        SimplePromptState* s = (SimplePromptState*)top->user_data;
        strncpy(s->user_input, ctx->window.filter, sizeof(s->user_input)-1);
        s->user_input[sizeof(s->user_input)-1]='\0';
        s->cursor = ctx->window.filter_cursor;
    }
}

void Vim_SetText(FlintContext* ctx, const char* text) {
    if (!ctx || !text) return;
    strncpy(ctx->window.filter, text, sizeof(ctx->window.filter)-1);
    ctx->window.filter[sizeof(ctx->window.filter)-1]='\0';
    ctx->window.filter_cursor = (int)strlen(ctx->window.filter);
    SyncFilterToActiveState(ctx);
    if (ctx->prompt_callbacks.refresh) {
        ListBoxInfo* lb = Vim_GetListBox(ctx);
        ctx->prompt_callbacks.refresh(ctx->window.filter, ctx->window.hListBox, lb);
    }
#if defined(_WIN32)
    if (WH_TO_HWND(ctx->window.hEditControl)) {
        SetWindowTextA(WH_TO_HWND(ctx->window.hEditControl), ctx->window.filter);
    }
    if (WH_TO_HWND(ctx->window.g_ActiveEditControl)) {
        SetWindowTextA(WH_TO_HWND(ctx->window.g_ActiveEditControl), ctx->window.filter);
    }
#endif
}

int Vim_GetCursor(FlintContext* ctx) {
    if (!ctx) return 0;
    int len = (int)strlen(ctx->window.filter);
    if (ctx->window.filter_cursor <0) return 0;
    if (ctx->window.filter_cursor > len) return len;
    return ctx->window.filter_cursor;
}

void Vim_SetCursor(FlintContext* ctx, int pos) {
    if (!ctx) return;
    int len = (int)strlen(ctx->window.filter);
    if (pos<0) pos=0;
    if (pos>len) pos=len;
    ctx->window.filter_cursor = pos;
    SyncFilterToActiveState(ctx);
#if defined(_WIN32)
    HWND hEdit = WH_TO_HWND(ctx->window.hEditControl);
    if (!hEdit) hEdit = WH_TO_HWND(ctx->window.g_ActiveEditControl);
    if (hEdit) {
        SendMessageA(hEdit, EM_SETSEL, (WPARAM)pos, (LPARAM)pos);
    }
#endif
}

#if defined(_WIN32)
static void CALLBACK SequenceTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    if (seq_state.waiting) {
        seq_state.waiting = false;
        seq_state.timed_out = true;
        log_info("Timer expired! Sequence: '%s'", seq_state.keys);
    }
    seq_state.timer_id = 0;
}
#endif

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
#if defined(_WIN32)
    if (seq_state.timer_id != 0) {
        KillTimer(NULL, seq_state.timer_id);
        seq_state.timer_id = 0;
    }
#endif
}

int SequenceLen(void) {
    return seq_state.len;
}

int SequenceTypedCount(void) {
    return seq_state.typed_count;
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
#if defined(_WIN32)
    if (seq_state.timer_id != 0) KillTimer(NULL, seq_state.timer_id);
    seq_state.timer_id = SetTimer(NULL, 0, SEQUENCE_TIMEOUT_MS, SequenceTimerProc);
#endif
}

bool SequenceIsWaiting(void) { return seq_state.waiting; }
bool SequenceTimedOut(void) { return seq_state.timed_out; }

void DeleteLastTypedChar(FlintContext* ctx) {
    if (!ctx) return;
    // Try to delete from active prompt state if it holds the typed char (for insert JK)
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (top && top->user_data && top->type == POPUP_PROMPT) {
        PromptState* s = (PromptState*)top->user_data;
        int len = (int)strlen(s->filter);
        int cur = s->cursor;
        if (cur > 0 && len > 0) {
            int del = cur - 1;
            memmove(s->filter + del, s->filter + del + 1, len - del);
            s->cursor = del;
            // sync to unified filter
            strncpy(ctx->window.filter, s->filter, sizeof(ctx->window.filter)-1);
            ctx->window.filter[sizeof(ctx->window.filter)-1]='\0';
            ctx->window.filter_cursor = s->cursor;
            if (ctx->prompt_callbacks.refresh) {
                ListBoxInfo* lb = Vim_GetListBox(ctx);
                ctx->prompt_callbacks.refresh(ctx->window.filter, ctx->window.hListBox, lb);
            }
#if defined(_WIN32)
            if (WH_TO_HWND(ctx->window.hEditControl)) SetWindowTextA(WH_TO_HWND(ctx->window.hEditControl), ctx->window.filter);
#endif
            return;
        }
    }
    if (top && top->user_data && top->type == POPUP_SIMPLE) {
        SimplePromptState* s = (SimplePromptState*)top->user_data;
        int len = (int)strlen(s->user_input);
        int cur = s->cursor;
        if (cur > 0 && len > 0) {
            int del = cur - 1;
            memmove(s->user_input + del, s->user_input + del + 1, len - del);
            s->cursor = del;
            strncpy(ctx->window.filter, s->user_input, sizeof(ctx->window.filter)-1);
            ctx->window.filter[sizeof(ctx->window.filter)-1]='\0';
            ctx->window.filter_cursor = s->cursor;
            return;
        }
    }
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
        ctx->prompt_callbacks.refresh(ctx->window.filter, ctx->window.hListBox, lb);
    }
#if defined(_WIN32)
    if (WH_TO_HWND(ctx->window.hEditControl)) SetWindowTextA(WH_TO_HWND(ctx->window.hEditControl), ctx->window.filter);
    if (WH_TO_HWND(ctx->window.g_ActiveEditControl)) SetWindowTextA(WH_TO_HWND(ctx->window.g_ActiveEditControl), ctx->window.filter);
#endif
}
