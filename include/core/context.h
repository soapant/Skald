#ifndef CONTEXT_H
#define CONTEXT_H

#include "core/types.h"
#include "core/modal.h"

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
    WindowHandle hPopupWnd;
    WindowHandle hListBox;
    WindowHandle hEditControl;
    WindowHandle hDisplayMessage;
    WindowHandle hSettings;
    WindowHandle g_ActiveEditControl;
    bool selection_complete;
    char output_buffer[256];
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
} FlintContext;

FlintContext* FlintContext_Get(void);

#endif
