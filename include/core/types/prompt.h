#ifndef TYPES_PROMPT_H
#define TYPES_PROMPT_H

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "core/types.h"

/**
 * @brief Internal listbox state.
 *
 * Owned and managed by the prompt/window that displays the listbox.
 * Do not construct or modify this directly; use the listbox functions
 * (`fl_populate_listbox`, `fl_listbox_begin`, etc.) and read the
 * fields below if needed.
 *
 * Fields:
 * - `populated_cache`    — Cached row strings (internal).
 * - `populated_cache_cap`— Capacity of `populated_cache` (internal).
 * - `populated_count`    — Number of cached rows (internal).
 * - `matching_indices`   — Indices of items that match the current filter.
 * - `total_matches`      — Total number of matches before the visible window.
 * - `selected_index`     — Index into `matching_indices` of the selected row.
 * - `selected_text`      — The currently selected text.
 *                          or -1 if nothing is selected.
 * - `view_offset`        — Scroll offset into `matching_indices`.
 * - `last_item_count`    — Total item count from the last populate call.
 */
typedef struct ListBoxInfo ListBoxInfo;
/**
 * @brief Internal prompt state.
 *
 * Passed to `RenderCallback` during a prompt render.
 *
 * Fields:
 * - `items`          — Item array (borrowed from the caller).
 * - `item_count`     — Number of items in `items`.
 * - `filter`         — Current filter string in the editbox.
 * - `cursor`         — Cursor position in `filter`.
 * - `sel_start`      — Selection start offset in `filter`, or -1.
 * - `sel_end`        — Selection end offset in `filter`, or -1.
 * - `scroll_offset`  — Horizontal scroll offset of the edit field.
 * - `width`, `height`— Prompt window size in pixels.
 * - `window_title`   — Window title string (borrowed).
 * - `listbox`        — Associated listbox state. See `ListBoxInfo`.
 * - `has_listbox`    — True if the prompt has a listbox.
 * - `no_bg`          — True if the prompt background should not be drawn.
 * - `placeholder`    — Placeholder text shown when `filter` is empty.
 * - `render`         — Optional custom render callback. See `RenderCallback`.
 * - `editbox_placed` — True if the editbox has been placed this frame.
 */
typedef struct PromptState PromptState;
/**
 * @brief Render callback for a prompt.
 *
 * Called once per frame while the prompt is open. Draw using the
 * `fl_draw_*` and `fl_place_*` functions. The callback receives the
 * prompt's current state.
 *
 * @param state  Current prompt state. See `PromptState`.
 */
typedef void (*RenderCallback)(PromptState *state);

#define LISTBOX_ROW_BYTES 512

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
};

struct PromptState {
  char **items;
  size_t item_count;
  char filter[512];
  size_t cursor;
  int sel_start;
  int sel_end;
  int scroll_offset;
  int width, height;
  char *window_title;
  ListBoxInfo listbox;
  bool has_listbox;
  bool no_bg;
  char *placeholder;
  RenderCallback render;
  bool editbox_placed;
};

/**
 * @brief Callback invoked when the filter text changes.
 *
 * The implementation should repopulate `lb` based on `filter` using
 * 'fl_populate_listbox'.
 *
 * @param filter  Current filter string.
 * @param lb      Listbox to repopulate. See `ListBoxInfo`.
 */
typedef void (*RefreshCallback)(const char *filter, ListBoxInfo *lb);

/**
 * @brief Callback invoked when Enter is pressed.
 *
 * @param lb         Listbox state. See `ListBoxInfo`.
 * @return           True to close the prompt, false to keep it open.
 */
typedef bool (*EnterCallback)(ListBoxInfo *lb);

/**
 * @brief Callback invoked when Ctrl+Enter is pressed.
 *
 * @param lb         Listbox state. See `ListBoxInfo`.
 * @return           True to close the prompt, false to keep it open.
 */
typedef bool (*CtrlEnterCallback)(ListBoxInfo *lb);

/**
 * @brief Callback invoked when Shift+Enter is pressed.
 *
 * @param lb         Listbox state. See `ListBoxInfo`.
 * @return           True to close the prompt, false to keep it open.
 */
typedef bool (*ShiftEnterCallback)(ListBoxInfo *lb);

/**
 * @brief Callback invoked when Ctrl+Shift+Enter is pressed.
 *
 * @param lb         Listbox state. See `ListBoxInfo`.
 * @return           True to close the prompt, false to keep it open.
 */
typedef bool (*CtrlShiftEnterCallback)(ListBoxInfo *lb);

/**
 * @brief Callback invoked when Ctrl+Alt+Enter is pressed.
 *
 * @param lb         Listbox state. See `ListBoxInfo`.
 * @return           True to close the prompt, false to keep it open.
 */
typedef bool (*CtrlAltEnterCallback)(ListBoxInfo *lb);

/**
 * @brief Callback invoked when Shift+Alt+Enter is pressed.
 *
 * @param lb         Listbox state. See `ListBoxInfo`.
 * @return           True to close the prompt, false to keep it open.
 */
typedef bool (*ShiftAltEnterCallback)(ListBoxInfo *lb);

/**
 * @brief Callback invoked when Tab is pressed.
 *
 * @param lb  Listbox state. See `ListBoxInfo`.
 */
typedef void (*TabCallback)(ListBoxInfo *lb);

/**
 * @brief Callback invoked on text edit events.
 *
 * @param lb         Listbox state. See `ListBoxInfo`.
 */
typedef void (*EditCallback)(ListBoxInfo *lb);

/**
 * @brief Callback used to filter listbox items.
 *
 * @param item    Item text to test.
 * @param filter  Current filter string.
 */
typedef void (*FilterCallback)(const char *item, const char *filter);

/**
 * @brief Callback invoked when the user requests help.
 */
typedef void (*HelpCallback)(void);

/**
 * @brief Callback invoked on key events.
 *
 * @param vkCode  Virtual key code.
 */
typedef void (*KeyCallback)(int vkCode);

/**
 * @brief Callback invoked when the prompt closes.
 */
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

/**
 * @brief Parameters for creating a prompt window.
 *
 * Passed to `fl_window`. One input callback is needed be it enter ctrl_enter
 * does not matter.
 *
 * Fields:
 * - `title`          — Window title.
 * - `items`          — Initial item array for the listbox.
 * - `refresh`        — Called when the filter changes. See `RefreshCallback`.
 * - `enter`          — Called on Enter. See `EnterCallback`.
 * - `ctrl_enter`     — Called on Ctrl+Enter. See `CtrlEnterCallback`.
 * - `shift_enter`    — Called on Shift+Enter. See `ShiftEnterCallback`.
 * - `ctrl_shift_enter` — Called on Ctrl+Shift+Enter. See
 * `CtrlShiftEnterCallback`.
 * - `ctrl_alt_enter` — Called on Ctrl+Alt+Enter. See `CtrlAltEnterCallback`.
 * - `shift_alt_enter`— Called on Shift+Alt+Enter. See `ShiftAltEnterCallback`.
 * - `tab`            — Called on Tab. See `TabCallback`.
 * - `copy`           — Called on copy. See `CopyCallback`.
 * - `render`         — Custom render callback. See `RenderCallback`.
 * - `edit`           — Called on listboxtext edit. See `EditCallback`.
 * - `filter`         — Item filter callback. See `FilterCallback`.
 * - `help`           — Called on help request. See `HelpCallback`.
 * - `on_close`       — Called when the prompt closes. See `CloseCallback`.
 * - `placeholder`    — Placeholder text appears in the editbox.
 * - `tick`           — If true, the prompt re-renders every frame instead of
 * per keypress.
 * - `auto_scale`     — If true, size(w,h) are scaled automatically.
 * - `no_bg`          — If true, no background is drawn.
 * - `w`, `h`         — Window width and height in pixels.
 * - `x`, `y`         — Window position in screen coordinates.
 * - `subwindow`      — If true, the prompt is a subwindow of an existing one
 * not to be done manually if a subwindow is wanted call `fl_subwindow`.
 */
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

/**
 * @brief Parameters for creating a simple text-input popup.
 *
 * Passed to `fl_popup`.
 *
 * Fields:
 * - `initial_text` — Starting text in the input field.
 * - `placeholder`  — Placeholder text when the field is empty.
 * - `title`        — Popup title.
 * - `x`, `y`       — Popup position in screen coordinates.
 * - `w`, `h`       — Popup size in pixels.
 */
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
  char keys[MAX_SEQUENCE_LEN];
  int len;
  int typed_count;
  long long deadline_ms;
  bool waiting;
  bool timed_out;
} SequenceState;

typedef struct {
  const char *name;
  void (*func)(const char **items, int count);
} SortOption;

#endif
