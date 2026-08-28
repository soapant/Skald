#ifndef UI_WIN32_H
#define UI_WIN32_H

#include "core/types.h"

static inline void UI_SetSelection(ListBoxInfo* lb, WindowHandle hList, int index);
static inline int UI_GetCount(ListBoxInfo* lb, WindowHandle hList);
static inline void UI_SetHorizontalExtent(WindowHandle hList, int width);
static inline void UI_SetItemCount(ListBoxInfo* lb, WindowHandle hList, int count);
static inline void UI_LB_SetCount(ListBoxInfo* lb, WindowHandle hList, int count);
static inline void UI_LB_SetItemHeight(WindowHandle hList, int height);
static inline void UI_LB_SetCaretIndex(WindowHandle hList, int index, BOOL scroll);
static inline void UI_LB_SetTopIndex(WindowHandle hList, int index);
static inline int UI_LB_GetTopIndex(WindowHandle hList);
static inline void UI_LB_SetFont(WindowHandle hList, HFONT hFont);
static inline int UI_LB_GetItemHeight(WindowHandle hList);
static inline void UI_NavigateTo(ListBoxInfo* lb, WindowHandle hListBox, int index);
static inline void UI_NavigateUp(ListBoxInfo* lb, WindowHandle hListBox);
static inline void UI_NavigateDown(ListBoxInfo* lb, WindowHandle hListBox);
static inline int UI_LB_GetTextLen(WindowHandle hList);
static inline int UI_LB_GetText(WindowHandle hList, int index, char* buffer);
static inline void UI_Edit_SetFont(WindowHandle hEdit, HFONT hFont);
static inline HFONT UI_GetFont(WindowHandle hWnd);
static inline void UI_Edit_UpdateUIState(WindowHandle hEdit, UINT action, UINT uiFlags);
static inline void UI_Edit_SetSel(WindowHandle hEdit, DWORD start, DWORD end);
static inline void UI_Edit_GetSel(WindowHandle hEdit, uint32_t* start, uint32_t* end);
static inline void UI_Edit_KeyDown(WindowHandle hEdit, WPARAM vk);
static inline void UI_Edit_KeyUp(WindowHandle hEdit, WPARAM vk);
#define UI_GetCurrentText(edit, buffer) GetWindowTextA(WH_TO_HWND(edit), buffer, sizeof(buffer))
static inline void UI_SetText(WindowHandle hEdit, char* text);
static inline void UI_SetCursorPosition(WindowHandle ctrl, int position);
static inline void UI_GetCurrentCursorPosition(uint32_t *start, uint32_t *end);
#define UI_DEBOUNCE_MS 0
#define UI_DEBOUNCE_TIMER_ID 998
static inline void UI_Redraw(ListBoxInfo* lb, WindowHandle hListBox);
static inline void UI_RedrawBackground(HDC hdc, RECT* rect, BOOL is_selected);
static inline void UI_GetParent(WindowHandle hwnd);
static inline void UI_FlushPendingInput(void);
static inline void UI_PreparePopupPump(void);
static inline bool UI_ShouldSkipPopupKey(const MSG* msg);

#endif
