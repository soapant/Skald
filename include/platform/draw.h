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
} PlDrawRectRawSpec;

typedef struct {
  int x1, y1, x2, y2;
  unsigned long color;
  float thickness;
} PlDrawLineRawSpec;

typedef struct {
  int cx, cy;
  int radius;
  unsigned long color;
  float thickness;
} PlDrawCircleRawSpec;

typedef struct {
  int x, y;
  const char *text;
  unsigned long color;
  FontHandle font;
  const char *align;
  int pad;
  int fontsize;
} PlDrawTextRawSpec;

typedef struct {
  int x, y, w, h;
  const char *text;
  unsigned long color;
  FontHandle font;
  int fontsize;
} PlDrawTextCenteredSpec;

typedef struct {
  int x, y;
  const char *text;
  int len;
  unsigned long color;
  int fontsize;
} PlDrawTextLineSpec;

typedef struct {
  int idx, count;
  unsigned long *num_color, *body_color, *plain_color;
} PlGetItemColorsSpec;

typedef struct {
  int y, width;
} PlDrawPromptSeparatorSpec;

typedef struct {
  const char *text;
  int len;
  int fontsize;
} PlMeasureTextWidthSpec;

bool Platform_DrawRectRaw(PlDrawRectRawSpec spec);
bool Platform_DrawLineRaw(PlDrawLineRawSpec spec);
bool Platform_DrawCircleRaw(PlDrawCircleRawSpec spec);
bool Platform_DrawTextRaw(PlDrawTextRawSpec spec);
void Platform_DrawTextCentered(PlDrawTextCenteredSpec spec);

int Platform_MeasureTextWidth(PlMeasureTextWidthSpec spec);
bool Platform_DrawTextLine(PlDrawTextLineSpec spec);

void Platform_GetTextLayoutMetrics(int *line_h, int *row_h, int *list_top);
int Platform_GetTextLineHeight(int fontsize);

#endif
