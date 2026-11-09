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

void UI_SetSelection(ListBoxInfo *lb, int index) {
  lb->selected_index = index;
  return;
}

void UI_SetHorizontalExtent(int width) {}

void UI_LB_SetCount(ListBoxInfo *lb, int count) {
  if (lb)
    lb->item_count = count;
}

void UI_LB_BeginUpdate() {}

void UI_LB_EndUpdate() {}

void UI_RequestPopupRepaint(void) {}

void UI_LegacySyncRepaint(void) {}

void UI_LB_SetItemHeight(int height) {}

void UI_LB_SetCaretIndex(int index, bool scroll) {}

int UI_LB_GetViewTop(ListBoxInfo *lb) { return lb ? lb->view_offset : 0; }

static int g_x11_lb_visible;

int UI_LB_GetVisibleCount(void) { return g_x11_lb_visible; }

void UI_LB_SetVisibleCount(int count) { g_x11_lb_visible = count; }

void UI_LB_ScrollToTop(ListBoxInfo *lb, int top_index) {
  if (!lb) {
    return;
  }
  if (top_index < 0)
    top_index = 0;
  if (g_x11_lb_visible > 0 && top_index + g_x11_lb_visible > lb->item_count)
    top_index = lb->item_count - g_x11_lb_visible;
  if (top_index < 0)
    top_index = 0;
  lb->view_offset = top_index;
}

void UI_NavigateTo(ListBoxInfo *lb, int index) {
  if (!lb) {
    return;
  }
  int count = lb->item_count;
  if (count <= 0) {
    lb->selected_index = -1;
    UI_LB_SetCaretIndex(-1, false);
    return;
  }
  if (index >= count)
    index = count - 1;
  if (index == -1) {
    lb->selected_index = -1;
    UI_LB_SetCaretIndex(-1, false);
    return;
  }
  int old_sel = lb->selected_index;
  ListBox_FocusByKeyboard();
  if (old_sel == index) {
    return;
  }
  lb->selected_index = index;
  lb->selected_text = ListBoxInfo_Row(lb, index);
  UI_LB_SetCaretIndex(index, false);
  ListBox_ScrollToSelection(lb);
}

void UI_NavigateUp(ListBoxInfo *lb) { ListBox_NavigateUp(lb); }

void UI_NavigateDown(ListBoxInfo *lb) { ListBox_NavigateDown(lb); }

void UI_Edit_GetSel(uint32_t *start, uint32_t *end) {
  if (start)
    *start = 0;
  if (end)
    *end = 0;
}

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

void UI_SyncFilterFromEdit(SkaldContext *ctx) {
  if (!ctx) {
    return;
  }
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

void UI_ApplyFilterToEdit(SkaldContext *ctx) {
  if (!ctx) {
    return;
  }
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

ListBoxInfo *UI_GetPopupListBox(SkaldContext *ctx) {
  if (!ctx)
    return NULL;
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

void UI_SyncEditCursor(SkaldContext *ctx, int pos) {
  if (!ctx) {
    return;
  }
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

bool UI_ApplyBackspaceToPopup(SkaldContext *ctx) {
  (void)ctx;
  return false;
}
