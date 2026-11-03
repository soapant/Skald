#ifndef TYPES_PROMPT_H
#define TYPES_PROMPT_H

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "core/types.h"


typedef struct ListBoxInfo ListBoxInfo;

typedef struct PromptState PromptState;
typedef struct PromptCallbacks PromptCallbacks;

typedef void (*RenderCallback)(PromptState *state);

#define LISTBOX_ROW_BYTES 512

typedef struct {
  char filter[512];
  size_t cursor;
  int sel_start;
  int sel_end;
  int scroll_offset;
  char *placeholder;
} EditBoxInfo;

struct ListBoxInfo {
  char (*populated_cache)[LISTBOX_ROW_BYTES];
  int populated_cache_cap;
  int populated_count;
  int matching_indices[MAX_VISIBLE_ITEMS];
  int total_matches;
  int selected_index;
  char *selected_text;
  int view_offset;
  int last_item_count;
  char **items;
  size_t item_count;
};

struct PromptState {
  int width, height;
  char *window_title;
  EditBoxInfo *editbox;
  ListBoxInfo *listbox;
  PromptCallbacks *callbacks;
  bool no_bg;
};


typedef void (*RefreshCallback)(PromptState* state);


typedef bool (*EnterCallback)(PromptState* state);


typedef bool (*CtrlEnterCallback)(PromptState* state);


typedef bool (*ShiftEnterCallback)(PromptState* state);


typedef bool (*CtrlShiftEnterCallback)(PromptState* state);


typedef bool (*CtrlAltEnterCallback)(PromptState* state);


typedef bool (*ShiftAltEnterCallback)(PromptState* state);


typedef void (*TabCallback)(PromptState* state);


typedef void (*EditCallback)(PromptState* state);


typedef void (*FilterCallback)(const char *item, const char *filter);


typedef void (*HelpCallback)(void);

typedef struct PromptCallbacks {
  RefreshCallback refresh;
  EnterCallback enter;
  CtrlEnterCallback ctrl_enter;
  ShiftEnterCallback shift_enter;
  CtrlShiftEnterCallback ctrl_shift_enter;
  CtrlAltEnterCallback ctrl_alt_enter;
  ShiftAltEnterCallback shift_alt_enter;
  TabCallback tab;
  HelpCallback help;
  RenderCallback render;
  EditCallback edit;
  FilterCallback filter;
} PromptCallbacks;



typedef void (*KeyCallback)(int vkCode);


typedef void (*CloseCallback)(void);

static inline void ListBoxInfo_Free(ListBoxInfo *lb) {
  if (!lb)
    return;
  free(lb->populated_cache);
  lb->populated_cache = NULL;
  lb->populated_cache_cap = 0;
  lb->populated_count = 0;
}

static inline bool ListBoxInfo_EnsureRows(ListBoxInfo *lb, int rows) {
  if (!lb || rows <= 0 || rows > MAX_VISIBLE_ITEMS)
    return false;
  if (lb->populated_cache && rows <= lb->populated_cache_cap)
    return true;
  int cap = lb->populated_cache_cap > 0 ? lb->populated_cache_cap : 64;
  while (cap < rows)
    cap *= 2;
  if (cap > MAX_VISIBLE_ITEMS)
    cap = MAX_VISIBLE_ITEMS;
  if (cap < rows)
    return false;
  char (*grown)[LISTBOX_ROW_BYTES] = (char (*)[LISTBOX_ROW_BYTES])realloc(
      lb->populated_cache, (size_t)cap * LISTBOX_ROW_BYTES);
  if (!grown)
    return false;
  lb->populated_cache = grown;
  lb->populated_cache_cap = cap;
  return true;
}

static inline const char *ListBoxInfo_Row(ListBoxInfo *lb, int idx) {
  if (!lb || !lb->populated_cache)
    return NULL;
  if (idx < 0 || idx >= lb->populated_count)
    return NULL;
  if (idx >= lb->populated_cache_cap)
    return NULL;
  return lb->populated_cache[idx];
}

static inline void ListBoxInfo_Init(ListBoxInfo *lb) {
  if (!lb)
    return;
  ListBoxInfo_Free(lb);
  lb->selected_index = -1;
  lb->selected_text = "";
  lb->total_matches = 0;
  lb->view_offset = 0;
  lb->last_item_count = 0;
}


typedef struct {
  const char *title;
  const char **items;
  RefreshCallback refresh;
  EnterCallback enter;
  CtrlEnterCallback ctrl_enter;
  ShiftEnterCallback shift_enter;
  CtrlShiftEnterCallback ctrl_shift_enter;
  CtrlAltEnterCallback ctrl_alt_enter;
  ShiftAltEnterCallback shift_alt_enter;
  TabCallback tab;
  RenderCallback render;
  EditCallback edit;
  FilterCallback filter;
  HelpCallback help;
  CloseCallback on_close;
  const char *placeholder;
  bool tick;
  bool auto_scale;
  bool no_bg;
  int w;
  int h;
  int x;
  int y;
  bool subwindow;
} PromptSpec;


typedef struct {
  const char *initial_text;
  const char *placeholder;
  const char *title;
  int x;
  int y;
  int w;
  int h;
} PopupSpec;

typedef struct WindowManager WindowManager;
WindowManager *WindowManager_Get(void);
char *WindowManager_ShowPrompt(WindowManager *wm, const PromptSpec *spec);

typedef struct {
  const char *name;
  void (*func)(const char **items, int count);
} SortOption;

#endif
