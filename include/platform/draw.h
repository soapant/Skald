#ifndef PLATFORM_DRAW_H
#define PLATFORM_DRAW_H

#include "core/types.h"

void Platform_SetDrawBgColor(unsigned long hex);
unsigned long Platform_GetDrawBgColor(void);
void Platform_SetTransparentWindow(bool on);
bool Platform_GetTransparentWindow(void);
void Platform_FlushDraw(void);
void Platform_SetPreRenderState(PromptState *state);
void Platform_GetLastMeasureOrigin(int *out_x, int *out_y);
void Platform_SetDrawOffset(int dx, int dy);
void Platform_BeginMeasure(void);
void Platform_EndMeasure(int *out_w, int *out_h);

void Platform_DrawPromptEditText(PromptState *state);
void Platform_DrawPromptEditCursor(PromptState *state);
void Platform_GetDrawOffset(int *out_x, int *out_y);
bool Platform_WindowToRoot(WindowHandle h, int *out_x, int *out_y);
bool Platform_DrawImageAt(const char *path, int x, int y, int w, int h);

typedef struct {
    int x, y, w, h;
    unsigned long color;
    int radius;
    bool rounded;
    bool opaque;
    float thickness;
    bool filled;
} PlDrawRectRawSpec;

bool Platform_DrawRectRaw_impl(PlDrawRectRawSpec spec);
#define Platform_DrawRectRaw(...) Platform_DrawRectRaw_impl((PlDrawRectRawSpec){__VA_ARGS__})
void Platform_DrawPromptSeparator(int y, int width);
bool Platform_DrawLineRaw(int x1, int y1, int x2, int y2, unsigned long color, float thickness);
bool Platform_DrawCircleRaw(int cx, int cy, int radius, unsigned long color, float thickness);
bool Platform_DrawTextRaw(int x, int y, const char *text, unsigned long color, FontHandle font, const char *align, int pad, int fontsize);
void Platform_DrawTextCentered(int x, int y, int w, int h, const char *text, unsigned long color, FontHandle font, int fontsize);
int Platform_MeasureTextWidth(const char *text, int len, int fontsize);
bool Platform_DrawTextLine(int x, int y, const char *text, int len, unsigned long color, int fontsize);
void Platform_GetItemColors(int idx, int count, unsigned long *num_color, unsigned long *body_color, unsigned long *plain_color);
bool Platform_PreRender(void);

void Platform_GetTextLayoutMetrics(int* line_h, int* row_h, int* list_top);
int Platform_GetGlyphInkHeight(const char *text, int fontsize);

#endif
