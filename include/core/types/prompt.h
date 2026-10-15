#ifndef TYPES_PROMPT_H
#define TYPES_PROMPT_H

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "core/types.h"

typedef struct ListBoxInfo ListBoxInfo;

#define LISTBOX_ROW_BYTES 512

struct ListBoxInfo {
    char (*populated_cache)[LISTBOX_ROW_BYTES];
    int   populated_cache_cap;
    const char* alias_suffix[MAX_VISIBLE_ITEMS];
    int  alias_suffix_count;
    bool alias_suffix_active;
    int  populated_count;
    int  matching_indices[MAX_VISIBLE_ITEMS];
    int  total_matches;
    int  selected_index;
    int  view_offset;
    int  last_item_count;
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

typedef void (*RefreshCallback)(const char* filter, ListBoxInfo* lb);
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

static inline void ListBoxInfo_Free(ListBoxInfo *lb) {
    if (!lb) return;
    free(lb->populated_cache);
    lb->populated_cache = NULL;
    lb->populated_cache_cap = 0;
    lb->populated_count = 0;
}

static inline bool ListBoxInfo_EnsureRows(ListBoxInfo *lb, int rows) {
    if (!lb || rows <= 0 || rows > MAX_VISIBLE_ITEMS) return false;
    if (lb->populated_cache && rows <= lb->populated_cache_cap) return true;
    int cap = lb->populated_cache_cap > 0 ? lb->populated_cache_cap : 64;
    while (cap < rows) cap *= 2;
    if (cap > MAX_VISIBLE_ITEMS) cap = MAX_VISIBLE_ITEMS;
    if (cap < rows) return false;
    char (*grown)[LISTBOX_ROW_BYTES] = (char (*)[LISTBOX_ROW_BYTES])realloc(
        lb->populated_cache, (size_t)cap * LISTBOX_ROW_BYTES);
    if (!grown) return false;
    lb->populated_cache = grown;
    lb->populated_cache_cap = cap;
    return true;
}

static inline const char* ListBoxInfo_Row(ListBoxInfo *lb, int idx) {
    if (!lb || !lb->populated_cache) return NULL;
    if (idx < 0 || idx >= lb->populated_count) return NULL;
    if (idx >= lb->populated_cache_cap) return NULL;
    return lb->populated_cache[idx];
}

static inline void ListBoxInfo_Init(ListBoxInfo *lb) {
    if (!lb) return;
    ListBoxInfo_Free(lb);
    lb->selected_index = -1;
    lb->total_matches = 0;
    lb->view_offset = 0;
    lb->last_item_count = 0;
    lb->alias_suffix_count = 0;
    lb->alias_suffix_active = false;
}

static inline const char* ListBoxInfo_GetSelectedText(ListBoxInfo *lb) {
    if (!lb) return NULL;
    return ListBoxInfo_Row(lb, lb->selected_index);
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
    bool tick;
} PromptSpec;

typedef struct {
    const char* initial_text;
    const char* placeholder;
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

#endif
