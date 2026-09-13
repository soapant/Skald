#ifndef CONTEXT_H
#define CONTEXT_H

#include "core/types.h"
#include "core/hashmap.h"

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
    HashMap* feature_history;
    HashMap* g_item_index_map;
    HashMap* history_indices;
    WinTimer g_timer;
    char* SelectionData[2048];
    int SelectionDataCount;
#if defined(_WIN32)
    uint32_t g_main_thread_id;
    HFONT g_hNumFont;
    HFONT g_hMainFont;
    HANDLE g_key_event;
    HHOOK hKeyboardHook;
    int g_timeout;
    bool g_waiting_for_key;
    int g_key_pressed;
    KeyCallback g_key_callback;
#endif
} FlintContext;

FlintContext* FlintContext_Get(void);

#endif
