#ifndef CONTEXT_H
#define CONTEXT_H

#include "core/types.h"
#include "deps/stb_ds.h"
typedef struct { char *key; void *value; } HashMap;

typedef enum {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_VISUAL,
} EditorMode;

typedef struct {
    RefreshCallback refresh;
    EnterCallback enter;
    CtrlEnterCallback ctrl_enter;
    ShiftEnterCallback shift_enter;
    CtrlShiftEnterCallback ctrl_shift_enter;
    CtrlAltEnterCallback ctrl_alt_enter;
    ShiftAltEnterCallback shift_alt_enter;
    TabCallback tab;
    HelpCallback help;
    CopyCallback copy;
    RenderCallback render;
    EditCallback edit;
    FilterCallback filter;
} PromptCallbacks;

typedef struct {
    WindowHandle hGlobalTrayWnd;
    WindowHandle hPopupWnd;
    WindowHandle hDisplayMessage;
    bool selection_complete;
    bool editbox_placed;
    char output_buffer[256];
    char filter[1024];
    int filter_cursor;
    char* current_feature;
    const char* title;
    int last_index;
    PromptCallbacks callbacks;
} WindowContext;

typedef struct {
    EditorMode vim_mode;
    const char* vim_mode_str;
    WindowContext window;
    PromptCallbacks prompt_callbacks;
    HashMap* feature_history;
    HashMap* item_index_map;
    HashMap* history_indices;
} FlintContext;

FlintContext* FlintContext_Get(void);

#endif
