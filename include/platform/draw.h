#ifndef PLATFORM_DRAW_H
#define PLATFORM_DRAW_H

#include "core/types.h"
#include "core/context.h"

#if defined(_WIN32)
#include <windows.h>
void Platform_SetDrawContextWin32(HDC hdc);
void Platform_SetDrawBgColor(unsigned long hex);
unsigned long Platform_GetDrawBgColor(void);
void Platform_SetTransparentWindow(bool on);
bool Platform_GetTransparentWindow(void);
void Platform_GetDrawContextWin32(HDC *out);
void Platform_SetPreRenderStateWin32(const char *text, void *rect);
void Platform_BeginMeasure(void);
void Platform_EndMeasure(int *out_w, int *out_h);
void Platform_GetLastMeasureOrigin(int *out_x, int *out_y);
void Platform_SetDrawOffset(int dx, int dy);
#else
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include "platform/x11/display_x11.h"
void Platform_DrawListBackground(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, int y, int h, bool selected);
void Platform_DrawListItem(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font, ListBoxInfo *lb, int idx, int y, int text_x, const char *icon_path);
void Platform_DrawEditBackground(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font);
void Platform_DrawEditText(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font);
void Platform_DrawEditTint(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font);
void Platform_DrawEditCursor(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font);
void Platform_SetDrawContext(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap);
void Platform_GetDrawContext(Display** dpy, XftDraw** xd, Visual** vis, Colormap* cmap);
void Platform_FlushDraw(void);
void Platform_SetDrawBgColor(unsigned long hex);
unsigned long Platform_GetDrawBgColor(void);
void Platform_SetTransparentWindow(bool on);
bool Platform_GetTransparentWindow(void);
void Platform_SetPreRenderState(PromptState *state);
PromptState* Platform_GetPreRenderStatePtr(void);
void Platform_GetLastMeasureOrigin(int *out_x, int *out_y);
void Platform_SetDrawOffset(int dx, int dy);
void Platform_BeginMeasure(void);
void Platform_EndMeasure(int *out_w, int *out_h);
void Platform_DrawEditBox(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap);
void Platform_DrawListBox(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap);
bool Platform_DrawImageScaled(Display *dpy, XftDraw *xd, Visual *vis, const char *path, int x, int y, int w, int h);
#endif
void Platform_DrawPromptEditText(PromptState *state);
void Platform_DrawPromptEditCursor(PromptState *state);
bool Platform_DrawImageAt(const char *path, int x, int y, int w, int h);
bool Platform_DrawRectRaw(int x, int y, int w, int h, unsigned long color, unsigned char alpha);
bool Platform_DrawRectRawOpaque(int x, int y, int w, int h, unsigned long color);
bool Platform_DrawRectBorder(int x, int y, int w, int h, unsigned long color, float thickness);
bool Platform_DrawLineRaw(int x1, int y1, int x2, int y2, unsigned long color, float thickness);
bool Platform_DrawCircleRaw(int cx, int cy, int radius, unsigned long color, float thickness);
bool Platform_DrawRoundedRectRaw(int x, int y, int w, int h, int radius, unsigned long color, float thickness);
bool Platform_DrawTextRaw(int x, int y, const char *text, unsigned long color, FontHandle font, const char *align, int pad,int fontsize);
int Platform_MeasureTextWidth(const char *text, int len, int fontsize);
bool Platform_DrawTextLine(int x, int y, const char *text, int len, unsigned long color, int fontsize);
void Platform_GetItemColors(int idx, int count, unsigned long *num_color, unsigned long *body_color, unsigned long *plain_color);
bool Platform_PreRender(void);
bool fl_prerender(void);

/* Vertical layout metrics for the edit box and list, as the draw code actually
   computes them. line_h is the font's real line box (ascent+descent), which is
   larger than the em size; row_h and list_top reserve space using it. */
void Platform_GetTextLayoutMetrics(int* line_h, int* row_h, int* list_top);

#endif
