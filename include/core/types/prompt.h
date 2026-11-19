#ifndef PROMPT_H
#define PROMPT_H

#include <stddef.h>
#include <stdbool.h>

#ifndef MAX_VISIBLE_ITEMS
#define MAX_VISIBLE_ITEMS 10000
#endif

typedef struct ListBox ListBox;
typedef struct PromptState PromptState;
typedef struct PromptCallbacks PromptCallbacks;

#include "core/types/color.h"

typedef void (*RenderCallback)(PromptState *state);

#define LISTBOX_ROW_BYTES 512

typedef struct {
  char filter[512];
  size_t cursor;
  int sel_start;
  int sel_end;
  int scroll_offset;
  char *placeholder;
} EditBox;

struct ListBox {
  char (*items)[LISTBOX_ROW_BYTES];
  int items_cap;
  int item_count;
  int matching_indices[MAX_VISIBLE_ITEMS];
  int total_matches;
  int selected_index;
  const char *selected_text;
  int view_offset;
  int text_hscroll;
  char last_filter[256];
};

struct PromptState {
  int width, height;
  char *window_title;
  EditBox *eb;
  ListBox *lb;
  PromptCallbacks *callbacks;
  void *render_data;
  bool no_bg;
  bool password_mode;
  ColorRef bg_color;
  bool no_separators;
};

typedef void (*RefreshCallback)(PromptState *state);

typedef bool (*EnterCallback)(PromptState *state);

typedef bool (*CtrlEnterCallback)(PromptState *state);

typedef bool (*ShiftEnterCallback)(PromptState *state);

typedef bool (*CtrlShiftEnterCallback)(PromptState *state);

typedef bool (*CtrlAltEnterCallback)(PromptState *state);

typedef bool (*ShiftAltEnterCallback)(PromptState *state);

typedef void (*TabCallback)(PromptState *state);

typedef void (*EditCallback)(PromptState *state);

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

void listbox_info_free(ListBox *lb);
bool listbox_info_ensure_rows(ListBox *lb, int rows);
const char *listbox_info_row(const ListBox *lb, int idx);
void listbox_info_init(ListBox *lb);

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
  void *render_data;
  bool tick;
  bool auto_scale;
  bool no_bg;
  bool password_mode;
  ColorRef bg_color;
  int w;
  int h;
  int x;
  int y;
  bool subwindow;
  bool no_separators;
  bool rounded;
  int radius;
} PromptSpec;

typedef struct {
  const char *initial_text;
  const char *placeholder;
  const char *title;
  bool password_mode;
  int x;
  int y;
  int w;
  int h;
} PopupSpec;

typedef struct {
  const char *name;
  void (*func)(const char **items, int count);
} SortOption;

#endif
