#ifndef TYPES_PROMPT_H
#define TYPES_PROMPT_H

#include "core/types.h"

typedef struct ListBoxInfo ListBoxInfo;

struct ListBoxInfo {
    char populated_cache[MAX_VISIBLE_ITEMS][512];
    int  populated_count;
    int  matching_indices[MAX_VISIBLE_ITEMS];
    int  total_matches;
    int  selected_index;
    int  view_offset;
    const char **last_items;
    int  last_item_count;
    char last_filter[256];
    bool last_filter_valid;
    int  prev_matches[MAX_VISIBLE_ITEMS];
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

typedef void (*RefreshCallback)(const char* filter, WindowHandle hListBox, ListBoxInfo* lb);
typedef bool (*EnterCallback)(const char* selection, ListBoxInfo* lb);
typedef bool (*CtrlEnterCallback)(const char* selection, ListBoxInfo* lb);
typedef void (*TabCallback)(ListBoxInfo* lb);
typedef void (*RenderCallback)(PromptState *state);
typedef void (*AliasCallback)(void* hdc, const char* text, void* rect, bool is_selected, int itemIndex);
typedef void (*EditCallback)(const char* selection, ListBoxInfo* lb);
typedef void (*FilterCallback)(const char* item,const char* filter);
typedef void (*CopyCallback)(const char* selection,int true_index,int type);
typedef void (*HelpCallback)();
typedef void (*KeyCallback)(int vkCode);
typedef void (*CloseCallback)(void);

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

typedef struct {
    const char* title;
    const char** items;
    RefreshCallback refresh;
    EnterCallback enter;
    CtrlEnterCallback ctrl_enter;
    TabCallback tab;
    CopyCallback copy;
    RenderCallback render;
    EditCallback edit;
    FilterCallback filter;
    HelpCallback help;
    CloseCallback on_close;
} PromptSpec;

typedef struct {
    const char* initial_text;
    const char* title;
    int x;
    int y;
    int w;
    int h;
} FlPopupSpec;

typedef struct WindowManager WindowManager;
WindowManager* WindowManager_Get(void);
char* WindowManager_ShowPrompt(WindowManager* wm, const PromptSpec* spec);
#define PROMPT(...) ((char*)WindowManager_ShowPrompt(WindowManager_Get(), &(PromptSpec){ __VA_ARGS__ }))

typedef struct {
    char keys[MAX_SEQUENCE_LEN];
    int len;
    int typed_count;
    long long deadline_ms;
    bool waiting;
    bool timed_out;
} SequenceState;

typedef struct {
    const char* name;                         
    void (*func)(const char** items, int count);
} SortOption;

void SubjectiveMessageDisplay(const char* buffer,int msg_height,int timeout,ColorRef color);

#endif
