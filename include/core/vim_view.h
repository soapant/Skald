#ifndef VIM_VIEW_H
#define VIM_VIEW_H

#include "core/types.h"
#include "core/modal.h"
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

// Isolated per-window vim state.
// Every window/prompt gets its own VimView - no globals leak.
typedef struct VimView {
    unsigned long xid;               // X11 Window id (or 0 if not mapped yet)
    char text[512];                  // unified filter/user_input buffer
    int cursor;
    int scroll_offset;
    int width, height;
    char* window_title;
    ListBoxInfo lb;
    bool has_listbox;
    EditorMode mode;
    SequenceState seq;
    struct timespec seq_start;       // for Linux timeout
    bool seq_timing;
    int last_index;                  // per-view RememberLastIndex
} VimView;

// Lifecycle
VimView* VimView_Create(bool has_listbox, const char* title, int width, int height);
void VimView_Destroy(VimView* v);
void VimView_Reset(VimView* v);

// Active view registry - single active modal at a time, but each view keeps its own state
void VimView_SetActive(VimView* v);
VimView* VimView_GetActive(void);
void VimView_ClearActive(void);
bool VimView_HasActive(void);

// Mode helpers (per-view, syncs globals for legacy code)
EditorMode VimView_GetMode(VimView* v);
const char* VimView_GetModeName(VimView* v);
void VimView_SetMode(VimView* v, EditorMode mode);
bool VimView_IsNormalMode(VimView* v);
bool VimView_IsInsertMode(VimView* v);
bool VimView_IsVisualMode(VimView* v);

// Sequence helpers (per-view)
void VimView_SeqAddKey(VimView* v, char key, bool was_typed);
void VimView_SeqReset(VimView* v);
int VimView_SeqLen(VimView* v);
int VimView_SeqTypedCount(VimView* v);
bool VimView_SeqMatches(VimView* v, const char* pattern);
bool VimView_SeqStartsWith(VimView* v, const char* pattern);
void VimView_SeqStartTimer(VimView* v);
bool VimView_SeqIsWaiting(VimView* v);
bool VimView_SeqTimedOut(VimView* v);
void VimView_SeqUpdateTimeout(VimView* v);

// Text helpers (unified)
void VimView_GetText(VimView* v, char* buf, size_t cap);
void VimView_SetText(VimView* v, const char* text);
int VimView_GetCursor(VimView* v);
void VimView_SetCursor(VimView* v, int pos);

// Sync legacy globals from active view (call after SetActive or mode change)
void VimView_SyncGlobals(void);

// High-level EZ API for feature authors
// Spec for a prompt-like window with fully isolated vim state.
// Feature file just fills this and calls VimView_ShowPrompt - all vim isolation is automatic.
typedef struct {
    const char* title;               // e.g. "🚀 Launcher"
    const char** items;              // list to display
    int count;
    int height;
    int width;
    int item_height;
    RefreshCallback refresh;
    EnterCallback enter;
    CtrlEnterCallback ctrl_enter;
    TabCallback tab;
    CopyCallback copy;
    RenderCallback render;
    EditCallback edit;
    FilterCallback filter;
    HelpCallback help;
} VimViewSpec;

// One-liner for features: creates isolated view, runs modal loop, returns selection (owned buffer, NULL if cancelled)
// This is the replacement for Platform_PromptUser for new features.
char* VimView_ShowPrompt(const VimViewSpec* spec);

// Same for simple prompt
char* VimView_ShowSimplePrompt(const char* prompt, const char* title, int height, int width, int item_height);

// Hotkey registration for EZ per-file features
// Feature file can call FEATURE_HOTKEY('P', HOTKEY_MOD_ALT, TriggerMyFeature) at top-level
// No need to edit hotkey_definitions.c
typedef bool (*FeatureEnabledFn)(void);
void Feature_RegisterHotkey(int vk, int mod, void (*handler)(void), const char* name, FeatureEnabledFn enabled);
bool Feature_HandleDynamicHotkey(int vk, int mod, const char* fgTitle);

// Helper macro for feature files: auto-registers at startup
#define FEATURE_HOTKEY(vk_, mod_, handler_) \
    __attribute__((constructor)) static void _feature_hotkey_reg_##handler_(void) { \
        Feature_RegisterHotkey((vk_), (mod_), (handler_), #handler_, NULL); \
    }

#define FEATURE_HOTKEY_IF(vk_, mod_, handler_, enabled_fn_) \
    __attribute__((constructor)) static void _feature_hotkey_reg_##handler_(void) { \
        Feature_RegisterHotkey((vk_), (mod_), (handler_), #handler_, (enabled_fn_)); \
    }

#endif
