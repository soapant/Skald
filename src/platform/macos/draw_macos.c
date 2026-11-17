#include <string.h>
#include "platform/draw.h"
#include "core/types/prompt.h"

void Platform_FlushDraw(void) {}

void Platform_DrawPromptEditbox(PromptState *state) { (void)state; }

void Platform_DrawPromptEditText(PromptState *state) { (void)state; }

void Platform_DrawPromptEditCursor(PromptState *state) { (void)state; }

bool Platform_SetWindowRounded(WindowHandle h, bool rounded, int radius) {
  (void)h;
  (void)rounded;
  (void)radius;
  return false;
}

bool Platform_WindowToRoot(WindowHandle h, int *out_x, int *out_y) {
  (void)h;
  if (out_x)
    *out_x = 0;
  if (out_y)
    *out_y = 0;
  return false;
}

bool Platform_DrawImageAt(const char *path, int x, int y, int w, int h) {
  (void)path;
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  return true;
}

bool Platform_DrawRectRaw(DrawRectRawSpec spec) {
  (void)spec;
  return true;
}

bool Platform_DrawLineRaw(DrawLineRawSpec spec) {
  (void)spec;
  return true;
}

bool Platform_DrawCircleRaw(DrawCircleRawSpec spec) {
  (void)spec;
  return true;
}

bool Platform_DrawTextRaw(DrawTextRawSpec spec) {
  (void)spec;
  return true;
}

void Platform_DrawTextCentered(DrawTextCenteredSpec spec) { (void)spec; }

int Platform_MeasureTextWidth(MeasureTextWidthSpec spec) {
  if (spec.text && spec.len > 0)
    return spec.len * 8;
  return 0;
}

bool Platform_DrawTextLine(DrawTextLineSpec spec) {
  (void)spec;
  return true;
}

void Platform_GetTextLayoutMetrics(int *line_h, int *row_h, int *list_top) {
  if (line_h)
    *line_h = 16;
  if (row_h)
    *row_h = 20;
  if (list_top)
    *list_top = 0;
}

int Platform_GetTextLineHeight(int fontsize) {
  return fontsize > 0 ? fontsize : 16;
}
