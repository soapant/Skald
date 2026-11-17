#ifndef DRAW_H
#define DRAW_H

#include "core/types.h"
#include "core/types/window.h"

#define RECT_RAW_UNPACK(spec)                                                  \
  int x = (spec).x;                                                            \
  int y = (spec).y;                                                            \
  int w = (spec).w;                                                            \
  int h = (spec).h;                                                            \
  unsigned long color = (spec).color;                                          \
  int radius = (spec).radius;                                                  \
  bool rounded = (spec).rounded;                                               \
  bool opaque = (spec).opaque;                                                 \
  float thickness = (spec).thickness;                                          \
  bool filled = (spec).filled

void Platform_FlushDraw(void);

void Platform_DrawPromptEditbox(PromptState *state);
void Platform_DrawPromptEditText(PromptState *state);
void Platform_DrawPromptEditCursor(PromptState *state);
bool Platform_WindowToRoot(WindowHandle h, int *out_x, int *out_y);
bool Platform_SetWindowRounded(WindowHandle win, bool rounded, int radius);
bool Platform_DrawImageAt(const char *path, int x, int y, int w, int h);

#define COLOR_AUTO (~0UL)

typedef struct {
  int x, y, w, h;
  unsigned long color;
  int radius;
  bool rounded;
  bool opaque;
  float thickness;
  bool filled;
} DrawRectRawSpec;

typedef struct {
  int x1, y1, x2, y2;
  unsigned long color;
  float thickness;
} DrawLineRawSpec;

typedef struct {
  int cx, cy;
  int radius;
  unsigned long color;
  float thickness;
} DrawCircleRawSpec;

typedef struct {
  int x, y;
  const char *text;
  unsigned long color;
  FontHandle font;
  const char *align;
  int pad;
  int fontsize;
} DrawTextRawSpec;

typedef struct {
  int x, y, w, h;
  const char *text;
  unsigned long color;
  FontHandle font;
  int fontsize;
} DrawTextCenteredSpec;

typedef struct {
  int x, y;
  const char *text;
  int len;
  unsigned long color;
  int fontsize;
} DrawTextLineSpec;

typedef struct {
  int idx, count;
  unsigned long *num_color, *body_color, *plain_color;
} GetItemColorsSpec;

typedef struct {
  int y, width;
} DrawPromptSeparatorSpec;

typedef struct {
  const char *text;
  int len;
  int fontsize;
} MeasureTextWidthSpec;

bool Platform_DrawRectRaw(DrawRectRawSpec spec);
bool Platform_DrawLineRaw(DrawLineRawSpec spec);
bool Platform_DrawCircleRaw(DrawCircleRawSpec spec);
bool Platform_DrawTextRaw(DrawTextRawSpec spec);
void Platform_DrawTextCentered(DrawTextCenteredSpec spec);

int Platform_MeasureTextWidth(MeasureTextWidthSpec spec);
bool Platform_DrawTextLine(DrawTextLineSpec spec);

void Platform_GetTextLayoutMetrics(int *line_h, int *row_h, int *list_top);
int Platform_GetTextLineHeight(int fontsize);

#endif
