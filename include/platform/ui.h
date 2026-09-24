#ifndef PLATFORM_UI_H
#define PLATFORM_UI_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "core/context.h"
#include "core/modal.h"
#include "core/types.h"
#include "config.h"
#include "core/timer.h"
#include "core/platform.h"
#include "helpers/helpers_ui.h"

bool IsPopupActive(WindowHandle hPopupWnd);

void UI_SetSelection(ListBoxInfo* lb, WindowHandle hList, int index);
void UI_SetHorizontalExtent(WindowHandle hList, int width);
void UI_LB_SetCount(ListBoxInfo* lb, WindowHandle hList, int count);
void UI_LB_SetItemHeight(WindowHandle hList, int height);
void UI_LB_SetCaretIndex(WindowHandle hList, int index, bool scroll);
void UI_LB_SetTopIndex(WindowHandle hList, int index);
int UI_LB_GetTopIndex(WindowHandle hList);
void UI_LB_SetFont(WindowHandle hList, void* hFont);
int UI_LB_GetItemHeight(WindowHandle hList);
void UI_NavigateTo(ListBoxInfo* lb, WindowHandle hListBox, int index);
void UI_NavigateUp(ListBoxInfo* lb, WindowHandle hListBox);
void UI_NavigateDown(ListBoxInfo* lb, WindowHandle hListBox);
void UI_Edit_SetSel(WindowHandle hEdit, uint32_t start, uint32_t end);
void UI_Edit_GetSel(WindowHandle hEdit, uint32_t* start, uint32_t* end);
void UI_GetCurrentTextEdit(WindowHandle edit, char* buffer, size_t cap);
void UI_SetText(WindowHandle hEdit, char* text);
void UI_SetCursorPosition(WindowHandle ctrl, int position);
void UI_Redraw(ListBoxInfo* lb, WindowHandle hListBox);
void UI_RedrawNow(ListBoxInfo* lb, WindowHandle hListBox);
void UI_GetParent(WindowHandle hwnd);

#define UI_DEBOUNCE_MS 0
#define UI_DEBOUNCE_TIMER_ID 998
#define UI_TICK_TIMER_ID 997
#define UI_TICK_TIMER_MS 1000

#if defined(_WIN32)
#include <windows.h>
void win32_apply_rounded_corners(HWND hwnd, int w, int h);
void win32_draw_background(HDC hdc, RECT* rc);
void win32_draw_selection_background(HDC hdc, RECT* rc, COLORREF base);
void win32_draw_background_slice(HDC hdc, RECT* rc, COLORREF base, int containerW, int containerH, int offsetX, int offsetY);
void win32_fill_rounded_rect(HDC hdc, RECT* rc, COLORREF col, int radius);
void win32_fill_rounded_rect_slice(HDC hdc, RECT* rc, COLORREF base, int containerW, int containerH, int offsetX, int offsetY, int radius);
void win32_draw_rounded_selection(HDC hdc, RECT* rc, COLORREF base, int radius);
void ForceWindowFocus(WindowHandle hwndTarget, WindowHandle hwndInput);
HFONT CreateCustomNerdFont(int height, const char* face_name);
void CreateChildControls(WindowHandle hwndParent, HFONT hFont, const char* list[], int count, const WindowLayout* layout, ListBoxInfo* lb);
void SettingsCreateChildControls(WindowHandle hwndParent, HFONT hFont,const WindowLayout* layout);
int UI_LB_GetTextLen(WindowHandle hList);
int UI_LB_GetText(WindowHandle hList, int index, char* buffer);
void UI_Edit_SetFont(WindowHandle hEdit, HFONT hFont);
HFONT UI_GetFont(WindowHandle hWnd);
void UI_Edit_UpdateUIState(WindowHandle hEdit, UINT action, UINT uiFlags);
void UI_FlushPendingInput(void);
void UI_PreparePopupPump(void);
bool UI_ShouldSkipPopupKey(const void* msg);
#endif

#endif
