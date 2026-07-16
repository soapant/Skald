// character_sequence.h - Add timeout handling
#ifndef CHARACTER_SEQUENCE_H
#define CHARACTER_SEQUENCE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_SEQUENCE_LEN 8
#define SEQUENCE_TIMEOUT_MS 300

typedef struct {
    char keys[MAX_SEQUENCE_LEN];
    int len;
    int typed_count; 
    UINT timer_id;
    bool waiting;
    bool timed_out;
} SequenceState;

static SequenceState g_seq_state = {0};

static inline void SequenceAddKey(char key, bool was_typed) {
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

static inline void SequenceReset(void) {
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

static inline int SequenceTypedCount(void) {
    return g_seq_state.typed_count;
}

static inline const char* SequenceGet(void) {
    return g_seq_state.keys;
}

static inline int SequenceLen(void) {
    return g_seq_state.len;
}

static inline bool SequenceMatches(const char* pattern) {
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

static inline bool SequenceStartsWith(const char* pattern) {
    int seq_len = g_seq_state.len;
    int pat_len = strlen(pattern);
    
    if (seq_len > pat_len) return false;
    if (seq_len == 0) return false;
    
    for (int i = 0; i < seq_len; i++) {
        if (tolower(pattern[i]) != tolower(g_seq_state.keys[i])) {
            return false;
        }
    }
    return true;
}

static inline void CALLBACK SequenceTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    if (g_seq_state.waiting) {
        g_seq_state.waiting = false;
        g_seq_state.timed_out = true;
    }
    g_seq_state.timer_id = 0;
}

static inline void SequenceStartTimer(void) {
    if (g_seq_state.timer_id != 0) {
        KillTimer(NULL, g_seq_state.timer_id);
    }
    g_seq_state.timer_id = SetTimer(NULL, 0, SEQUENCE_TIMEOUT_MS, SequenceTimerProc);
}

static inline bool SequenceIsWaiting(void) {
    return g_seq_state.waiting;
}

static inline bool SequenceTimedOut(void) {
    bool result = g_seq_state.timed_out;
    if (result) {
        g_seq_state.timed_out = false;  // Clear after check
    }
    return result;
}

#endif
