#ifndef CONTEXT_H
#define CONTEXT_H

#include "core/types.h"

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
    WindowHandle hEditControl;
    WindowHandle hDisplayMessage;
    WindowHandle hSettings;
    WindowHandle g_hEditPopup;
    WindowHandle g_hSearchPopup;
    WindowHandle g_ActiveEditControl;
    EditPopupData* g_EditData;
    bool selection_complete;
    char output_buffer[256];
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
} FlintContext;

FlintContext* FlintContext_Get(void);

#endif
