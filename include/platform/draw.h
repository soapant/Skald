#ifndef PLATFORM_DRAW_H
#define PLATFORM_DRAW_H

#include "core/types.h"
#include "core/context.h"

#if defined(_WIN32)
#include <windows.h>
bool Platform_DrawTextRaw(int x, int y, const char *text, unsigned long color, FontHandle font, const char *align, int pad);
void Platform_SetDrawContextWin32(HDC hdc);
void Platform_GetDrawContextWin32(HDC *out);
#else
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include "platform/x11/display.h"
void Platform_DrawListBackground(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, int y, int h, bool selected);
void Platform_DrawListItem(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font, ListBoxInfo *lb, int idx, int y, int text_x, const char *icon_path);
void Platform_DrawEditBackground(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font);
void Platform_DrawEditText(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font);
void Platform_DrawEditCursor(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font);
void Platform_DrawSeparators(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap);
bool Platform_DrawTextRaw(int x, int y, const char *text, unsigned long color, FontHandle font, const char *align, int pad);
void Platform_SetDrawContext(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap);
void Platform_GetDrawContext(Display** dpy, XftDraw** xd, Visual** vis, Colormap* cmap);
#endif
void Platform_GetItemColors(int idx, int count, unsigned long *num_color, unsigned long *body_color, unsigned long *plain_color);
bool Platform_PreRender(void);
bool fl_prerender(void);
#if !defined(_WIN32)
void Platform_SetPreRenderState(PromptState *state);
#else
void Platform_SetPreRenderStateWin32(const char *text, void *rect);
#endif

#endif
