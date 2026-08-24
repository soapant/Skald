#include "core/vim_view.h"
#include "common.h"
#include "core/modal.h"
#include "core/common_ui.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#if !defined(_WIN32)
#include <sys/time.h>
#endif

// Active view singleton - only one modal prompt is active at once,
// but each view retains its own mode/seq/lb when inactive.
static VimView* g_activeVimView = NULL;

// Dynamic hotkey registry for per-file FEATURE_HOTKEY macro
#define MAX_DYNAMIC_HOTKEYS 64
typedef struct {
    int vk;
    int mod;
    void (*handler)(void);
    const char* name;
    FeatureEnabledFn enabled;
    bool used;
} DynamicHotkey;
static DynamicHotkey g_dynamicHotkeys[MAX_DYNAMIC_HOTKEYS];
static int g_dynamicHotkeyCount = 0;

static const char* g_mode_names[] = { "NORMAL", "INSERT", "VISUAL" };

VimView* VimView_Create(bool has_listbox, const char* title, int width, int height) {
    VimView* v = (VimView*)calloc(1, sizeof(VimView));
    if (!v) return NULL;
    v->has_listbox = has_listbox;
    v->window_title = title ? strdup(title) : NULL;
    v->width = width;
    v->height = height;
    v->cursor = 0;
    v->scroll_offset = 0;
    v->text[0] = '\0';
    v->mode = MODE_INSERT;
    v->seq.len = 0;
    v->seq.typed_count = 0;
    v->seq.waiting = false;
    v->seq.timed_out = false;
    v->seq.timer_id = 0;
    v->seq.keys[0] = '\0';
    v->seq_timing = false;
    v->last_index = 0;
    if (has_listbox) {
        ListBoxInfo_Init(&v->lb);
        v->lb.selected_index = -1;
    }
    return v;
}

void VimView_Destroy(VimView* v) {
    if (!v) return;
    if (v->window_title) free(v->window_title);
    free(v);
}

void VimView_Reset(VimView* v) {
    if (!v) return;
    v->text[0] = '\0';
    v->cursor = 0;
    v->scroll_offset = 0;
    ListBoxInfo_Init(&v->lb);
    v->mode = MODE_INSERT;
    memset(&v->seq, 0, sizeof(v->seq));
    v->seq_timing = false;
}

void VimView_SetActive(VimView* v) {
    g_activeVimView = v;
    VimView_SyncGlobals();
}

VimView* VimView_GetActive(void) {
    return g_activeVimView;
}

void VimView_ClearActive(void) {
    g_activeVimView = NULL;
    // don't clear globals - leave last mode for ModePersistsThroughFeatures
}

bool VimView_HasActive(void) {
    return g_activeVimView != NULL;
}

void VimView_SyncGlobals(void) {
    if (!g_activeVimView) return;
    VimView* v = g_activeVimView;
    // Sync legacy globals so existing rendering code that reads g_current_mode/g_vim_mode keeps working
    g_current_mode = v->mode;
    g_vim_mode = g_mode_names[v->mode];
    g_seq_state = v->seq;
    // Also sync hListBox dummy? Not needed on Linux, but keep for compat
}

EditorMode VimView_GetMode(VimView* v) {
    if (!v) v = g_activeVimView;
    if (v) return v->mode;
    return g_current_mode;
}

const char* VimView_GetModeName(VimView* v) {
    EditorMode m = VimView_GetMode(v);
    return g_mode_names[m];
}

void VimView_SetMode(VimView* v, EditorMode mode) {
    if (!v) v = g_activeVimView;
    if (!v) {
        // fallback to global if no active view (headless / Windows path)
        extern void SetModeWithListBox(EditorMode mode, ListBoxInfo* lb);
        SetModeWithListBox(mode, NULL);
        return;
    }
    v->mode = mode;
    // keep globals in sync
    g_current_mode = mode;
    g_vim_mode = g_mode_names[mode];
    v->seq.waiting = false;
    v->seq.timed_out = false;
    g_seq_state = v->seq;

    // Mirror SetModeWithListBox listbox selection logic but per-view
    bool in_main = true;
#if defined(_WIN32)
    in_main = (WH_TO_HWND(g_hSearchPopup) == NULL && WH_TO_HWND(g_hEditPopup) == NULL);
#else
    extern SimplePromptState* g_currentEditPopupState;
    in_main = (g_currentEditPopupState == NULL);
#endif
    // For prompt windows, use the PromptState's listbox (source of truth for rendering)
    // For custom VimView windows, use view's own lb
    ListBoxInfo* lb = NULL;
    extern PromptState* g_currentPromptState;
    if (g_currentPromptState && g_currentPromptState->has_listbox) lb = &g_currentPromptState->listbox;
    else if (v->has_listbox) lb = &v->lb;

    if (in_main && lb) {
        extern bool RememberLastIndex;
        // Use per-view last_index for isolation, fallback to global if needed
        int* p_last = &v->last_index;
        if (mode == MODE_INSERT) {
            if (RememberLastIndex) *p_last = lb->selected_index;
            UI_SetSelection(lb, hListBox, -1);
        } else if (mode == MODE_NORMAL) {
            if (*p_last < lb->total_matches && RememberLastIndex) {
                UI_NavigateTo(lb, hListBox, *p_last);
            } else {
                UI_SetSelection(lb, hListBox, 0);
            }
        }
        // Keep global last_index in sync for legacy code that reads it
        extern int last_index;
        last_index = *p_last;
    }
    if (WH_TO_HWND(g_ActiveEditControl) != NULL) {
        UpdateTextCaret(g_ActiveEditControl);
    }
}

bool VimView_IsNormalMode(VimView* v) { return VimView_GetMode(v) == MODE_NORMAL; }
bool VimView_IsInsertMode(VimView* v) { return VimView_GetMode(v) == MODE_INSERT; }
bool VimView_IsVisualMode(VimView* v) { return VimView_GetMode(v) == MODE_VISUAL; }

// Sequence per-view
void VimView_SeqAddKey(VimView* v, char key, bool was_typed) {
    if (!v) v = g_activeVimView;
    if (!v) {
        // fallback to global
        extern void SequenceAddKey(char key, bool was_typed);
        SequenceAddKey(key, was_typed);
        return;
    }
    if (v->seq.len < MAX_SEQUENCE_LEN - 1) {
        v->seq.keys[v->seq.len++] = key;
        v->seq.keys[v->seq.len] = '\0';
        v->seq.waiting = true;
        v->seq.timed_out = false;
        if (was_typed) v->seq.typed_count++;
        g_seq_state = v->seq;
        clock_gettime(CLOCK_MONOTONIC, &v->seq_start);
        v->seq_timing = true;
    }
}

void VimView_SeqReset(VimView* v) {
    if (!v) v = g_activeVimView;
    if (!v) {
        extern void SequenceReset(void);
        SequenceReset();
        return;
    }
    memset(v->seq.keys, 0, MAX_SEQUENCE_LEN);
    v->seq.len = 0;
    v->seq.typed_count = 0;
    v->seq.waiting = false;
    v->seq.timed_out = false;
    v->seq.timer_id = 0;
    v->seq_timing = false;
    g_seq_state = v->seq;
#if defined(_WIN32)
    if (v->seq.timer_id != 0) {
        KillTimer(NULL, v->seq.timer_id);
        v->seq.timer_id = 0;
    }
#endif
}

int VimView_SeqLen(VimView* v) {
    if (!v) v = g_activeVimView;
    if (v) return v->seq.len;
    extern int SequenceLen(void);
    return SequenceLen();
}

int VimView_SeqTypedCount(VimView* v) {
    if (!v) v = g_activeVimView;
    if (v) return v->seq.typed_count;
    extern int SequenceTypedCount(void);
    return SequenceTypedCount();
}

bool VimView_SeqMatches(VimView* v, const char* pattern) {
    if (!v) v = g_activeVimView;
    if (v) {
        int seq_len = v->seq.len;
        int pat_len = strlen(pattern);
        if (seq_len != pat_len) return false;
        if (seq_len == 0) return false;
        for (int i = 0; i < seq_len; i++) {
            if (tolower((unsigned char)pattern[i]) != tolower((unsigned char)v->seq.keys[i])) return false;
        }
        return true;
    }
    extern bool SequenceMatches(const char* pattern);
    return SequenceMatches(pattern);
}

bool VimView_SeqStartsWith(VimView* v, const char* pattern) {
    if (!v) v = g_activeVimView;
    if (v) {
        int seq_len = v->seq.len;
        int pat_len = strlen(pattern);
        if (seq_len > pat_len) return false;
        if (seq_len == 0) return false;
        for (int i = 0; i < seq_len; i++) {
            char pc = tolower((unsigned char)pattern[i]);
            char sc = tolower((unsigned char)v->seq.keys[i]);
            if (pc != sc) return false;
        }
        return true;
    }
    extern bool SequenceStartsWith(const char* pattern);
    return SequenceStartsWith(pattern);
}

void VimView_SeqStartTimer(VimView* v) {
    if (!v) v = g_activeVimView;
    if (v) {
#if defined(_WIN32)
        if (v->seq.timer_id != 0) KillTimer(NULL, v->seq.timer_id);
        // On Linux we use monotonic time, not Win32 timer
        // Keep timer_id 0 for Linux, rely on VimView_SeqTimedOut check
        clock_gettime(CLOCK_MONOTONIC, &v->seq_start);
        v->seq_timing = true;
        g_seq_state = v->seq;
#else
        clock_gettime(CLOCK_MONOTONIC, &v->seq_start);
        v->seq_timing = true;
        g_seq_state = v->seq;
#endif
        return;
    }
    extern void SequenceStartTimer(void);
    SequenceStartTimer();
}

bool VimView_SeqIsWaiting(VimView* v) {
    if (!v) v = g_activeVimView;
    if (v) {
        VimView_SeqUpdateTimeout(v);
        return v->seq.waiting;
    }
    extern bool SequenceIsWaiting(void);
    return SequenceIsWaiting();
}

bool VimView_SeqTimedOut(VimView* v) {
    if (!v) v = g_activeVimView;
    if (v) {
        VimView_SeqUpdateTimeout(v);
        return v->seq.timed_out;
    }
    extern bool SequenceTimedOut(void);
    return SequenceTimedOut();
}

void VimView_SeqUpdateTimeout(VimView* v) {
    if (!v) v = g_activeVimView;
    if (!v || !v->seq_timing || !v->seq.waiting) return;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long elapsed_ms = (now.tv_sec - v->seq_start.tv_sec) * 1000 + (now.tv_nsec - v->seq_start.tv_nsec) / 1000000;
    if (elapsed_ms > SEQUENCE_TIMEOUT_MS) {
        v->seq.waiting = false;
        v->seq.timed_out = true;
        v->seq_timing = false;
        g_seq_state = v->seq;
    }
}

void VimView_GetText(VimView* v, char* buf, size_t cap) {
    if (!v) v = g_activeVimView;
    if (!buf || cap==0) return;
    buf[0]='\0';
    // If a PromptState is active, that is the source of truth for prompt windows
    extern PromptState* g_currentPromptState;
    extern SimplePromptState* g_currentSimplePromptState;
    extern SimplePromptState* g_currentEditPopupState;
    if (g_currentEditPopupState) {
        strncpy(buf, g_currentEditPopupState->user_input, cap-1);
        buf[cap-1]='\0';
        return;
    }
    if (g_currentPromptState) {
        strncpy(buf, g_currentPromptState->filter, cap-1);
        buf[cap-1]='\0';
        return;
    }
    if (g_currentSimplePromptState) {
        strncpy(buf, g_currentSimplePromptState->user_input, cap-1);
        buf[cap-1]='\0';
        return;
    }
    if (!v) { buf[0]='\0'; return; }
    strncpy(buf, v->text, cap-1);
    buf[cap-1] = '\0';
}

void VimView_SetText(VimView* v, const char* text) {
    if (!v) v = g_activeVimView;
    if (!text) return;
    extern PromptState* g_currentPromptState;
    extern SimplePromptState* g_currentSimplePromptState;
    extern SimplePromptState* g_currentEditPopupState;
    if (g_currentEditPopupState) {
        strncpy(g_currentEditPopupState->user_input, text, sizeof(g_currentEditPopupState->user_input)-1);
        g_currentEditPopupState->user_input[sizeof(g_currentEditPopupState->user_input)-1]='\0';
        g_currentEditPopupState->cursor = strlen(g_currentEditPopupState->user_input);
        if (v) g_seq_state = v->seq;
        return;
    }
    if (g_currentPromptState) {
        strncpy(g_currentPromptState->filter, text, sizeof(g_currentPromptState->filter)-1);
        g_currentPromptState->filter[sizeof(g_currentPromptState->filter)-1]='\0';
        g_currentPromptState->cursor = (int)strlen(g_currentPromptState->filter);
        if (g_refresh_callback) g_refresh_callback(g_currentPromptState->filter, hListBox, g_currentPromptState->has_listbox ? &g_currentPromptState->listbox : NULL);
        if (v) g_seq_state = v->seq;
        // also keep view text in sync for consistency
        if (v) { strncpy(v->text, text, sizeof(v->text)-1); v->text[sizeof(v->text)-1]='\0'; v->cursor = g_currentPromptState->cursor; }
        return;
    }
    if (g_currentSimplePromptState) {
        strncpy(g_currentSimplePromptState->user_input, text, sizeof(g_currentSimplePromptState->user_input)-1);
        g_currentSimplePromptState->user_input[sizeof(g_currentSimplePromptState->user_input)-1]='\0';
        g_currentSimplePromptState->cursor = (int)strlen(g_currentSimplePromptState->user_input);
        if (v) { strncpy(v->text, text, sizeof(v->text)-1); v->text[sizeof(v->text)-1]='\0'; v->cursor = g_currentSimplePromptState->cursor; g_seq_state = v->seq; }
        return;
    }
    if (!v) return;
    strncpy(v->text, text, sizeof(v->text)-1);
    v->text[sizeof(v->text)-1] = '\0';
    v->cursor = strlen(v->text);
    // trigger refresh if has listbox and callback set (for custom VimView windows)
    if (v->has_listbox && g_refresh_callback) {
        g_refresh_callback(v->text, hListBox, &v->lb);
    }
    g_seq_state = v->seq;
}

int VimView_GetCursor(VimView* v) {
    extern PromptState* g_currentPromptState;
    extern SimplePromptState* g_currentSimplePromptState;
    extern SimplePromptState* g_currentEditPopupState;
    if (g_currentEditPopupState) return (int)g_currentEditPopupState->cursor;
    if (g_currentPromptState) return g_currentPromptState->cursor;
    if (g_currentSimplePromptState) return g_currentSimplePromptState->cursor;
    if (!v) v = g_activeVimView;
    if (!v) return 0;
    return v->cursor;
}

void VimView_SetCursor(VimView* v, int pos) {
    extern PromptState* g_currentPromptState;
    extern SimplePromptState* g_currentSimplePromptState;
    extern SimplePromptState* g_currentEditPopupState;
    if (g_currentEditPopupState) {
        int len = (int)strlen(g_currentEditPopupState->user_input);
        if (pos<0) pos=0;
        if (pos>len) pos=len;
        g_currentEditPopupState->cursor = pos;
        if (v) v->cursor = pos;
        return;
    }
    if (g_currentPromptState) {
        int len = (int)strlen(g_currentPromptState->filter);
        if (pos<0) pos=0;
        if (pos>len) pos=len;
        g_currentPromptState->cursor = pos;
        if (v) v->cursor = pos;
        return;
    }
    if (g_currentSimplePromptState) {
        int len = (int)strlen(g_currentSimplePromptState->user_input);
        if (pos<0) pos=0;
        if (pos>len) pos=len;
        g_currentSimplePromptState->cursor = pos;
        if (v) v->cursor = pos;
        return;
    }
    if (!v) v = g_activeVimView;
    if (!v) return;
    int len = strlen(v->text);
    if (pos < 0) pos = 0;
    if (pos > len) pos = len;
    v->cursor = pos;
}

// Hotkey registry
void Feature_RegisterHotkey(int vk, int mod, void (*handler)(void), const char* name, FeatureEnabledFn enabled) {
    if (g_dynamicHotkeyCount >= MAX_DYNAMIC_HOTKEYS) {
        fprintf(stderr, "[FEATURE] Too many dynamic hotkeys\n");
        return;
    }
    g_dynamicHotkeys[g_dynamicHotkeyCount].vk = vk;
    g_dynamicHotkeys[g_dynamicHotkeyCount].mod = mod;
    g_dynamicHotkeys[g_dynamicHotkeyCount].handler = handler;
    g_dynamicHotkeys[g_dynamicHotkeyCount].name = name;
    g_dynamicHotkeys[g_dynamicHotkeyCount].enabled = enabled;
    g_dynamicHotkeys[g_dynamicHotkeyCount].used = true;
    g_dynamicHotkeyCount++;
    printf("[FEATURE] Registered hotkey %s vk=%d mod=%d\n", name, vk, mod);
}

bool Feature_HandleDynamicHotkey(int vk, int mod, const char* fgTitle) {
    (void)fgTitle;
    for (int i = 0; i < g_dynamicHotkeyCount; i++) {
        if (g_dynamicHotkeys[i].vk == vk && g_dynamicHotkeys[i].mod == mod) {
            if (g_dynamicHotkeys[i].enabled && !g_dynamicHotkeys[i].enabled()) continue;
            // blacklist check? For now respect global blacklist via caller
            g_dynamicHotkeys[i].handler();
            return true;
        }
    }
    return false;
}

// EZ prompt implementations - thin wrappers that create isolated VimView
// Forward declare Platform helpers to avoid circular
extern char* Platform_PromptUser(const char* window_list[], const char* window_title, int count, int height, int width, int item_height,
    RefreshCallback refresh_callback, 
    EnterCallback enter_callback,
    CtrlEnterCallback ctrl_enter_callback,
    TabCallback tab_callback,
    CopyCallback copy_callback,
    RenderCallback render_callback,
    EditCallback edit_callback,
    FilterCallback filter_callback,
    HelpCallback help_callback);
extern char* Platform_SimplePromptUser(const char* prompt, const char* window_title,int height, int width, int item_height);

char* VimView_ShowPrompt(const VimViewSpec* spec) {
    if (!spec) return NULL;
    // Platform_PromptUser already creates an isolated VimView internally,
    // so just delegate - isolation is automatic.
    return Platform_PromptUser(spec->items, spec->title, spec->count, spec->height, spec->width, spec->item_height,
        spec->refresh, spec->enter, spec->ctrl_enter, spec->tab, spec->copy, spec->render, spec->edit, spec->filter, spec->help);
}

char* VimView_ShowSimplePrompt(const char* prompt, const char* title, int height, int width, int item_height) {
    return Platform_SimplePromptUser(prompt, title, height, width, item_height);
}
