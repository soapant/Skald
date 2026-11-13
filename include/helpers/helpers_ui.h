#ifndef HELPERS_UI_H
#define HELPERS_UI_H

#include <stdbool.h>
#include <wchar.h>
#include "core/types.h"
#include "core/types/window.h"
#include "platform/draw.h"
#include "platform/platform.h"

unsigned long RgbToHex(unsigned long win_color);
unsigned short OpacityAlpha16(void);

unsigned int Color_Lerp(unsigned int a, unsigned int b, float t);

bool Cover_Scale(int sw, int sh, int tw, int th, int *dw, int *dh);

int DrawThicknessPx(float thickness);
void DrawMeasureTrack(int x, int y, int w, int h);
int DrawLinePrep(const PlDrawLineRawSpec *spec, int off_x, int off_y, int *x1,
                 int *y1, int *x2, int *y2);
int DrawCirclePrep(const PlDrawCircleRawSpec *spec, int off_x, int off_y,
                   int *cx, int *cy);
void PopulateListBox(ListBoxInfo *lb, const char *filter, const char **items,
                     int item_count,
                     bool (*filter_match)(const char *item,
                                          const char *filter));

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

ColorRef GetRainbowColor(float t, float s, float v);
void UpdateTextCaret(void);

#define MSG_PROMPT_GAP_DEFAULT 300
#define MSG_TIMEOUT_INDEFINITE 6969.0
#define MsgTimeoutIsIndefinite(t) ((t) == MSG_TIMEOUT_INDEFINITE || (t) <= 0.0)

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

char *PromptUserRun(WindowManager *wm, const PromptSpec *spec);
void Prompt_ProcessPendingSubwindow(void);
bool TerminalGuard(const char *working_dir, const char *extra_args);
PlMetrics *Metrics_Get(void);
void DrawPromptSeparator(PlDrawPromptSeparatorSpec spec);
void Item_GetColors(PlGetItemColorsSpec spec);
void Listbox_ComputeLayout(const PlListSpec *spec, int default_y0, int item_h,
                           int popup_w, int popup_h, int count,
                           ListboxLayout *out);

void MessageSlotToRect(MessageSlot slot, int prompt_gap, int *out_x, int *out_y,
                       int *out_w, int *out_h, int *out_h_slot, int *out_v_slot,
                       int *out_gap, int *out_outward);
void DisplayMessageSlot(const char *message, MessageSlot slot, int max_w,
                        int max_h, int prompt_gap, double timeout,
                        ColorRef color, void (*render)(void), const char *name);

void SetTransparentWindow(bool on);
bool GetTransparentWindow(void);
void SetDrawBgColor(unsigned long hex);
unsigned long GetDrawBgColor(void);
void BeginMeasure(void);
void EndMeasure(int *out_w, int *out_h);
void GetLastMeasureOrigin(int *out_x, int *out_y);
void SetDrawOffset(int dx, int dy);
void GetDrawOffset(int *out_x, int *out_y);

bool IsPopupActive(WindowHandle hPopupWnd);
void UI_SetSelection(ListBoxInfo *lb, int index);
void UI_LB_SetCount(ListBoxInfo *lb, int count);
void UI_LB_BeginUpdate(void);
void UI_LB_EndUpdate(void);
void UI_RequestPopupRepaint(void);
void UI_LegacySyncRepaint(void);
int UI_LB_GetViewTop(ListBoxInfo *lb);
int UI_LB_GetVisibleCount(void);
void UI_LB_SetVisibleCount(int count);
void UI_LB_ScrollToTop(ListBoxInfo *lb, int top_index);
void UI_NavigateTo(ListBoxInfo *lb, int index);
void UI_NavigateUp(ListBoxInfo *lb);
void UI_NavigateDown(ListBoxInfo *lb);

void UI_Edit_GetSel(uint32_t *start, uint32_t *end);
void UI_GetCurrentTextEdit(char *buffer, size_t cap);
void UI_SetText(const char *text);
void UI_SetCursorPosition(int position);
int UI_GetCursorPosition(void);
int UI_InsertText(const char *utf8, int len);
void UI_DeleteForward(void);
void UI_Redraw(ListBoxInfo *lb);
void UI_RedrawNow(void);
void UI_SyncFilterFromEdit(void);
void UI_ApplyFilterToEdit(void);
ListBoxInfo *UI_GetPopupListBox(void);
void UI_SyncEditCursor(int pos);
int UI_InsertChar(wchar_t wc);

#define UI_DEBOUNCE_MS 40
#define UI_DEBOUNCE_TIMER_ID 998
#define UI_TICK_TIMER_ID 997
#define UI_TICK_TIMER_MS 1000

int BgTileLruPick(const void *slots, int count, size_t stride,
                  int (*is_empty)(const void *slot),
                  unsigned long (*stamp)(const void *slot));

void DrawTextCenteredImpl(const PlDrawTextCenteredSpec *spec, int lineHeight);
int MsgAnchorCenter(int rect, int rect_size, int slot, int gap, int outward,
                    int win_size);
int MsgStackDir(int v_slot, bool outward);

#endif
