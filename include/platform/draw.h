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
  bool filled = (spec).filled;                                                 \
  float angle = (spec).angle

void Platform_FlushDraw(void);

void Platform_DrawPromptEditbox(PromptState *state);
void Platform_DrawPromptEditText(PromptState *state);
void Platform_DrawPromptEditCursor(PromptState *state);
bool Platform_WindowToRoot(WindowHandle h, int *out_x, int *out_y);
bool Platform_SetWindowRounded(WindowHandle win, bool rounded, int radius);
typedef struct {
  const char *f_path;
  int f_x;
  int f_y;
  int f_w;
  int f_h;
} PlatformDrawImageAt;

bool Platform_DrawImageAt(PlatformDrawImageAt *argspec);
bool Platform_GetImageSize(const char *path, int *w, int *h);
ImageBuffer *Platform_CaptureWindowByTitle(const char *title);
ImageBuffer *Platform_CaptureWindowById(const char *id);
void Platform_FreeImage(ImageBuffer *image);
typedef struct {
  const ImageBuffer *f_image;
  int f_x;
  int f_y;
  int f_w;
  int f_h;
} PlatformDrawImageBuffer;

bool Platform_DrawImageBuffer(PlatformDrawImageBuffer *argspec);

#define COLOR_AUTO (~0UL)

typedef struct {
  int x, y, w, h;
  unsigned long color;
  int radius;
  bool rounded;
  bool opaque;
  float thickness;
  bool filled;
  float angle;
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
  int font_size;
  float angle;
} DrawTextRawSpec;

typedef struct {
  int x, y, w, h;
  const char *text;
  unsigned long color;
  FontHandle font;
  int font_size;
} DrawTextCenteredSpec;

typedef struct {
  int x, y;
  const char *text;
  int len;
  unsigned long color;
  int font_size;
} DrawTextLineSpec;

typedef struct {
  int y, width;
} DrawPromptSeparatorSpec;

typedef struct {
  const char *text;
  int len;
  int font_size;
} MeasureTextWidthSpec;

bool Platform_DrawRectRaw(DrawRectRawSpec spec);
bool Platform_DrawLineRaw(DrawLineRawSpec spec);
bool Platform_DrawCircleRaw(DrawCircleRawSpec spec);
bool Platform_DrawTextRaw(DrawTextRawSpec spec);
void Platform_DrawTextCentered(DrawTextCenteredSpec spec);

int Platform_MeasureTextWidth(MeasureTextWidthSpec spec);
bool Platform_DrawTextLine(DrawTextLineSpec spec);

void Platform_GetTextLayoutMetrics(int *line_h, int *row_h, int *list_top);
int Platform_GetTextLineHeight(int font_size);

#endif

