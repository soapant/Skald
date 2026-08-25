#include "core/vim.h"
#include "common.h"
#include "core/common_ui.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#if defined(_WIN32)
#include <windows.h>
#endif

// ---------------------------------------------------------------------------
// VimContext: text/cursor access for the active surface
// ---------------------------------------------------------------------------

void Vim_GetText(VimContext* ctx, char* buf, size_t cap) {
    if (!ctx || !buf || cap==0) return;
    buf[0]='\0';
    if (ctx->edit_state) {
        strncpy(buf, ctx->edit_state->user_input, cap-1);
        buf[cap-1]='\0';
    } else if (ctx->state) {
        strncpy(buf, ctx->state->filter, cap-1);
        buf[cap-1]='\0';
    } else if (ctx->simple_state) {
        strncpy(buf, ctx->simple_state->user_input, cap-1);
        buf[cap-1]='\0';
    } else {
#if defined(_WIN32)
        if (WH_TO_HWND(ctx->hEdit)) GetWindowTextA(WH_TO_HWND(ctx->hEdit), buf, (int)cap);
#else
        UI_GetCurrentText(ctx->hEdit, buf);
#endif
    }
}

void Vim_SetText(VimContext* ctx, const char* text) {
    if (!ctx || !text) return;
    if (ctx->edit_state) {
        strncpy(ctx->edit_state->user_input, text, sizeof(ctx->edit_state->user_input)-1);
        ctx->edit_state->user_input[sizeof(ctx->edit_state->user_input)-1]='\0';
        ctx->edit_state->cursor = strlen(ctx->edit_state->user_input);
    } else if (ctx->state) {
        strncpy(ctx->state->filter, text, sizeof(ctx->state->filter)-1);
        ctx->state->filter[sizeof(ctx->state->filter)-1]='\0';
        ctx->state->cursor = (int)strlen(ctx->state->filter);
        if (g_refresh_callback) g_refresh_callback(ctx->state->filter, ctx->hListBox, ctx->lb);
    } else if (ctx->simple_state) {
        strncpy(ctx->simple_state->user_input, text, sizeof(ctx->simple_state->user_input)-1);
        ctx->simple_state->user_input[sizeof(ctx->simple_state->user_input)-1]='\0';
        ctx->simple_state->cursor = (int)strlen(ctx->simple_state->user_input);
    } else {
        UI_SetText(ctx->hEdit, (char*)text);
    }
}

int Vim_GetCursor(VimContext* ctx) {
    if (!ctx) return 0;
    if (ctx->edit_state) return (int)ctx->edit_state->cursor;
    if (ctx->state) return ctx->state->cursor;
    if (ctx->simple_state) return ctx->simple_state->cursor;
    uint32_t s=0,e=0;
    UI_Edit_GetSel(ctx->hEdit, &s, &e);
    return (int)s;
}

void Vim_SetCursor(VimContext* ctx, int pos) {
    if (!ctx) return;
    if (ctx->edit_state) {
        int len = (int)strlen(ctx->edit_state->user_input);
        if (pos<0) pos=0;
        if (pos>len) pos=len;
        ctx->edit_state->cursor = pos;
    } else if (ctx->state) {
        int len = (int)strlen(ctx->state->filter);
        if (pos<0) pos=0;
        if (pos>len) pos=len;
        ctx->state->cursor = pos;
    } else if (ctx->simple_state) {
        int len = (int)strlen(ctx->simple_state->user_input);
        if (pos<0) pos=0;
        if (pos>len) pos=len;
        ctx->simple_state->cursor = pos;
    } else {
        UI_SetCursorPosition(ctx->hEdit, pos);
    }
}

VimContext Vim_FromGlobals(void) {
    VimContext ctx = {0};
#if defined(_WIN32)
    ctx.hEdit = g_ActiveEditControl;
    ctx.hListBox = hListBox;
    if (WH_TO_HWND(hPopupWnd) && IsWindow(WH_TO_HWND(hPopupWnd))) {
        ctx.lb = (ListBoxInfo*)GetWindowLongPtrA(WH_TO_HWND(hPopupWnd), GWLP_USERDATA);
    }
    ctx.state = NULL;
    ctx.simple_state = NULL;
    if (!ctx.lb) {
        extern PromptState* g_currentPromptState;
        extern SimplePromptState* g_currentSimplePromptState;
        if (g_currentPromptState) {
            ctx.lb = g_currentPromptState->has_listbox ? &g_currentPromptState->listbox : NULL;
            ctx.state = g_currentPromptState;
        } else if (g_currentSimplePromptState) {
            ctx.simple_state = g_currentSimplePromptState;
            ctx.lb = NULL;
        }
    }
#else
    extern PromptState* g_currentPromptState;
    extern SimplePromptState* g_currentSimplePromptState;
    extern SimplePromptState* g_currentEditPopupState;
    if (g_currentEditPopupState) {
        return Vim_FromEditState(g_currentEditPopupState);
    }
    ctx.state = g_currentPromptState;
    ctx.simple_state = g_currentSimplePromptState;
    if (ctx.state) {
        ctx.lb = ctx.state->has_listbox ? &ctx.state->listbox : NULL;
        ctx.hListBox = hListBox;
        ctx.hEdit = g_ActiveEditControl;
    } else if (ctx.simple_state) {
        ctx.lb = NULL;
        ctx.hListBox = hListBox;
        ctx.hEdit = g_ActiveEditControl;
    } else {
        ctx.lb = NULL;
        ctx.hListBox = hListBox;
        ctx.hEdit = g_ActiveEditControl;
    }
#endif
    return ctx;
}

VimContext Vim_FromState(PromptState* state) {
    VimContext ctx = {0};
    ctx.state = state;
    if (state) {
        ctx.lb = state->has_listbox ? &state->listbox : NULL;
        ctx.hListBox = hListBox;
        ctx.hEdit = g_ActiveEditControl;
    }
    return ctx;
}

VimContext Vim_FromSimpleState(SimplePromptState* state) {
    VimContext ctx = {0};
    ctx.simple_state = state;
    ctx.lb = NULL;
    ctx.hListBox = hListBox;
    ctx.hEdit = g_ActiveEditControl;
    return ctx;
}

VimContext Vim_FromEditState(SimplePromptState* state) {
    VimContext ctx = {0};
    ctx.edit_state = state;
    ctx.lb = NULL;
    ctx.hListBox = hListBox;
    ctx.hEdit = g_ActiveEditControl;
    return ctx;
}

// ---------------------------------------------------------------------------
// Key sequences
// ---------------------------------------------------------------------------

#if defined(_WIN32)
static void CALLBACK SequenceTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    if (g_seq_state.waiting) {
        g_seq_state.waiting = false;
        g_seq_state.timed_out = true;
        printf("[SEQ-DEBUG] Timer expired! Sequence: '%s'\n", g_seq_state.keys);
    }
    g_seq_state.timer_id = 0;
}
#endif

void SequenceAddKey(char key, bool was_typed) {
    if (g_seq_state.len < MAX_SEQUENCE_LEN - 1) {
        g_seq_state.keys[g_seq_state.len++] = key;
        g_seq_state.keys[g_seq_state.len] = '\0';
        g_seq_state.waiting = true;
        g_seq_state.timed_out = false;
        if (was_typed) {
            g_seq_state.typed_count++;
        }
    }
}

void SequenceReset(void) {
    memset(g_seq_state.keys, 0, MAX_SEQUENCE_LEN);
    g_seq_state.len = 0;
    g_seq_state.typed_count = 0;
    g_seq_state.waiting = false;
    g_seq_state.timed_out = false;
#if defined(_WIN32)
    if (g_seq_state.timer_id != 0) {
        KillTimer(NULL, g_seq_state.timer_id);
        g_seq_state.timer_id = 0;
    }
#endif
}

int SequenceLen(void) {
    return g_seq_state.len;
}

int SequenceTypedCount(void) {
    return g_seq_state.typed_count;
}

bool SequenceMatches(const char* pattern) {
    int seq_len = g_seq_state.len;
    int pat_len = strlen(pattern);

    if (seq_len != pat_len) return false;
    if (seq_len == 0) return false;

    for (int i = 0; i < seq_len; i++) {
        if (tolower((unsigned char)pattern[i]) != tolower((unsigned char)g_seq_state.keys[i])) {
            return false;
        }
    }
    return true;
}

bool SequenceStartsWith(const char* pattern) {
    int seq_len = g_seq_state.len;
    int pat_len = strlen(pattern);

    if (seq_len > pat_len) {
        return false;
    }
    if (seq_len == 0) {
        return false;
    }

    for (int i = 0; i < seq_len; i++) {
        char pattern_char = tolower((unsigned char)pattern[i]);
        char seq_char = tolower((unsigned char)g_seq_state.keys[i]);
        if (pattern_char != seq_char) {
            return false;
        }
    }
    return true;
}

void SequenceStartTimer(void) {
#if defined(_WIN32)
    if (g_seq_state.timer_id != 0) {
        KillTimer(NULL, g_seq_state.timer_id);
    }
    g_seq_state.timer_id = SetTimer(NULL, 0, SEQUENCE_TIMEOUT_MS, SequenceTimerProc);
#endif
}

bool SequenceIsWaiting(void) {
    return g_seq_state.waiting;
}

bool SequenceTimedOut(void) {
    return g_seq_state.timed_out;
}

void DeleteLastTypedChar(VimContext* ctx) {
    printf("[VIM-DEBUG] DeleteLastTypedChar: START\n");
    if (!ctx) {
        printf("[VIM-DEBUG] DeleteLastTypedChar: ctx null!\n");
        return;
    }
    char text[512];
    Vim_GetText(ctx, text, sizeof(text));
    int len = (int)strlen(text);
    int cursor_pos = Vim_GetCursor(ctx);
    printf("[VIM-DEBUG] DeleteLastTypedChar: text='%s', len=%d, cursor_pos=%d\n", text, len, cursor_pos);
    if (cursor_pos == 0 || len == 0) {
        printf("[VIM-DEBUG] Nothing to delete (cursor at start or empty)\n");
        return;
    }
    int delete_pos = cursor_pos - 1;
    char deleted_char = text[delete_pos];
    printf("[VIM-DEBUG] Deleting character at position %d: '%c'\n", delete_pos, deleted_char);
    memmove(&text[delete_pos], &text[delete_pos + 1], len - delete_pos);
    Vim_SetText(ctx, text);
    Vim_SetCursor(ctx, delete_pos);
    printf("[VIM-DEBUG] Deleted character '%c', new text='%s', cursor at %d\n",
           deleted_char, text, delete_pos);
}
