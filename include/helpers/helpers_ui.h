#ifndef HELPERS_UI_H
#define HELPERS_UI_H

#include <stdbool.h>
#include <wchar.h>
#include "core/types.h"
#include "core/types/window.h"
#include "core/draw_state.h"
#include "platform/draw.h"
#include "platform/platform.h"

unsigned long rgb_to_hex(unsigned long win_color);
unsigned short opacity_alpha16(void);

unsigned int color_lerp(unsigned int a, unsigned int b, float t);
ColorRef color_lerp_bgr(ColorRef a, ColorRef b, float t);

typedef struct {
  int sw;
  int sh;
  int tw;
  int th;
  int *dw;
  int *dh;
} CoverScale;

bool cover_scale(CoverScale *argspec);

int draw_thickness_px(float thickness);
#define draw_measure_track(x, y, w, h)                                         \
  draw_state_track_measure((x), (y), (w), (h))

typedef struct {
  const DrawLineRaw *spec;
  int off_x;
  int off_y;
  int *x1;
  int *y1;
  int *x2;
  int *y2;
} DrawLinePrep;

int draw_line_prep(DrawLinePrep *argspec);

typedef struct {
  const DrawCircleRaw *spec;
  int off_x;
  int off_y;
  int *cx;
  int *cy;
} DrawCirclePrep;

int draw_circle_prep(DrawCirclePrep *argspec);

typedef struct {
  ListBox *lb;
  const char *filter;
  const char **items;
  int item_count;
  bool (*filter_match)(const char *item, const char *filter);
} PopulateListbox;

void populate_listbox(PopulateListbox *argspec);

typedef struct {
  ListBox *lb;
  const char *filter;
  const char **items;
  int item_count;
  bool (*filter_match)(const char *item, const char *filter);
  int (*item_rank)(int item_index);
} PopulateListboxRanked;

void populate_listbox_ranked(PopulateListboxRanked *argspec);

typedef enum {
  AUTO = 0,
  TOP_LEFT,
  TOP_CENTER,
  TOP_RIGHT,
  LEFT,
  CENTER,
  RIGHT,
  BOTTOM_LEFT,
  BOTTOM_CENTER,
  BOTTOM_RIGHT,
  TOP_LEFT_RELATIVE,
  TOP_CENTER_RELATIVE,
  TOP_RIGHT_RELATIVE,
  LEFT_RELATIVE,
  CENTER_RELATIVE,
  RIGHT_RELATIVE,
  BOTTOM_LEFT_RELATIVE,
  BOTTOM_CENTER_RELATIVE,
  BOTTOM_RIGHT_RELATIVE,
  FREE
} MessageSlot;

void update_text_caret(void);

#define MSG_PROMPT_GAP_DEFAULT 300
#define MSG_TIMEOUT_INDEFINITE 6969.0
#define MsgTimeoutIsIndefinite(t) ((t) == MSG_TIMEOUT_INDEFINITE || (t) <= 0.0)

typedef struct WindowManager WindowManager;

typedef struct {
  int bx;
  int bw;
  int fw;
  int fh;
  int y0;
  int item_h;
  int list_h;
  int visible;
} ListboxLayout;

char *prompt_user_run(WindowManager *wm, const PromptSpec *spec);
void prompt_process_pending_subwindow(void);
bool terminal_guard(const char *working_dir, const char *extra_args);
Metrics *metrics_get(void);
void draw_prompt_separator(DrawPromptSeparator spec);
bool prompt_band_take_placed(void);
void prompt_band_commit_placed(void);

typedef struct {
  const List *spec;
  int default_y0;
  int item_h;
  int popup_w;
  int popup_h;
  int count;
  ListboxLayout *out;
} ListboxComputeLayout;

void listbox_compute_layout(ListboxComputeLayout *argspec);

typedef struct {
  int x, y, w, h;
  int h_slot, v_slot, gap, outward;
} SlotRect;

SlotRect message_slot_to_rect(MessageSlot slot, int prompt_gap);

typedef struct {
  const char *message;
  MessageSlot slot;
  int max_w, max_h;
  int prompt_gap;
  double timeout;
  ColorRef color;
  void (*render)(void *ud);
  void *render_data;
  const char *name;
  int win_w, win_h;
  bool rounded;
  int radius;
  int fps;
  bool no_bg;
} MessageSlotSpec;

void display_message_slot(const MessageSlotSpec *spec);

void set_transparent_window(bool on);
bool get_transparent_window(void);
void set_draw_bg_color(unsigned long hex);
unsigned long get_draw_bg_color(void);
void begin_measure(void);
void end_measure(int *out_w, int *out_h);
void get_last_measure_origin(int *out_x, int *out_y);
void set_draw_offset(int dx, int dy);
void get_draw_offset(int *out_x, int *out_y);

ColorRef widget_accent(void);
ColorRef widget_neutral(void);

typedef struct {
  bool valid;
  int mx, my;
  bool down;
} WidgetMouseState;

void widget_mouse_poll(WidgetMouseState *state, int *out_x, int *out_y,
                       bool *out_down);

bool is_popup_active(WindowHandle hPopupWnd);
void UI_SetSelection(ListBox *lb, int index);
void UI_LB_SetCount(ListBox *lb, int count);
void UI_LB_BeginUpdate(void);
void UI_LB_EndUpdate(void);
void UI_LB_RequestRepaint(void);
void UI_RequestPopupRepaint(void);
void UI_LegacySyncRepaint(void);
int UI_LB_GetViewTop(const ListBox *lb);
int UI_LB_GetVisibleCount(void);
void UI_LB_SetVisibleCount(int count);
void UI_LB_ScrollToTop(ListBox *lb, int top_index);
void UI_NavigateTo(ListBox *lb, int index);
void UI_NavigateUp(ListBox *lb);
void UI_NavigateDown(ListBox *lb);

void UI_Edit_GetSel(uint32_t *start, uint32_t *end);
void UI_GetCurrentTextEdit(char *buffer, size_t cap);
void UI_SetText(const char *text);
void UI_SetCursorPosition(int position);
int UI_GetCursorPosition(void);
int UI_InsertText(const char *utf8, int len);
void UI_DeleteForward(void);
void UI_SyncFilterFromEdit(void);
void UI_ApplyFilterToEdit(void);
ListBox *UI_GetPopupListBox(void);
void UI_SyncEditCursor(int pos);
int UI_InsertChar(wchar_t wc);

#define UI_DEBOUNCE_MS 40
#define UI_DEBOUNCE_TIMER_ID 998
#define UI_TICK_TIMER_ID 997
#define UI_TICK_TIMER_MS 1000

typedef struct {
  const void *slots;
  int count;
  size_t stride;
  int (*is_empty)(const void *slot);
  unsigned long (*stamp)(const void *slot);
} BgTileLruPick;

int bg_tile_lru_pick(BgTileLruPick *argspec);

void draw_text_centered_impl(const DrawTextCentered *spec, int line_height);

typedef struct {
  int rect;
  int rect_size;
  int slot;
  int gap;
  int outward;
  int win_size;
} MsgAnchorCenter;

int msg_anchor_center(MsgAnchorCenter *argspec);
int msg_stack_dir(int v_slot, bool outward);

#endif
