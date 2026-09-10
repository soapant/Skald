#ifndef TYPES_PROMPT_H
#define TYPES_PROMPT_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "core/types/window.h"
#include "core/types/color.h"

#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#endif

typedef struct ListBoxInfo ListBoxInfo;

struct ListBoxInfo {
    char populated_cache[MAX_ITEMS][512];
    int  populated_count;
    int  matching_indices[MAX_MATCHES];
    int  total_matches;
    int  selected_index;
    int  view_offset;
    const char **last_items;
    int  last_item_count;
    char last_filter[256];
    bool last_filter_valid;
    int  prev_matches[MAX_MATCHES];
    int  last_filter_full_count;
};

typedef struct {
     char**      items;
     size_t      item_count;
     char        filter[512];
     size_t      cursor;
     int         scroll_offset;
     int         width, height;
     char*       window_title;
     ListBoxInfo listbox;
     bool        has_listbox;
} PromptState;

typedef struct {
     char*       display_message;
     size_t      cursor;
     int         scroll_offset;
     char        user_input[512];
     int         width, height;
     char*       window_title;
} SimplePromptState;

typedef void (*RefreshCallback)(const char* filter, WindowHandle hListBox, ListBoxInfo* lb);
typedef bool (*EnterCallback)(const char* selection, ListBoxInfo* lb);
typedef bool (*CtrlEnterCallback)(const char* selection, ListBoxInfo* lb);
typedef void (*TabCallback)(ListBoxInfo* lb);
#if defined(_WIN32)
typedef void (*RenderCallback)(HDC hdc, const char* text, RECT* rect, BOOL is_selected, int itemIndex);
typedef void (*AliasCallback)(HDC hdc, const char* text, RECT* rect, BOOL is_selected, int itemIndex);
#else
typedef void (*RenderCallback)(Display *dpy, Window *win, PromptState *state);
typedef void (*AliasCallback)(void* hdc, const char* text, void* rect, bool is_selected, int itemIndex);
#endif
typedef void (*EditCallback)(const char* selection, ListBoxInfo* lb);
typedef void (*FilterCallback)(const char* item,const char* filter);
typedef void (*CopyCallback)(const char* selection,int true_index,int type);
typedef void (*HelpCallback)();
typedef void (*KeyCallback)(int vkCode);

static inline void ListBoxInfo_Init(ListBoxInfo *lb) {
    if (!lb) return;
    memset(lb, 0, sizeof(*lb));
    lb->selected_index = -1;
    lb->populated_count = 0;
    lb->total_matches = 0;
    lb->view_offset = 0;
}

static inline const char* ListBoxInfo_GetSelectedText(ListBoxInfo *lb) {
    if (!lb) return NULL;
    if (lb->selected_index < 0 || lb->selected_index >= lb->populated_count) return NULL;
    return lb->populated_cache[lb->selected_index];
}

static inline int ListBoxInfo_GetCount(ListBoxInfo *lb) {
    return lb ? lb->populated_count : 0;
}

static inline bool ListBoxInfo_IsSelected(ListBoxInfo *lb) {
    return lb && lb->selected_index != -1;
}


typedef struct {
    const char* title;
    const char** items;
    int count;
    int height, width, item_height;
    RefreshCallback refresh;
    EnterCallback enter;
    CtrlEnterCallback ctrl_enter;
    TabCallback tab;
    CopyCallback copy;
    RenderCallback render;
    EditCallback edit;
    FilterCallback filter;
    HelpCallback help;
} PromptSpec;

typedef struct {
    const char* prompt;
    const char* title;
    int height, width, item_height;
} SimplePromptSpec;

typedef struct WindowManager WindowManager;
WindowManager* WindowManager_Get(void);
char* WindowManager_ShowPrompt(WindowManager* wm, const PromptSpec* spec);
char* WindowManager_ShowSimplePrompt(WindowManager* wm, const SimplePromptSpec* spec);
#define PROMPT(...) ((char*)WindowManager_ShowPrompt(WindowManager_Get(), &(PromptSpec){ __VA_ARGS__ }))
#define SIMPLE_PROMPT(...) ((char*)WindowManager_ShowSimplePrompt(WindowManager_Get(), &(SimplePromptSpec){ __VA_ARGS__ }))

typedef struct {
    char keys[MAX_SEQUENCE_LEN];
    int len;
    int typed_count;
    unsigned int timer_id;
    bool waiting;
    bool timed_out;
} SequenceState;

typedef struct {
    const char* name;                         
    void (*func)(const char** items, int count);
} SortOption;

void SubjectiveMessageDisplay(const char* buffer,int msg_height,int timeout,ColorRef color);

#endif