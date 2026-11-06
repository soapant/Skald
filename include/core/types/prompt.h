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
  char (*items)[LISTBOX_ROW_BYTES];
  int items_cap;
  int item_count;
  int matching_indices[MAX_VISIBLE_ITEMS];
  int total_matches;
  int selected_index;
  char *selected_text;
  int view_offset;
};

struct PromptState {
  int width, height;
  char *window_title;
  EditBoxInfo *eb;
  ListBoxInfo *lb;
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
  free(lb->items);
  lb->items = NULL;
  lb->items_cap = 0;
  lb->item_count = 0;
}

static inline bool ListBoxInfo_EnsureRows(ListBoxInfo *lb, int rows) {
  if (!lb || rows <= 0 || rows > MAX_VISIBLE_ITEMS)
    return false;
  if (lb->items && rows <= lb->items_cap)
    return true;
  int cap = lb->items_cap > 0 ? lb->items_cap : 64;
  while (cap < rows)
    cap *= 2;
  if (cap > MAX_VISIBLE_ITEMS)
    cap = MAX_VISIBLE_ITEMS;
  if (cap < rows)
    return false;
  char (*grown)[LISTBOX_ROW_BYTES] = (char (*)[LISTBOX_ROW_BYTES])realloc(
      lb->items, (size_t)cap * LISTBOX_ROW_BYTES);
  if (!grown)
    return false;
  lb->items = grown;
  lb->items_cap = cap;
  return true;
}

static inline const char *ListBoxInfo_Row(ListBoxInfo *lb, int idx) {
  if (!lb || !lb->items)
    return NULL;
  if (idx < 0 || idx >= lb->item_count)
    return NULL;
  if (idx >= lb->items_cap)
    return NULL;
  return lb->items[idx];
}

static inline void ListBoxInfo_Init(ListBoxInfo *lb) {
  if (!lb)
    return;
  ListBoxInfo_Free(lb);
  lb->selected_index = -1;
  lb->selected_text = "";
  lb->total_matches = 0;
  lb->view_offset = 0;
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
  const char *initial_text;
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
