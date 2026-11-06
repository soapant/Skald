#include <windows.h>
#include <gdiplus.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "deps/log.h"
#include "config.h"
#include "core/context.h"
#include "core/window_manager.h"
#include "platform/win32/win32_state.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include "platform/win32/common_ui_win32.h"

#include "core/timer.h"
#include "core/listbox_hover.h"
#include "core/prompt_keys.h"
bool IsPopupActive(WindowHandle hPopupWnd) {
  (void)hPopupWnd;
  return WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL &&
         IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
}

void UI_RequestPopupRepaint(void) {
  HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
  if (!hPopup || !IsWindow(hPopup)) {
    return;
  }
  extern void win32_popup_request_repaint(HWND hPopup);
  win32_popup_request_repaint(hPopup);
}

void UI_LegacySyncRepaint(void) {
  HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
  if (hPopup && IsWindow(hPopup)) {
    RedrawWindow(hPopup, NULL, NULL,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
  }
}

void UI_SetSelection(ListBoxInfo *lb, int index) {
  if (!lb) {
    return;
  }
  int old_sel = lb->selected_index;
  if (old_sel == index) {
    return;
  }
  lb->selected_index = index;

  UI_RequestPopupRepaint();
}

void UI_SetHorizontalExtent(int width) {}

static int g_lb_update_depth = 0;

void UI_LB_BeginUpdate(void) { g_lb_update_depth++; }

void UI_LB_EndUpdate(void) {
  if (g_lb_update_depth > 0)
    g_lb_update_depth--;
}

void UI_LB_SetCount(ListBoxInfo *lb, int count) {
  if (lb)
    lb->item_count = count;
  UI_RequestPopupRepaint();
}

void UI_LB_SetItemHeight(int height) {}

void UI_LB_SetCaretIndex(int index, bool scroll) { UI_RequestPopupRepaint(); }

int UI_LB_GetViewTop(ListBoxInfo *lb) { return lb ? lb->view_offset : 0; }

static int g_render_lb_visible = 0;

int UI_LB_GetVisibleCount(void) {
  if (FlintContext_Get()->window.prompt_callbacks.render != NULL &&
      g_render_lb_visible > 0)
    return g_render_lb_visible;
  int itemHeight = 0;
  if (itemHeight <= 0)
    return 16;
  RECT rect;
  int visible = rect.bottom / itemHeight;
  if (visible < 1)
    visible = 1;
  return visible;
}

void UI_LB_SetVisibleCount(int count) { g_render_lb_visible = count; }

void UI_LB_ScrollToTop(ListBoxInfo *lb, int top_index) {
  if (!lb) {
    return;
  }
  int visible = UI_LB_GetVisibleCount();
  if (top_index < 0)
    top_index = 0;
  if (visible > 0 && top_index + visible > lb->item_count)
    top_index = lb->item_count - visible;
  if (top_index < 0)
    top_index = 0;
  lb->view_offset = top_index;

  UI_RequestPopupRepaint();
}

void UI_NavigateTo(ListBoxInfo *lb, int index) {
  if (!lb) {
    return;
  }
  int count = lb->item_count;
  if (count <= 0) {
    lb->selected_index = -1;
    UI_RequestPopupRepaint();
    return;
  }
  if (index >= count)
    index = count - 1;
  if (index == -1) {
    lb->selected_index = -1;
    UI_RequestPopupRepaint();
    return;
  }
  int old_sel = lb->selected_index;
  if (old_sel == index) {
    return;
  }
  lb->selected_index = index;
  lb->selected_text = ListBoxInfo_Row(lb, index);
  ListBox_ScrollToSelection(lb);
  UI_RequestPopupRepaint();
}

void UI_NavigateUp(ListBoxInfo *lb) {
  ListBox_NavigateUp(lb);
  UI_RequestPopupRepaint();
}

void UI_NavigateDown(ListBoxInfo *lb) {
  ListBox_NavigateDown(lb);
  UI_RequestPopupRepaint();
}

typedef struct {
  char *text;
  size_t capacity;
  int *cursor_i;
  size_t *cursor_z;
} Win32TextTarget;

static int win32_target_cursor(const Win32TextTarget *t) {
  if (t->cursor_i)
    return *t->cursor_i;
  if (t->cursor_z)
    return (int)*t->cursor_z;
  return 0;
}

static void win32_target_set_cursor(const Win32TextTarget *t, int value) {

  if (t->cursor_i) {
    *t->cursor_i = value;
    return;
  }
  if (t->cursor_z) {
    *t->cursor_z = (size_t)value;
    return;
  }
}

static wchar_t s_pending_high_surrogate = 0;

static bool win32_resolve_text_target(Win32TextTarget *out) {

  out->text = NULL;
  out->capacity = 0;
  out->cursor_i = NULL;
  out->cursor_z = NULL;
  WindowManager *wm = WindowManager_Get();
  Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
  if (top && top->type == POPUP_PROMPT && top->user_data) {
    PromptState *prompt = (PromptState *)top->user_data;
    out->text = prompt->eb->filter;
    out->capacity = sizeof(prompt->eb->filter);
    out->cursor_z = &prompt->eb->cursor;
    return true;
  }
  return false;
}

void UI_Edit_GetSel(uint32_t *start, uint32_t *end) {
  Win32TextTarget t;
  uint32_t cursor = 0;
  if (win32_resolve_text_target(&t))
    cursor = (uint32_t)win32_target_cursor(&t);
  if (start)
    *start = cursor;
  if (end)
    *end = cursor;
}

void UI_GetCurrentTextEdit(char *buffer, size_t cap) {
  if (!buffer || cap == 0) {
    return;
  }
  buffer[0] = '\0';
  Win32TextTarget t;
  if (!win32_resolve_text_target(&t) || !t.text) {
    return;
  }
  snprintf(buffer, cap, "%s", t.text);
  buffer[cap - 1] = '\0';
}

void UI_SetText(const char *text) {
  if (!text)
    text = "";
  Win32TextTarget t;
  if (!win32_resolve_text_target(&t) || !t.text || t.capacity == 0) {
    return;
  }
  strncpy(t.text, text, t.capacity - 1);
  t.text[t.capacity - 1] = '\0';
  UI_SetCursorPosition((int)strlen(t.text));
  if (FlintContext_Get()->window.prompt_callbacks.render == NULL) {
    UI_RequestPopupRepaint();
    return;
  }
  HWND popup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
  if (popup && IsWindow(popup))
    UI_RequestPopupRepaint();
}

int UI_GetCursorPosition(void) {
  Win32TextTarget t;
  if (!win32_resolve_text_target(&t) || !t.text)
    return 0;
  int pos = win32_target_cursor(&t);
  return pos;
}

void UI_SetCursorPosition(int position) {
  s_pending_high_surrogate = 0;
  Win32TextTarget t;
  if (!win32_resolve_text_target(&t) || !t.text) {
    return;
  }
  int len = (int)strlen(t.text);
  if (position < 0)
    position = 0;
  if (position > len)
    position = len;
  win32_target_set_cursor(&t, position);
}

int UI_InsertText(const char *utf8, int len) {
  Win32TextTarget t;
  if (!utf8 || len <= 0)
    return 0;
  if (!win32_resolve_text_target(&t) || !t.text || t.capacity == 0)
    return 0;
  size_t cur = (size_t)win32_target_cursor(&t);
  size_t have = strlen(t.text);
  if (cur > have)
    cur = have;
  if (have + (size_t)len + 1 > t.capacity)
    return 0;
  memmove(t.text + cur + len, t.text + cur, have - cur + 1);
  memcpy(t.text + cur, utf8, (size_t)len);
  win32_target_set_cursor(&t, (int)(cur + len));
  return len;
}

static int win32_next_char_len(const char *text, size_t len, size_t cur) {

  if (cur >= len)
    return 0;
  size_t i = cur + 1;
  while (i < len && ((unsigned char)text[i] & 0xC0) == 0x80)
    i++;
  return (int)(i - cur);
}

void UI_DeleteForward(void) {
  s_pending_high_surrogate = 0;
  Win32TextTarget t;
  if (!win32_resolve_text_target(&t) || !t.text) {
    return;
  }
  size_t have = strlen(t.text);
  size_t cur = (size_t)win32_target_cursor(&t);
  if (cur > have)
    cur = have;
  int n = win32_next_char_len(t.text, have, cur);
  if (n <= 0) {
    return;
  }
  memmove(t.text + cur, t.text + cur + n, have - cur - n + 1);
}

int UI_InsertChar(wchar_t wc) {
  wchar_t pair[2];
  int pairlen = 0;
  if (s_pending_high_surrogate) {
    if (wc >= 0xDC00 && wc <= 0xDFFF) {
      pair[0] = s_pending_high_surrogate;
      pair[1] = wc;
      pairlen = 2;
      s_pending_high_surrogate = 0;
    } else {
      s_pending_high_surrogate = 0;
      return 0;
    }
  } else if (wc >= 0xD800 && wc <= 0xDBFF) {
    s_pending_high_surrogate = wc;
    return 0;
  } else {
    pair[0] = wc;
    pairlen = 1;
  }
  char utf8[8];
  int n = WideCharToMultiByte(CP_UTF8, 0, pair, pairlen, utf8,
                              (int)sizeof(utf8), NULL, NULL);
  if (n <= 0)
    return 0;
  return UI_InsertText(utf8, n);
}

void UI_RedrawNow() {
  HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
  if (hPopup && IsWindow(hPopup)) {
    UI_RequestPopupRepaint();
  }
}

void UI_Redraw(ListBoxInfo *lb) {
  if (FlintContext_Get()->window.prompt_callbacks.render == NULL) {
    HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
    if (hPopup)
      KillTimer(hPopup, UI_DEBOUNCE_TIMER_ID);
    UI_RedrawNow();
    return;
  }
  if (IsPopupActive(FlintContext_Get()->window.hPopupWnd)) {
    SetTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd),
             UI_DEBOUNCE_TIMER_ID, UI_DEBOUNCE_MS, NULL);
    return;
  }
}

#include <gdiplus.h>

PromptState *Platform_GetPromptStateForHWND(HWND hwnd) {
  if (hwnd && IsWindow(hwnd)) {
    PromptState *st = (PromptState *)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    if (st)
      return st;
    HWND parent = GetParent(hwnd);
    if (parent && IsWindow(parent)) {
      st = (PromptState *)GetWindowLongPtrA(parent, GWLP_USERDATA);
      if (st)
        return st;
    }
  }
  WindowManager *wm = WindowManager_Get();
  if (wm) {
    for (int i = wm->popup_count - 1; i >= 0; i--) {
      Popup *p = wm->popups[i];
      if (p && p->user_data && p->type == POPUP_PROMPT) {
        HWND ph = WH_TO_HWND(p->handle);
        if (hwnd && ph && (hwnd == ph || IsChild(ph, hwnd))) {
          return (PromptState *)p->user_data;
        }
      }
    }
    Popup *top = WindowManager_GetTopPopup(wm);
    if (top && top->type == POPUP_PROMPT && top->user_data) {
      return (PromptState *)top->user_data;
    }
  }
  return NULL;
}

PromptState *Platform_GetWin32PromptState(void) {
  return Platform_GetPromptStateForHWND(GetFocus() ? GetFocus()
                                                   : GetForegroundWindow());
}

void UI_SyncFilterFromEdit(FlintContext *ctx) {
  if (!ctx) {
    return;
  }
}

void UI_ApplyFilterToEdit(FlintContext *ctx) {
  if (!ctx) {
    return;
  }
  PromptState *prompt = Platform_GetWin32PromptState();
  if (prompt && prompt->eb)
    prompt->eb->filter[sizeof(prompt->eb->filter) - 1] = '\0';
}

ListBoxInfo *UI_GetPopupListBox(FlintContext *ctx) {
  (void)ctx;
  HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
  if (hPopup && IsWindow(hPopup)) {
    ListBoxInfo *lb = (ListBoxInfo *)GetWindowLongPtrA(hPopup, GWLP_USERDATA);
    if (lb)
      return lb;
  }
  return NULL;
}

void UI_SyncEditCursor(FlintContext *ctx, int pos) {
  if (!ctx) {
    return;
  }
  UI_SetCursorPosition(pos);
  if (ctx->window.prompt_callbacks.render != NULL) {
    HWND hPopup = WH_TO_HWND(ctx->window.hPopupWnd);
    if (hPopup && IsWindow(hPopup))
      UI_RequestPopupRepaint();
  }
}

bool UI_ApplyBackspaceToPopup(FlintContext *ctx) {
  (void)ctx;
  return false;
}
