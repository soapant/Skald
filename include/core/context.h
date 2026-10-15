#ifndef CONTEXT_H
#define CONTEXT_H

#include "core/types.h"
#include "deps/stb_ds.h"
typedef struct { char *key; void *value; } FlintMap;

typedef enum {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_VISUAL,
} EditorMode;

typedef struct {
    RefreshCallback refresh;
    EnterCallback enter;
    CtrlEnterCallback ctrl_enter;
    TabCallback tab;
    HelpCallback help;
    CopyCallback copy;
    RenderCallback render;
    AliasCallback alias;
    EditCallback edit;
    FilterCallback filter;
} PromptCallbacks;

typedef struct {
    WindowHandle hGlobalTrayWnd;
    WindowHandle hPopupWnd;
    WindowHandle hListBox;
    WindowHandle hDisplayMessage;
    WindowHandle g_hEditPopup;
    EditPopupData* g_EditData;
    bool selection_complete;
    bool editbox_placed;
    char output_buffer[256];
    char filter[1024];
    int filter_cursor;
    char* current_feature;
    const char* title;
    int last_index;
    PromptCallbacks callbacks;
#if !defined(_WIN32)
    Display* g_font_dpy;
#endif
} WindowContext;

typedef struct {
    EditorMode vim_mode;
    const char* vim_mode_str;
    WindowContext window;
    PromptCallbacks prompt_callbacks;
    FlintMap* feature_history;
    FlintMap* item_index_map;
    FlintMap* history_indices;
    char* SelectionData[2048];
    int SelectionDataCount;
} FlintContext;

FlintContext* FlintContext_Get(void);

#endif
