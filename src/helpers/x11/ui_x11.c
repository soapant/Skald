#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "helpers/helpers_ui.h"
#include "helpers/helpers_text.h"
#include "core/window_manager.h"
#include "core/context.h"
#include "core/listbox_hover.h"
#include "core/prompt_keys.h"
#include "core/types.h"
#include "core/types/window.h"

static PromptState *ui_top_prompt(void) {
  WindowManager *wm = WindowManager_Get();
  if (!wm)
    return NULL;
  Popup *top = WindowManager_GetTopPopup(wm);
  if (!top || top->type != POPUP_PROMPT || !top->user_data)
    return NULL;
  return (PromptState *)top->user_data;
}

bool IsPopupActive(WindowHandle hPopupWnd) {
  (void)hPopupWnd;
  WindowManager *wm = WindowManager_Get();
  return wm && wm->popup_count > 0;
}

void UI_RequestPopupRepaint(void) {}

void UI_LegacySyncRepaint(void) {}

void UI_GetCurrentTextEdit(char *buffer, size_t cap) {
  if (!buffer || cap == 0) {
    return;
  }
  buffer[0] = '\0';
  PromptState *prompt = ui_top_prompt();
  if (prompt) {
    snprintf(buffer, cap, "%s", prompt->eb->filter);
    buffer[cap - 1] = '\0';
  }
}

void UI_SetText(const char *text) {
  if (!text)
    text = "";
  PromptState *prompt = ui_top_prompt();
  if (prompt) {
    SafeCopy(prompt->eb->filter, sizeof(prompt->eb->filter), text);
    prompt->eb->cursor = (int)strlen(prompt->eb->filter);
  }
}

int UI_GetCursorPosition(void) {
  PromptState *prompt = ui_top_prompt();
  return prompt ? (int)prompt->eb->cursor : 0;
}

void UI_SetCursorPosition(int position) {
  PromptState *prompt = ui_top_prompt();
  if (prompt) {
    int len = (int)strlen(prompt->eb->filter);
    if (position < 0)
      position = 0;
    if (position > len)
      position = len;
    prompt->eb->cursor = position;
  }
}

void UI_SyncFilterFromEdit(void) { ui_top_prompt(); }

void UI_ApplyFilterToEdit(void) {
  PromptState *prompt = ui_top_prompt();
  if (prompt)
    prompt->eb->filter[sizeof(prompt->eb->filter) - 1] = '\0';
}

ListBoxInfo *UI_GetPopupListBox(void) {
  PromptState *prompt = ui_top_prompt();
  return prompt ? prompt->lb : NULL;
}

void UI_SyncEditCursor(int pos) {
  PromptState *prompt = ui_top_prompt();
  if (prompt) {
    int len = (int)strlen(prompt->eb->filter);
    if (pos < 0)
      pos = 0;
    if (pos > len)
      pos = len;
    prompt->eb->cursor = pos;
  }
  UI_SetCursorPosition(pos);
}

void UI_Edit_GetSel(uint32_t *start, uint32_t *end) {
  if (start)
    *start = 0;
  if (end)
    *end = 0;
}
