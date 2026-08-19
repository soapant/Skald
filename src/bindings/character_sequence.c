#include "common.h"
#include "bindings/character_sequence.h"
#include "core/common_ui.h"
#include <ctype.h>

static void CALLBACK SequenceTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    if (g_seq_state.waiting) {
        g_seq_state.waiting = false;
        g_seq_state.timed_out = true;
        printf("[SEQ-DEBUG] Timer expired! Sequence: '%s'\n", g_seq_state.keys);
    }
    g_seq_state.timer_id = 0;
}

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
    if (g_seq_state.timer_id != 0) {
        KillTimer(NULL, g_seq_state.timer_id);
        g_seq_state.timer_id = 0;
    }
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
        if (tolower(pattern[i]) != tolower(g_seq_state.keys[i])) {
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
        char pattern_char = tolower(pattern[i]);
        char seq_char = tolower(g_seq_state.keys[i]);
        if (pattern_char != seq_char) {
            return false;
        }
    }
    return true;
}

void SequenceStartTimer(void) {
    if (g_seq_state.timer_id != 0) {
        KillTimer(NULL, g_seq_state.timer_id);
    }
    g_seq_state.timer_id = SetTimer(NULL, 0, SEQUENCE_TIMEOUT_MS, SequenceTimerProc);
}

bool SequenceIsWaiting(void) {
    return g_seq_state.waiting;
}

bool SequenceTimedOut(void) {
    return g_seq_state.timed_out;
}

void DeleteLastTypedChar(void) {
    printf("[VIM-DEBUG] DeleteLastTypedChar: START\n");
    
    if (!g_ActiveEditControl || !IsWindow(g_ActiveEditControl)) {
        printf("[VIM-DEBUG] DeleteLastTypedChar: g_ActiveEditControl invalid!\n");
        return;
    }
    
    char text[512];
    UI_GetCurrentText(g_ActiveEditControl, text);
    int len = (int)strlen(text);
    
    DWORD start, end;
    UI_GetCurrentCursorPosition(&start, &end);
    int cursor_pos = (int)start;
    
    printf("[VIM-DEBUG] DeleteLastTypedChar: text='%s', len=%d, cursor_pos=%d\n", text, len, cursor_pos);
    
    if (cursor_pos == 0 || len == 0) {
        printf("[VIM-DEBUG] Nothing to delete (cursor at start or empty)\n");
        return;
    }
    
    int delete_pos = cursor_pos - 1;
    char deleted_char = text[delete_pos];
    
    printf("[VIM-DEBUG] Deleting character at position %d: '%c'\n", delete_pos, deleted_char);
    
    memmove(&text[delete_pos], &text[delete_pos + 1], len - delete_pos);
    UI_SetText(g_ActiveEditControl, text);
    
    UI_Edit_SetSel(g_ActiveEditControl, delete_pos, delete_pos);
    
    printf("[VIM-DEBUG] Deleted character '%c', new text='%s', cursor at %d\n", 
           deleted_char, text, delete_pos);
}
