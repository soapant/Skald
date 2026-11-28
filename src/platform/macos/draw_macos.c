#include <string.h>
#include "platform/draw.h"
#include "core/types/prompt.h"
#include "helpers/helpers_misc.h"

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

bool Platform_DrawImageAt(PlatformDrawImageAt *argspec) {
  (void)argspec->path;
  (void)argspec->x;
  (void)argspec->y;
  (void)argspec->w;
  (void)argspec->h;
  return true;
}

bool Platform_GetImageSize(const char *path, int *w, int *h) {
  if (w)
    *w = 0;
  if (h)
    *h = 0;
  return svg_file_size(path, w, h);
}

bool Platform_DrawRectRaw(DrawRectRawSpec spec) {
  (void)spec;
  return true;
}

bool Platform_DrawLineRaw(DrawLineRaw spec) {
  (void)spec;
  return true;
}

bool Platform_DrawCircleRaw(DrawCircleRaw spec) {
  (void)spec;
  return true;
}

bool Platform_DrawTextRaw(DrawTextRaw spec) {
  (void)spec;
  return true;
}

void Platform_DrawTextCentered(DrawTextCentered spec) { (void)spec; }

int Platform_MeasureTextWidth(MeasureTextWidth spec) {
  if (spec.text && spec.len > 0)
    return spec.len * 8;
  return 0;
}

bool Platform_DrawTextLine(DrawTextLine spec) {
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

int Platform_GetTextLineHeight(int font_size) {
  return font_size > 0 ? font_size : 16;
}
