#include <windows.h>
#include <gdiplus.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "core/types/window.h"
#include "core/types/prompt.h"
#include "core/context.h"
#include "core/window_manager.h"
#include "helpers/win32/win32_state.h"
#include "helpers/helpers_ui.h"
#include "helpers/win32/common_ui_win32.h"
#include "helpers/helpers_text.h"
#include "core/listbox_hover.h"

bool is_popup_active(WindowHandle hPopupWnd) {
  (void)hPopupWnd;
  return WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd) != NULL &&
         IsWindow(WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd));
}

void UI_RequestPopupRepaint(void) {
  HWND hPopup = WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd);
  if (!hPopup || !IsWindow(hPopup)) {
    return;
  }
  extern void win32_popup_request_repaint(HWND hPopup);
  win32_popup_request_repaint(hPopup);
}

void UI_LegacySyncRepaint(void) {
  HWND hPopup = WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd);
  if (hPopup && IsWindow(hPopup)) {
    RedrawWindow(hPopup, NULL, NULL,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
  }
}

void UI_Edit_GetSel(uint32_t *start, uint32_t *end);

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
  safe_copy(t.text, t.capacity, text);
  UI_SetCursorPosition((int)strlen(t.text));
  if (SkaldContext_Get()->window.prompt_callbacks.render == NULL) {
    UI_RequestPopupRepaint();
    return;
  }
  HWND popup = WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd);
  if (popup && IsWindow(popup))
    UI_RequestPopupRepaint();
}

int UI_GetCursorPosition(void) {
  Win32TextTarget t;
  if (!win32_resolve_text_target(&t) || !t.text)
    return 0;
  return win32_target_cursor(&t);
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

PromptState *win32_prompt_state_from_hwnd(HWND hwnd) {
  HWND cur = hwnd;
  for (int depth = 0; cur && depth < 8; depth++) {
    Popup *p = prompt_popup_find_popup_by_handle(WH_FROM_HWND(cur));
    if (p && p->user_data)
      return (PromptState *)p->user_data;
    HWND next = GetParent(cur);
    if (next == cur)
      break;
    cur = next;
  }
  return prompt_popup_get_top_prompt_state();
}

void UI_SyncFilterFromEdit(void) {}

void UI_ApplyFilterToEdit(void) {
  PromptState *prompt = prompt_popup_get_top_prompt_state();
  if (prompt && prompt->eb)
    prompt->eb->filter[sizeof(prompt->eb->filter) - 1] = '\0';
}

ListBox *UI_GetPopupListBox(void) {
  const PromptState *prompt = prompt_popup_get_top_prompt_state();
  return prompt ? prompt->lb : NULL;
}

void UI_SyncEditCursor(int pos) {
  UI_SetCursorPosition(pos);
  SkaldContext *c = SkaldContext_Get();
  if (c && c->window.prompt_callbacks.render != NULL) {
    HWND hPopup = WH_TO_HWND(c->window.hPopupWnd);
    if (hPopup && IsWindow(hPopup))
      UI_RequestPopupRepaint();
  }
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
