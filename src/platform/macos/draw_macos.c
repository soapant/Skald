#include "platform/draw.h"
#include "core/context.h"

static int g_draw_offset_x = 0;
static int g_draw_offset_y = 0;
static unsigned long g_draw_bg_color = 0;
static bool g_transparent_window = false;
static int g_measure_w = 0;
static int g_measure_h = 0;
static int g_last_measure_x = 0;
static int g_last_measure_y = 0;

void Platform_SetDrawOffset(int dx, int dy) {
  g_draw_offset_x = dx;
  g_draw_offset_y = dy;
}

void Platform_GetDrawOffset(int *out_x, int *out_y) {
  if (out_x)
    *out_x = g_draw_offset_x;
  if (out_y)
    *out_y = g_draw_offset_y;
}

void Platform_BeginMeasure(void) {
  g_measure_w = 0;
  g_measure_h = 0;
}

void Platform_EndMeasure(int *out_w, int *out_h) {
  if (out_w)
    *out_w = g_measure_w;
  if (out_h)
    *out_h = g_measure_h;
}

void Platform_GetLastMeasureOrigin(int *out_x, int *out_y) {
  if (out_x)
    *out_x = g_last_measure_x;
  if (out_y)
    *out_y = g_last_measure_y;
}

void Platform_SetDrawBgColor(unsigned long hex) { g_draw_bg_color = hex; }

unsigned long Platform_GetDrawBgColor(void) { return g_draw_bg_color; }

void Platform_SetTransparentWindow(bool on) { g_transparent_window = on; }

bool Platform_GetTransparentWindow(void) { return g_transparent_window; }

void Platform_FlushDraw(void) {}

bool Platform_WindowToRoot(WindowHandle h, int *out_x, int *out_y) {
  if (out_x)
    *out_x = 0;
  if (out_y)
    *out_y = 0;
  return false;
}
bool Platform_DrawImageAt(const char *path, int x, int y, int w, int h) {
  return true;
}
bool Platform_DrawRectRaw(PlDrawRectRawSpec spec) { return true; }
void Platform_DrawPromptSeparator(PlDrawPromptSeparatorSpec spec) {}
bool Platform_DrawLineRaw(PlDrawLineRawSpec spec) { return true; }
bool Platform_DrawCircleRaw(PlDrawCircleRawSpec spec) { return true; }
bool Platform_DrawTextRaw(int x, int y, const char *text, unsigned long color,
                          FontHandle font, const char *align, int pad,
                          int fontsize) {
  return true;
}
void Platform_DrawTextCentered(int x, int y, int w, int h, const char *text,
                               unsigned long color, FontHandle font,
                               int fontsize) {}
int Platform_MeasureTextWidth(const char *text, int len, int fontsize) {
  return len * 8;
}
bool Platform_DrawTextLine(int x, int y, const char *text, int len,
                           unsigned long color, int fontsize) {
  return true;
}
void Platform_GetItemColors(int idx, int count, unsigned long *num_color,
                            unsigned long *body_color,
                            unsigned long *plain_color) {}
void Platform_GetTextLayoutMetrics(int *line_h, int *row_h, int *list_top) {
  if (line_h)
    *line_h = 16;
  if (row_h)
    *row_h = 20;
  if (list_top)
    *list_top = 0;
}
int Platform_GetTextLineHeight(int fontsize) { return fontsize > 0 ? fontsize : 16; }
void Platform_DrawPromptEditText(PromptState *state) {}
void Platform_DrawPromptEditCursor(PromptState *state) {}
