#ifndef COMMON_UI_H
#define COMMON_UI_H

#include <windows.h>
#include <shlwapi.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "common.h"
#include "core/types.h"
#include "config.h"
#include "core/timer.h"

extern bool g_add_alias_to_string;
extern char g_alias_display[MAX_ITEMS][512];
extern int g_alias_display_count;
extern bool g_alias_display_active;
void BuildAliasDisplayItems(const char** items, int count);
static inline void UI_SetSelection(HWND hList, int index, int old_sel) {
  if (old_sel == index) return;
  g_SelectedIndex = index;

  RECT itemRect;
  if (old_sel >= 0) {
    SendMessageA(hList, LB_GETITEMRECT, old_sel, (LPARAM)&itemRect);
    InvalidateRect(hList, &itemRect, FALSE);
  }
  if (index >= 0) {
    SendMessageA(hList, LB_GETITEMRECT, index, (LPARAM)&itemRect);
    InvalidateRect(hList, &itemRect, FALSE);
  }
}

static inline int UI_GetCount(HWND hList) {
  return g_populated_count;
}
static inline bool UI_IsListBoxSelected(void) {
  return g_SelectedIndex != -1;
}

bool GetFromHistory(void);

static inline void UI_SetHorizontalExtent(HWND hList, int width) {
  SendMessageA(hList, LB_SETHORIZONTALEXTENT, width, 0);
}

static inline void UI_SetItemCount(HWND hList, int count) {
  g_populated_count = count;
  SendMessageA(hList, LB_SETCOUNT, count, 0);
}

static inline void UI_LB_SetCount(HWND hList, int count) {
  g_populated_count = count;
  SendMessageA(hList, LB_SETCOUNT, count, 0);
}

static inline void UI_LB_SetItemHeight(HWND hList, int height) {
  SendMessageA(hList, LB_SETITEMHEIGHT, 0, height);
}

static inline void UI_LB_SetCaretIndex(HWND hList, int index, BOOL scroll) {
  SendMessageA(hList, LB_SETCARETINDEX, index, scroll);
}

static inline void UI_LB_SetTopIndex(HWND hList, int index) {
  SendMessageA(hList, LB_SETTOPINDEX, index, 0);
}

static inline int UI_LB_GetTopIndex(HWND hList) {
  return (int)SendMessageA(hList, LB_GETTOPINDEX, 0, 0);
}
static inline void UI_LB_SetFont(HWND hList, HFONT hFont) {
  SendMessageA(hList, WM_SETFONT, (WPARAM)hFont, FALSE);
}

static inline int UI_LB_GetItemHeight(HWND hList) {
  return (int)SendMessageA(hList, LB_GETITEMHEIGHT, 0, 0);
}

static inline void UI_NavigateTo(int index) {
  int count = g_populated_count;
  if (count <= 0) {
    g_SelectedIndex = -1;
    InvalidateRect(hListBox, NULL, FALSE);
    return;
  }
  if (index >= count) index = count - 1;
  if (index == -1) {
    g_SelectedIndex = -1;
    InvalidateRect(hListBox, NULL, FALSE);
    return;
  }

  int old_sel = g_SelectedIndex;
  int oldTop = UI_LB_GetTopIndex(hListBox);
  g_SelectedIndex = index;

  UI_LB_SetCaretIndex(hListBox, index, FALSE);

  int itemHeight = UI_LB_GetItemHeight(hListBox);
  RECT rect;
  GetClientRect(hListBox, &rect);
  int visibleCount = (itemHeight > 0) ? (rect.bottom / itemHeight) : 16;
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
  if (newTop != oldTop) {
    UI_LB_SetTopIndex(hListBox, newTop);
  }

  RECT itemRect;
  if (old_sel >= 0 && old_sel != index) {
    SendMessageA(hListBox, LB_GETITEMRECT, old_sel, (LPARAM)&itemRect);
    InvalidateRect(hListBox, &itemRect, FALSE);
  }
  SendMessageA(hListBox, LB_GETITEMRECT, index, (LPARAM)&itemRect);
  InvalidateRect(hListBox, &itemRect, FALSE);
}

static inline void UI_NavigateUp(void) {
  int count = g_populated_count;
  if (count <= 0) return;
  int sel = g_SelectedIndex;
  if (sel >= 0) {
    sel--;
  }
  if (sel == g_SelectedIndex) return;
  UI_NavigateTo(sel);
}

static inline void UI_NavigateDown(void) {
  int count = g_populated_count;
  if (count <= 0) return;
  int sel = g_SelectedIndex;
  if (sel < 0) sel = -1;
  if (sel >= count - 1) sel = count - 1;
  else sel++;
  if (sel == g_SelectedIndex) return;
  UI_NavigateTo(sel);
}

static inline int UI_LB_GetTextLen(HWND hList) {
  return (int)SendMessageA(hList, LB_GETTEXTLEN, 0, 0);
}

static inline int UI_LB_GetText(HWND hList, int index, char* buffer) {
  return (int)SendMessageA(hList, LB_GETTEXT, index, (LPARAM)buffer);
}

static inline void UI_Edit_SetFont(HWND hEdit, HFONT hFont) {
  SendMessageA(hEdit, WM_SETFONT, (WPARAM)hFont, FALSE);
}

static inline void UI_Edit_SetSel(HWND hEdit, DWORD start, DWORD end) {
  SendMessageA(hEdit, EM_SETSEL, start, end);
}

static inline void UI_Edit_GetSel(HWND hEdit, DWORD* start, DWORD* end) {
  SendMessageA(hEdit, EM_GETSEL, (WPARAM)start, (LPARAM)end);
}

static inline void UI_Edit_UpdateUIState(HWND hEdit, UINT action, UINT uiFlags) {
  SendMessageA(hEdit, WM_UPDATEUISTATE, MAKEWPARAM(action, uiFlags), 0);
}

static inline HFONT UI_GetFont(HWND hWnd) {
  return (HFONT)SendMessageA(hWnd, WM_GETFONT, 0, 0);
}

static inline void UI_Edit_KeyDown(HWND hEdit, WPARAM vk) {
  SendMessageA(hEdit, WM_KEYDOWN, vk, 0);
}
static inline void UI_Edit_KeyUp(HWND hEdit, WPARAM vk) {
  SendMessageA(hEdit, WM_KEYUP, vk, 0);
}

void PopulateListBox(HWND hListBox, const char* filter, 
    const char** items, int item_count,
    bool (*filter_match)(const char* item, const char* filter));

COLORREF GetRainbowColor(float t, float s, float v);
HFONT CreateCustomNerdFont(int height, const char* face_name);
void UpdateTextCaret(HWND hwndEditControl);
void CalculateWindowMetrics(int count, WindowLayout* layout,int height , int width,int item_height);
void CalculateSettingsWindowMetrics(int count, WindowLayout* layout,int height , int width,int item_height);
void SimpleCreateChildControls(HWND hwndParent, HFONT hFont, const char* string, int count, const WindowLayout* layout);
void SettingsCreateChildControls(HWND hwndParent, HFONT hFont,const WindowLayout* layout);
void ForceWindowFocus(HWND hwndTarget, HWND hwndInput);
void CreateChildControls(HWND hwndParent, HFONT hFont, const char* list[], int count, const WindowLayout* layout);

static inline bool IsPopupActive(HWND hPopupWnd) {
  return hPopupWnd != NULL && IsWindow(hPopupWnd);
}

static inline bool CheckAndBlockIfPopupActive(HWND hPopupWnd) {
  return IsPopupActive(hPopupWnd);
}
static inline void UI_GetCurrentCursorPosition(HWND hEditControl, DWORD *start, DWORD *end) {
  if (start && end) {
    UI_Edit_GetSel(g_ActiveEditControl, start, end);
  }
}
static inline void UI_SetCursorPosition(HWND g_ActiveEditControl,int position) {
  UI_Edit_SetSel(g_ActiveEditControl, position, position);
}
#define UI_GetCurrentText(edit, buffer) GetWindowTextA(edit, buffer, sizeof(buffer))
static inline void UI_SetText(HWND g_ActiveEditControl,char* text) {
  SetWindowTextA(g_ActiveEditControl,text);
}

#define UI_DEBOUNCE_MS 30
#define UI_DEBOUNCE_TIMER_ID 998

static inline void UI_RedrawNow(HWND hListBox) {
  if (g_refresh_callback != NULL) {
    char buffer[128];
    UI_GetCurrentText(hEditControl,buffer);
    g_refresh_callback(buffer,hListBox);
  }
}

static inline void UI_Redraw(HWND hListBox) {
  if (IsPopupActive(hPopupWnd)) {
    SetTimer(hPopupWnd, UI_DEBOUNCE_TIMER_ID, UI_DEBOUNCE_MS, NULL);
    return;
  }
  UI_RedrawNow(hListBox);
}
static inline void UI_RedrawBackground(HDC hdc,RECT* rect,BOOL is_selected) {
  HBRUSH hBgBrush = CreateSolidBrush(is_selected ? COLOR_SELECTED_TEXT : COLOR_BG);
  FillRect(hdc, rect, hBgBrush);
  DeleteObject(hBgBrush);
}

static bool g_toggle_key_pending = false;

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
static inline void UI_GetParent(HWND hwnd) {
  SendMessageA(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_INPUT_BOX, EN_CHANGE), (LPARAM)hEditControl);
}

#endif
