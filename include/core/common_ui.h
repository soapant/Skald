#ifndef COMMON_UI_H
#define COMMON_UI_H

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#if !defined(_WIN32)
#include <sys/time.h>
#endif
#include "common.h"
#include "core/types.h"
#include "config.h"
#include "core/timer.h"
#if !defined(_WIN32)
#include "platform/platform.h"
#endif

extern bool g_add_alias_to_string;
extern char g_alias_display[MAX_ITEMS][512];
extern int g_alias_display_count;
extern bool g_alias_display_active;
void BuildAliasDisplayItems(const char** items, int count);

#if defined(_WIN32)
static inline void UI_SetSelection(ListBoxInfo* lb, WindowHandle hList, int index) {
  if (!lb) return;
  int old_sel = lb->selected_index;
  if (old_sel == index) return;
  lb->selected_index = index;

  RECT invalidRect = {0};
  bool hasInvalid = false;
  RECT itemRect = {0, 0, 0, 0};

  if (old_sel >= 0 && old_sel < lb->populated_count) {
    SendMessageA(WH_TO_HWND(hList), LB_GETITEMRECT, old_sel, (LPARAM)&itemRect);
    invalidRect = itemRect;
    hasInvalid = true;
  }
  if (index >= 0 && index < lb->populated_count) {
    SendMessageA(WH_TO_HWND(hList), LB_GETITEMRECT, index, (LPARAM)&itemRect);
    if (!hasInvalid) {
      invalidRect = itemRect;
      hasInvalid = true;
    } else {
      UnionRect(&invalidRect, &invalidRect, &itemRect);
    }
  }

  if (hasInvalid) {
    RedrawWindow(WH_TO_HWND(hList), &invalidRect, NULL, RDW_INVALIDATE);
  }
}
#else
static inline void UI_SetSelection(ListBoxInfo* lb, WindowHandle hList, int index) {
  (void)hList;
  if (!lb) return;
  if (lb->selected_index == index) return;
  Platform_DismissMessage();
  lb->selected_index = index;
}
#endif

static inline int UI_GetCount(ListBoxInfo* lb, WindowHandle hList) {
  (void)hList;
  if (!lb) return 0;
  return lb->populated_count;
}
static inline bool UI_IsListBoxSelected(ListBoxInfo* lb) {
  return lb && lb->selected_index != -1;
}

bool GetFromHistory(ListBoxInfo* lb);

#if defined(_WIN32)
static inline void UI_SetHorizontalExtent(WindowHandle hList, int width) {
  SendMessageA(WH_TO_HWND(hList), LB_SETHORIZONTALEXTENT, width, 0);
}

static inline void UI_SetItemCount(ListBoxInfo* lb, WindowHandle hList, int count) {
  if (lb) lb->populated_count = count;
  SendMessageA(WH_TO_HWND(hList), LB_SETCOUNT, count, 0);
}

static inline void UI_LB_SetCount(ListBoxInfo* lb, WindowHandle hList, int count) {
  if (lb) lb->populated_count = count;
  SendMessageA(WH_TO_HWND(hList), LB_SETCOUNT, count, 0);
}

static inline void UI_LB_SetItemHeight(WindowHandle hList, int height) {
  SendMessageA(WH_TO_HWND(hList), LB_SETITEMHEIGHT, 0, height);
}
#endif

#if defined(_WIN32)
static inline void UI_LB_SetCaretIndex(WindowHandle hList, int index, BOOL scroll) {
  SendMessageA(WH_TO_HWND(hList), LB_SETCARETINDEX, index, scroll);
}
#else
static inline void UI_LB_SetCaretIndex(WindowHandle hList, int index, bool scroll) {
  (void)hList; (void)index; (void)scroll;
}
#endif

#if defined(_WIN32)
static inline void UI_LB_SetTopIndex(WindowHandle hList, int index) {
  SendMessageA(WH_TO_HWND(hList), LB_SETTOPINDEX, index, 0);
}

static inline int UI_LB_GetTopIndex(WindowHandle hList) {
  return (int)SendMessageA(WH_TO_HWND(hList), LB_GETTOPINDEX, 0, 0);
}

static inline void UI_LB_SetFont(WindowHandle hList, HFONT hFont) {
  SendMessageA(WH_TO_HWND(hList), WM_SETFONT, (WPARAM)hFont, FALSE);
}

static inline int UI_LB_GetItemHeight(WindowHandle hList) {
  return (int)SendMessageA(WH_TO_HWND(hList), LB_GETITEMHEIGHT, 0, 0);
}
#else
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

static inline void UI_LB_SetCount(ListBoxInfo* lb, WindowHandle hList, int count) {
  if (lb) lb->populated_count = count;
  (void)hList;
}
#endif

static inline void UI_NavigateTo(ListBoxInfo* lb, WindowHandle hListBox, int index) {
  if (!lb) return;
  int count = lb->populated_count;
  if (count <= 0) {
    if (lb->selected_index != -1) {
#if !defined(_WIN32)
      Platform_DismissMessage();
#endif
    }
    lb->selected_index = -1;
    UI_LB_SetCaretIndex(hListBox, -1, FALSE);
    return;
  }
  if (index >= count) index = count - 1;
  if (index == -1) {
    if (lb->selected_index != -1) {
#if !defined(_WIN32)
      Platform_DismissMessage();
#endif
    }
    lb->selected_index = -1;
    UI_LB_SetCaretIndex(hListBox, -1, FALSE);
    return;
  }

  int old_sel = lb->selected_index;
  if (old_sel == index) return;
#if !defined(_WIN32)
  Platform_DismissMessage();
#endif
  int oldTop = UI_LB_GetTopIndex(hListBox);
  lb->selected_index = index;

  UI_LB_SetCaretIndex(hListBox, index, FALSE);

  int itemHeight = UI_LB_GetItemHeight(hListBox);
#if defined(_WIN32)
  RECT rect;
  GetClientRect(WH_TO_HWND(hListBox), &rect);
  int visibleCount = (itemHeight > 0) ? (rect.bottom / itemHeight) : 16;
#else
  (void)itemHeight;
  int visibleCount = 16;
#endif
  if (visibleCount < 1) visibleCount = 1;

  int newTop = oldTop;
  if (index <= oldTop + PaddingForScrolling) {
    newTop = index - PaddingForScrolling;
  } else if (index >= oldTop + visibleCount - PaddingForScrolling) {
    newTop = index - visibleCount + PaddingForScrolling + 1;
  }
  if (newTop < 0) newTop = 0;
  if (newTop + visibleCount > count) newTop = count - visibleCount;
  if (newTop < 0) newTop = 0;

#if defined(_WIN32)
  RECT invalidRect = {0};
  bool hasInvalid = false;

  if (newTop != oldTop) {
    UI_LB_SetTopIndex(hListBox, newTop);
    hasInvalid = true;
  }

  RECT itemRect;
  if (old_sel >= 0 && old_sel != index && old_sel < lb->populated_count) {
    SendMessageA(WH_TO_HWND(hListBox), LB_GETITEMRECT, old_sel, (LPARAM)&itemRect);
    if (!hasInvalid) {
      invalidRect = itemRect;
      hasInvalid = true;
    } else {
      UnionRect(&invalidRect, &invalidRect, &itemRect);
    }
  }
  if (index >= 0 && index < lb->populated_count) {
    SendMessageA(WH_TO_HWND(hListBox), LB_GETITEMRECT, index, (LPARAM)&itemRect);
    if (!hasInvalid) {
      invalidRect = itemRect;
      hasInvalid = true;
    } else {
      UnionRect(&invalidRect, &invalidRect, &itemRect);
    }
  }

  if (hasInvalid) {
    RedrawWindow(WH_TO_HWND(hListBox), &invalidRect, NULL, RDW_INVALIDATE);
  }
#else
  if (newTop != oldTop) {
    UI_LB_SetTopIndex(hListBox, newTop);
  }
#endif
}

static inline void UI_NavigateUp(ListBoxInfo* lb, WindowHandle hListBox) {
  if (!lb) return;
  int count = lb->populated_count;
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

#if defined(_WIN32)
static inline int UI_LB_GetTextLen(WindowHandle hList) {
  return (int)SendMessageA(WH_TO_HWND(hList), LB_GETTEXTLEN, 0, 0);
}

static inline int UI_LB_GetText(WindowHandle hList, int index, char* buffer) {
  return (int)SendMessageA(WH_TO_HWND(hList), LB_GETTEXT, index, (LPARAM)buffer);
}

static inline void UI_Edit_SetFont(WindowHandle hEdit, HFONT hFont) {
  SendMessageA(WH_TO_HWND(hEdit), WM_SETFONT, (WPARAM)hFont, FALSE);
}

static inline HFONT UI_GetFont(WindowHandle hWnd) {
  return (HFONT)SendMessageA(WH_TO_HWND(hWnd), WM_GETFONT, 0, 0);
}

static inline void UI_Edit_UpdateUIState(WindowHandle hEdit, UINT action, UINT uiFlags) {
  SendMessageA(WH_TO_HWND(hEdit), WM_UPDATEUISTATE, MAKEWPARAM(action, uiFlags), 0);
}
#endif

#if defined(_WIN32)
static inline void UI_Edit_SetSel(WindowHandle hEdit, DWORD start, DWORD end) {
  SendMessageA(WH_TO_HWND(hEdit), EM_SETSEL, start, end);
}
#else
static inline void UI_Edit_SetSel(WindowHandle hEdit, uint32_t start, uint32_t end) {
  (void)hEdit; (void)start; (void)end;
}
#endif

#if defined(_WIN32)
static inline void UI_Edit_GetSel(WindowHandle hEdit, uint32_t* start, uint32_t* end) {
  SendMessageA(WH_TO_HWND(hEdit), EM_GETSEL, (WPARAM)start, (LPARAM)end);
}
#else
static inline void UI_Edit_GetSel(WindowHandle hEdit, uint32_t* start, uint32_t* end) {
  (void)hEdit;
  if (start) *start = 0;
  if (end) *end = 0;
}
#endif

#if defined(_WIN32)
static inline void UI_Edit_KeyDown(WindowHandle hEdit, WPARAM vk) {
  SendMessageA(WH_TO_HWND(hEdit), WM_KEYDOWN, vk, 0);
}
static inline void UI_Edit_KeyUp(WindowHandle hEdit, WPARAM vk) {
  SendMessageA(WH_TO_HWND(hEdit), WM_KEYUP, vk, 0);
}
#else
static inline void UI_Edit_KeyDown(WindowHandle hEdit, int vk) {
  (void)hEdit; (void)vk;
}
static inline void UI_Edit_KeyUp(WindowHandle hEdit, int vk) {
  (void)hEdit; (void)vk;
}
#endif

void PopulateListBox(ListBoxInfo* lb, WindowHandle hListBox, const char* filter, 
    const char** items, int item_count,
    bool (*filter_match)(const char* item, const char* filter));

ColorRef GetRainbowColor(float t, float s, float v);
#if defined(_WIN32)
HFONT CreateCustomNerdFont(int height, const char* face_name);
#endif
void UpdateTextCaret(WindowHandle hwndEditControl);
void CalculateWindowMetrics(int count, WindowLayout* layout,int height , int width,int item_height);
void CalculateSettingsWindowMetrics(int count, WindowLayout* layout,int height , int width,int item_height);
#if defined(_WIN32)
void SimpleCreateChildControls(WindowHandle hwndParent, HFONT hFont, const char* string, int count, const WindowLayout* layout);
void SettingsCreateChildControls(WindowHandle hwndParent, HFONT hFont,const WindowLayout* layout);
void ForceWindowFocus(WindowHandle hwndTarget, WindowHandle hwndInput);
void CreateChildControls(WindowHandle hwndParent, HFONT hFont, const char* list[], int count, const WindowLayout* layout, ListBoxInfo* lb);
#endif

#if defined(_WIN32)
static inline bool IsPopupActive(WindowHandle hPopupWnd) {
  return WH_TO_HWND(hPopupWnd) != NULL && IsWindow(WH_TO_HWND(hPopupWnd));
}
#else
extern PromptState* g_currentPromptState;
extern SimplePromptState* g_currentSimplePromptState;
extern SimplePromptState* g_currentEditPopupState;
extern bool g_EditPopupCloseRequested;
extern bool g_IsMainWindowActive;
static inline bool IsPopupActive(WindowHandle hPopupWnd) {
  (void)hPopupWnd;
  return g_IsMainWindowActive || g_currentPromptState != NULL || g_currentSimplePromptState != NULL || g_currentEditPopupState != NULL;
}
#endif

#if defined(_WIN32)
static inline bool IsWindowValid(WindowHandle h) {
  return WH_TO_HWND(h) != NULL && IsWindow(WH_TO_HWND(h));
}
#else
static inline bool IsWindowValid(WindowHandle h) {
  return WH_TO_HWND(h) != NULL;
}
#endif

static inline bool CheckAndBlockIfPopupActive(WindowHandle hPopupWnd) {
#if !defined(_WIN32)
  extern bool g_PromptJustClosed;
  extern long g_PromptCloseTimeMs;
  if (g_PromptJustClosed) {
    struct timeval tv; gettimeofday(&tv, NULL);
    long now = tv.tv_sec*1000 + tv.tv_usec/1000;
    if (now - g_PromptCloseTimeMs < 600) {
      return true;
    }
    g_PromptJustClosed = false;
  }
#endif
  return IsPopupActive(hPopupWnd);
}
static inline void UI_GetCurrentCursorPosition(uint32_t *start, uint32_t *end) {
  if (start && end) {
    UI_Edit_GetSel(g_ActiveEditControl, start, end);
  }
}
static inline void UI_SetCursorPosition(WindowHandle ctrl,int position) {
  UI_Edit_SetSel(ctrl, position, position);
}
#if defined(_WIN32)
#define UI_GetCurrentText(edit, buffer) GetWindowTextA(WH_TO_HWND(edit), buffer, sizeof(buffer))
#else
#define UI_GetCurrentText(edit, buffer) (0)
#endif
#if defined(_WIN32)
static inline void UI_SetText(WindowHandle hEdit,char* text) {
  SetWindowTextA(WH_TO_HWND(hEdit),text);
}
#else
static inline void UI_SetText(WindowHandle hEdit,char* text) {
    strncpy(g_currentPromptState->filter,"", sizeof(g_currentPromptState->filter)-1);
    g_currentPromptState->filter[sizeof(g_currentPromptState->filter)-1]='\0';
    g_currentPromptState->cursor = (int)strlen(g_currentPromptState->filter);
}
#endif

#define UI_DEBOUNCE_MS 0
#define UI_DEBOUNCE_TIMER_ID 998

static inline void UI_RedrawNow(ListBoxInfo* lb, WindowHandle hListBox) {
  if (g_refresh_callback != NULL) {
    char buffer[128];
    UI_GetCurrentText(hEditControl,buffer);
    g_refresh_callback(buffer,hListBox,lb);
  }
}

#if defined(_WIN32)
static inline void UI_Redraw(ListBoxInfo* lb, WindowHandle hListBox) {
  if (IsPopupActive(hPopupWnd)) {
    SetTimer(WH_TO_HWND(hPopupWnd), UI_DEBOUNCE_TIMER_ID, UI_DEBOUNCE_MS, NULL);
    return;
  }
  UI_RedrawNow(lb, hListBox);
}
#else
static inline void UI_Redraw(ListBoxInfo* lb, WindowHandle hListBox) {
  UI_RedrawNow(lb, hListBox);
}
#endif
#if defined(_WIN32)
static inline void UI_RedrawBackground(HDC hdc,RECT* rect,BOOL is_selected) {
  HBRUSH hBgBrush = CreateSolidBrush(is_selected ? COLOR_SELECTED_TEXT : COLOR_BG);
  FillRect(hdc, rect, hBgBrush);
  DeleteObject(hBgBrush);
}
#endif

static bool g_toggle_key_pending = false;

#if defined(_WIN32)
static inline void UI_FlushPendingInput(void) {
  MSG msg;
  while (PeekMessageA(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) {
    if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) {
      g_toggle_key_pending = false;
    }
  }
}

static inline void UI_PreparePopupPump(void) {
  g_toggle_key_pending = true;
  UI_FlushPendingInput();
}

static inline bool UI_ShouldSkipPopupKey(const MSG* msg) {
  if (!g_toggle_key_pending) return false;
  if (msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN ||
      msg->message == WM_KEYUP || msg->message == WM_SYSKEYUP ||
      msg->message == WM_CHAR || msg->message == WM_SYSCHAR) {
    g_toggle_key_pending = false;
    return true;
  }
  return false;
}
#endif
#if defined(_WIN32)
static inline void UI_GetParent(WindowHandle hwnd) {
  SendMessageA(GetParent(WH_TO_HWND(hwnd)), WM_COMMAND, MAKEWPARAM(IDC_INPUT_BOX, EN_CHANGE), (LPARAM)WH_TO_HWND(hEditControl));
}
#else
static inline void UI_GetParent(WindowHandle hwnd) {
  (void)hwnd;
}
#endif

#endif
