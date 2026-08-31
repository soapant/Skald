#ifndef UI_X11_H
#define UI_X11_H

#include "core/types.h"
#include "core/platform.h"

static inline void UI_SetSelection(ListBoxInfo* lb, WindowHandle hList, int index);
static inline int UI_GetCount(ListBoxInfo* lb, WindowHandle hList);
static inline void UI_SetHorizontalExtent(WindowHandle hList, int width);
static inline void UI_SetItemCount(ListBoxInfo* lb, WindowHandle hList, int count);
static inline void UI_LB_SetCount(ListBoxInfo* lb, WindowHandle hList, int count);
static inline void UI_LB_SetItemHeight(WindowHandle hList, int height);
static inline void UI_LB_SetCaretIndex(WindowHandle hList, int index, bool scroll);
static inline void UI_LB_SetTopIndex(WindowHandle hList, int index);
static inline int UI_LB_GetTopIndex(WindowHandle hList);
static inline void UI_LB_SetFont(WindowHandle hList, void* hFont);
static inline int UI_LB_GetItemHeight(WindowHandle hList);
static inline void UI_NavigateTo(ListBoxInfo* lb, WindowHandle hListBox, int index);
static inline void UI_NavigateUp(ListBoxInfo* lb, WindowHandle hListBox);
static inline void UI_NavigateDown(ListBoxInfo* lb, WindowHandle hListBox);
static inline void UI_Edit_SetSel(WindowHandle hEdit, uint32_t start, uint32_t end);
static inline void UI_Edit_GetSel(WindowHandle hEdit, uint32_t* start, uint32_t* end);
static inline void UI_Edit_KeyDown(WindowHandle hEdit, int vk);
static inline void UI_Edit_KeyUp(WindowHandle hEdit, int vk);
#define UI_GetCurrentText(edit, buffer) UI_GetCurrentTextEdit(edit, buffer, sizeof(buffer))
static inline void UI_GetCurrentTextEdit(WindowHandle edit, char* buffer, size_t cap);
static inline void UI_SetText(WindowHandle hEdit, char* text);
static inline void UI_SetCursorPosition(WindowHandle ctrl, int position);
static inline void UI_GetCurrentCursorPosition(uint32_t *start, uint32_t *end);
static inline void UI_Redraw(ListBoxInfo* lb, WindowHandle hListBox);
static inline void UI_GetParent(WindowHandle hwnd);

#endif
