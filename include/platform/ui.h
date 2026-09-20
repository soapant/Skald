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

#if defined(_WIN32)
#include <windows.h>
void Win32_ApplyRoundedCorners(HWND hwnd, int w, int h);
void Win32_DrawBackground(HDC hdc, RECT* rc);
void Win32_DrawSelectionBackground(HDC hdc, RECT* rc, COLORREF base);
void Win32_DrawBackgroundSlice(HDC hdc, RECT* rc, COLORREF base, int containerW, int containerH, int offsetX, int offsetY);
void Win32_FillRoundedRect(HDC hdc, RECT* rc, COLORREF col, int radius);
void Win32_FillRoundedRectSlice(HDC hdc, RECT* rc, COLORREF base, int containerW, int containerH, int offsetX, int offsetY, int radius);
void Win32_DrawRoundedSelection(HDC hdc, RECT* rc, COLORREF base, int radius);
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
