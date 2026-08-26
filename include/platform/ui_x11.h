#ifndef UI_X11_H
#define UI_X11_H

#include "core/types.h"
#include "platform/platform.h"

extern PromptState* g_currentPromptState;

static inline void UI_SetSelection(ListBoxInfo* lb, WindowHandle hList, int index) {
  (void)hList;
  if (!lb) return;
  if (lb->selected_index == index) return;
  Platform_DismissMessage();
  lb->selected_index = index; }

static inline int UI_GetCount(ListBoxInfo* lb, WindowHandle hList) {
  (void)hList;
  if (!lb) return 0;
  return lb->populated_count;
}

static inline void UI_SetHorizontalExtent(WindowHandle hList, int width) {
  (void)hList; (void)width;
}

static inline void UI_SetItemCount(ListBoxInfo* lb, WindowHandle hList, int count) {
  (void)hList;
  if (lb) lb->populated_count = count;
}

static inline void UI_LB_SetCount(ListBoxInfo* lb, WindowHandle hList, int count) {
  (void)hList;
  if (lb) lb->populated_count = count;
}

static inline void UI_LB_SetItemHeight(WindowHandle hList, int height) {
  (void)hList; (void)height;
}

static inline void UI_LB_SetCaretIndex(WindowHandle hList, int index, bool scroll) {
  (void)hList; (void)index; (void)scroll;
}

static inline void UI_LB_SetTopIndex(WindowHandle hList, int index) {
  (void)hList; (void)index;
}

static inline int UI_LB_GetTopIndex(WindowHandle hList) {
  (void)hList;
  return 0;
}

static inline void UI_LB_SetFont(WindowHandle hList, void* hFont) {
  (void)hList; (void)hFont;
}

static inline int UI_LB_GetItemHeight(WindowHandle hList) {
  (void)hList;
  return 0;
}

static inline void UI_NavigateTo(ListBoxInfo* lb, WindowHandle hListBox, int index) {
  if (!lb) return;
  int count = lb->populated_count;
  if (count <= 0) {
    if (lb->selected_index != -1) {
      Platform_DismissMessage();
    }
    lb->selected_index = -1;
    UI_LB_SetCaretIndex(hListBox, -1, FALSE);
    return;
  }
  if (index >= count) index = count - 1;
  if (index == -1) {
    if (lb->selected_index != -1) {
      Platform_DismissMessage();
    }
    lb->selected_index = -1;
    UI_LB_SetCaretIndex(hListBox, -1, FALSE);
    return;
  }

  int old_sel = lb->selected_index;
  if (old_sel == index) return;
  Platform_DismissMessage();
  int oldTop = UI_LB_GetTopIndex(hListBox);
  lb->selected_index = index;

  UI_LB_SetCaretIndex(hListBox, index, FALSE);

  int visibleCount = 16;

  int newTop = oldTop;
  if (index <= oldTop + PaddingForScrolling) {
    newTop = index - PaddingForScrolling;
  } else if (index >= oldTop + visibleCount - PaddingForScrolling) {
    newTop = index - visibleCount + PaddingForScrolling + 1;
  }
  if (newTop < 0) newTop = 0;
  if (newTop + visibleCount > count) newTop = count - visibleCount;
  if (newTop < 0) newTop = 0;

  if (newTop != oldTop) {
    UI_LB_SetTopIndex(hListBox, newTop);
  }
}

static inline void UI_NavigateUp(ListBoxInfo* lb, WindowHandle hListBox) {
  if (!lb) return;
  int sel = lb->selected_index;
  if (sel >= 0) {
    sel--;
  }
  if (sel == lb->selected_index) return;
  UI_NavigateTo(lb, hListBox, sel);
}

static inline void UI_NavigateDown(ListBoxInfo* lb, WindowHandle hListBox) {
  if (!lb) return;
  int count = lb->populated_count;
  if (count <= 0) return;
  int sel = lb->selected_index;
  if (sel < 0) sel = -1;
  if (sel >= count - 1) sel = count - 1;
  else sel++;
  if (sel == lb->selected_index) return;
  UI_NavigateTo(lb, hListBox, sel);
}

static inline void UI_Edit_SetSel(WindowHandle hEdit, uint32_t start, uint32_t end) {
  (void)hEdit; (void)start; (void)end;
}

static inline void UI_Edit_GetSel(WindowHandle hEdit, uint32_t* start, uint32_t* end) {
  (void)hEdit;
  if (start) *start = 0;
  if (end) *end = 0;
}

static inline void UI_Edit_KeyDown(WindowHandle hEdit, int vk) {
  (void)hEdit; (void)vk;
}
static inline void UI_Edit_KeyUp(WindowHandle hEdit, int vk) {
  (void)hEdit; (void)vk;
}

#define UI_GetCurrentText(edit, buffer) UI_GetCurrentTextEdit(edit, buffer, sizeof(buffer))

// Reads the text of whatever prompt surface is active.
static inline void UI_GetCurrentTextEdit(WindowHandle edit, char* buffer, size_t cap) {
  (void)edit;
  if (!buffer || cap == 0) return;
  buffer[0] = '\0';
  extern SimplePromptState* g_currentSimplePromptState;
  extern SimplePromptState* g_currentEditPopupState;
  if (g_currentEditPopupState) {
    strncpy(buffer, g_currentEditPopupState->user_input, cap-1);
    buffer[cap-1]='\0';
  } else if (g_currentPromptState) {
    strncpy(buffer, g_currentPromptState->filter, cap-1);
    buffer[cap-1]='\0';
  } else if (g_currentSimplePromptState) {
    strncpy(buffer, g_currentSimplePromptState->user_input, cap-1);
    buffer[cap-1]='\0';
  }
}

static inline void UI_SetText(WindowHandle hEdit,char* text) {
  (void)text;
  // Linux prompts keep text in PromptState; clearing is the only legacy use.
  if (g_currentPromptState) {
    g_currentPromptState->filter[0] = '\0';
    g_currentPromptState->cursor = 0;
  }
}

static inline void UI_SetCursorPosition(WindowHandle ctrl,int position) {
  (void)ctrl; (void)position;
}
static inline void UI_GetCurrentCursorPosition(uint32_t *start, uint32_t *end) {
  if (start) *start = 0;
  if (end) *end = 0;
}

static inline void UI_Redraw(ListBoxInfo* lb, WindowHandle hListBox) {
  UI_RedrawNow(lb, hListBox);
}

static inline void UI_GetParent(WindowHandle hwnd) {
  (void)hwnd;
}

#endif
