#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "platform/ui.h"
#include "core/window_manager.h"
#include "core/context.h"
#include "core/listbox_hover.h"
#include "core/prompt_keys.h"
#include "core/types.h"
#include "core/types/window.h"

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
  WindowManager *wm = WindowManager_Get();
  if (!wm) {
    return;
  }
  Popup *top = WindowManager_GetTopPopup(wm);
  if (!top) {
    return;
  }
  if (top->type == POPUP_PROMPT && top->user_data) {
    PromptState *prompt = (PromptState *)top->user_data;
    snprintf(buffer, cap, "%s", prompt->eb->filter);
    buffer[cap - 1] = '\0';
    buffer[cap - 1] = '\0';
  }
}

void UI_SetText(const char *text) {
  if (!text)
    text = "";
  WindowManager *wm = WindowManager_Get();
  if (!wm) {
    return;
  }
  Popup *top = WindowManager_GetTopPopup(wm);
  if (!top) {
    return;
  }
  if (top->type == POPUP_PROMPT && top->user_data) {
    PromptState *prompt = (PromptState *)top->user_data;
    strncpy(prompt->eb->filter, text, sizeof(prompt->eb->filter) - 1);
    prompt->eb->filter[sizeof(prompt->eb->filter) - 1] = '\0';
    prompt->eb->cursor = (int)strlen(prompt->eb->filter);
  }
}

int UI_GetCursorPosition(void) {
  WindowManager *wm = WindowManager_Get();
  if (!wm)
    return 0;
  Popup *top = WindowManager_GetTopPopup(wm);
  if (!top)
    return 0;
  if (top->type == POPUP_PROMPT && top->user_data) {
    PromptState *prompt = (PromptState *)top->user_data;
    return (int)prompt->eb->cursor;
  }
  return 0;
}

void UI_SetCursorPosition(int position) {
  WindowManager *wm = WindowManager_Get();
  if (!wm) {
    return;
  }
  Popup *top = WindowManager_GetTopPopup(wm);
  if (!top) {
    return;
  }
  if (top->type == POPUP_PROMPT && top->user_data) {
    PromptState *prompt = (PromptState *)top->user_data;
    int len = (int)strlen(prompt->eb->filter);
    if (position < 0)
      position = 0;
    if (position > len)
      position = len;
    prompt->eb->cursor = position;
  }
}

void UI_RedrawNow() {}

void UI_Redraw(ListBoxInfo *lb) { UI_RedrawNow(); }

void UI_SyncFilterFromEdit(void) {
  WindowManager *wm = WindowManager_Get();
  Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
  if (!top) {
    return;
  }
  if (top->type == POPUP_PROMPT && top->user_data) {
    PromptState *prompt = (PromptState *)top->user_data;
    return;
  }
}

void UI_ApplyFilterToEdit(void) {
  WindowManager *wm = WindowManager_Get();
  Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
  if (!top) {
    return;
  }
  if (top->type == POPUP_PROMPT && top->user_data) {
    PromptState *prompt = (PromptState *)top->user_data;
    prompt->eb->filter[sizeof(prompt->eb->filter) - 1] = '\0';
    return;
  }
}

ListBoxInfo *UI_GetPopupListBox(void) {
  WindowManager *wm = WindowManager_Get();
  Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
  if (!top)
    return NULL;
  if (top->type == POPUP_PROMPT && top->user_data) {
    PromptState *prompt = (PromptState *)top->user_data;
    return prompt->lb != NULL ? prompt->lb : NULL;
  }
  return NULL;
}

void UI_SyncEditCursor(int pos) {
  WindowManager *wm = WindowManager_Get();
  Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
  if (!top) {
    return;
  }
  if (top->type == POPUP_PROMPT && top->user_data) {
    PromptState *prompt = (PromptState *)top->user_data;
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
