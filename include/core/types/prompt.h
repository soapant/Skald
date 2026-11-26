#ifndef PROMPT_H
#define PROMPT_H

#define LISTBOX_SEPARATOR_MIN_LIST_H 70

typedef struct {
  int x;
  int y;
} Pos;

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

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
  int edit_cursor_width;
  int edit_cursor_height;
  int editbox_padding;
  int listbox_padding;
  int listbox_item_height;
} Metrics;

typedef struct {
  char filter[512];
  size_t cursor;
  int sel_start;
  int sel_end;
  int scroll_offset;
  char *placeholder;
} EditBox;

#define PROMPT_EDITBOX_MAX 32
#define EDITBOX_WRAP_MAX_CHARS 513

typedef struct {
  uint64_t key;
  int len;
  int line_h;
  int max_width;
  int count;
  Pos *positions;
  int positions_cap;
} EditboxWrapCache;

typedef struct {
  char name[128];
  EditBox eb;
  char *buffer;
  size_t buffer_size;
  Metrics metrics;
  int x, y, w, h;
  bool placed;
  bool was_down;
  bool primary;
  bool no_tab;
  bool wrapped;
  int font_size;
  EditboxWrapCache wrap_cache;
} PromptEditbox;

struct ListBox {
  char **items;
  int items_cap;
  int item_count;
  int *matching_indices;
  int matches_cap;
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
  EditBox *default_eb;
  PromptEditbox editboxes[PROMPT_EDITBOX_MAX];
  int editbox_count;
  int active_editbox;
  ListBox *lb;
  PromptCallbacks *callbacks;
  void *render_data;
  bool no_bg;
  bool password_mode;
  ColorRef bg_color;
  bool no_separators;
  double delta_time;
  long long last_frame_ticks;
};

typedef void (*RefreshCallback)(PromptState *state);

typedef bool (*EnterCallback)(PromptState *state);

typedef bool (*CtrlEnterCallback)(PromptState *state);

typedef bool (*ShiftEnterCallback)(PromptState *state);

typedef bool (*CtrlShiftEnterCallback)(PromptState *state);

typedef bool (*CtrlAltEnterCallback)(PromptState *state);

typedef bool (*ShiftAltEnterCallback)(PromptState *state);

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
  HelpCallback help;
  RenderCallback render;
  EditCallback edit;
  FilterCallback filter;
} PromptCallbacks;

typedef void (*KeyCallback)(int vkCode);

typedef void (*CloseCallback)(void);

void listbox_info_free(ListBox *lb);
bool listbox_info_ensure_rows(ListBox *lb, int rows);
bool listbox_info_set_row(ListBox *lb, int idx, const char *text);
bool listbox_info_ensure_matches(ListBox *lb, int n);
const char *listbox_info_row(const ListBox *lb, int idx);
void listbox_info_init(ListBox *lb);
void editbox_wrap_cache_free(EditboxWrapCache *cache);

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
  RenderCallback render;
  EditCallback edit;
  FilterCallback filter;
  HelpCallback help;
  CloseCallback on_close;
  void *render_data;
  int fps;
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
  bool anchor_y_to_prompt_h;
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
