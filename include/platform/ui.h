#ifndef PLATFORM_UI_H
#define PLATFORM_UI_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "core/context.h"

bool IsPopupActive(WindowHandle hPopupWnd);

void UI_SetSelection(ListBoxInfo *lb, int index);
void UI_SetHorizontalExtent(int width);
void UI_LB_SetCount(ListBoxInfo *lb, int count);
void UI_LB_BeginUpdate(void);
void UI_LB_EndUpdate(void);
void UI_RequestPopupRepaint(void);
void UI_LegacySyncRepaint(void);
void UI_LB_SetItemHeight(int height);
void UI_LB_SetCaretIndex(int index, bool scroll);
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
void UI_Backspace(void);
void UI_DeleteForward(void);
void UI_Redraw(ListBoxInfo *lb);
void UI_RedrawNow();
void UI_SyncFilterFromEdit(FlintContext *ctx);
void UI_ApplyFilterToEdit(FlintContext *ctx);
ListBoxInfo *UI_GetPopupListBox(FlintContext *ctx);
void UI_SyncEditCursor(FlintContext *ctx, int pos);
bool UI_ApplyBackspaceToPopup(FlintContext *ctx);

#define UI_DEBOUNCE_MS 40
#define UI_DEBOUNCE_TIMER_ID 998
#define UI_TICK_TIMER_ID 997
#define UI_TICK_TIMER_MS 1000

#endif
