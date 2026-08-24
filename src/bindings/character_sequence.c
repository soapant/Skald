#include "common.h"
#include "bindings/character_sequence.h"
#include "core/common_ui.h"
#include "core/vim_view.h"
#include <ctype.h>

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
    VimView* v = VimView_GetActive();
    if (v) { VimView_SeqAddKey(v, key, was_typed); return; }
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
    VimView* v = VimView_GetActive();
    if (v) { VimView_SeqReset(v); return; }
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
    VimView* v = VimView_GetActive();
    if (v) return VimView_SeqLen(v);
    return g_seq_state.len;
}

int SequenceTypedCount(void) {
    VimView* v = VimView_GetActive();
    if (v) return VimView_SeqTypedCount(v);
    return g_seq_state.typed_count;
}

bool SequenceMatches(const char* pattern) {
    VimView* v = VimView_GetActive();
    if (v) return VimView_SeqMatches(v, pattern);
    int seq_len = g_seq_state.len;
    int pat_len = strlen(pattern);
    
    if (seq_len != pat_len) return false;
    if (seq_len == 0) return false;
    
    for (int i = 0; i < seq_len; i++) {
        if (tolower(pattern[i]) != tolower(g_seq_state.keys[i])) {
            return false;
        }
    }
    return true;
}

bool SequenceStartsWith(const char* pattern) {
    VimView* v = VimView_GetActive();
    if (v) return VimView_SeqStartsWith(v, pattern);
    int seq_len = g_seq_state.len;
    int pat_len = strlen(pattern);
    
    if (seq_len > pat_len) {
        return false;
    }
    if (seq_len == 0) {
        return false;
    }
    
    for (int i = 0; i < seq_len; i++) {
        char pattern_char = tolower(pattern[i]);
        char seq_char = tolower(g_seq_state.keys[i]);
        if (pattern_char != seq_char) {
            return false;
        }
    }
    return true;
}

void SequenceStartTimer(void) {
    VimView* v = VimView_GetActive();
    if (v) { VimView_SeqStartTimer(v); return; }
#if defined(_WIN32)
    if (g_seq_state.timer_id != 0) {
        KillTimer(NULL, g_seq_state.timer_id);
    }
    g_seq_state.timer_id = SetTimer(NULL, 0, SEQUENCE_TIMEOUT_MS, SequenceTimerProc);
#endif
}

bool SequenceIsWaiting(void) {
    VimView* v = VimView_GetActive();
    if (v) return VimView_SeqIsWaiting(v);
    return g_seq_state.waiting;
}

bool SequenceTimedOut(void) {
    VimView* v = VimView_GetActive();
    if (v) return VimView_SeqTimedOut(v);
    return g_seq_state.timed_out;
}

void DeleteLastTypedChar(VimContext* ctx) {
    printf("[VIM-DEBUG] DeleteLastTypedChar: START\n");
    if (!ctx) {
        printf("[VIM-DEBUG] DeleteLastTypedChar: ctx null!\n");
        return;
    }
    char text[512];
    VimCtx_GetText(ctx, text, sizeof(text));
    int len = (int)strlen(text);
    int cursor_pos = VimCtx_GetCursor(ctx);
    printf("[VIM-DEBUG] DeleteLastTypedChar: text='%s', len=%d, cursor_pos=%d\n", text, len, cursor_pos);
    if (cursor_pos == 0 || len == 0) {
        printf("[VIM-DEBUG] Nothing to delete (cursor at start or empty)\n");
        return;
    }
    int delete_pos = cursor_pos - 1;
    char deleted_char = text[delete_pos];
    printf("[VIM-DEBUG] Deleting character at position %d: '%c'\n", delete_pos, deleted_char);
    memmove(&text[delete_pos], &text[delete_pos + 1], len - delete_pos);
    VimCtx_SetText(ctx, text);
    VimCtx_SetCursor(ctx, delete_pos);
    printf("[VIM-DEBUG] Deleted character '%c', new text='%s', cursor at %d\n", 
           deleted_char, text, delete_pos);
}