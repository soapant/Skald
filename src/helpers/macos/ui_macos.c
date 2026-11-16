#include <stdbool.h>
#include <stddef.h>
#include "core/types.h"
#include "helpers/helpers_ui.h"

bool is_popup_active(WindowHandle hPopupWnd) {
  (void)hPopupWnd;
  return false;
}

void UI_RequestPopupRepaint(void) {}

void UI_LegacySyncRepaint(void) {}

void UI_GetCurrentTextEdit(char *buffer, size_t cap) {
  if (!buffer || cap == 0)
    return;
  buffer[0] = '\0';
}

void UI_SetText(const char *text) { (void)text; }

int UI_GetCursorPosition(void) { return 0; }

void UI_SetCursorPosition(int position) { (void)position; }
