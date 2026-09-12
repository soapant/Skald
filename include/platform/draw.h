#ifndef PLATFORM_DRAW_H
#define PLATFORM_DRAW_H

#include "core/types.h"
#include "core/context.h"

#if defined(_WIN32)
#include <windows.h>
void Platform_BeginListDraw(PromptState *state, void *hdc);
void Platform_EndListDraw(PromptState *state, void *hdc);
void Platform_DrawListBackground(PromptState *state, void *hdc, int y, int h, bool selected);
void Platform_DrawListItem(PromptState *state, void *hdc, ListBoxInfo *lb, int idx, int y, int text_x, const char *icon_path);
void Platform_BeginEditDraw(PromptState *state, void *hdc);
void Platform_EndEditDraw(PromptState *state, void *hdc);
void Platform_DrawEditBackground(PromptState *state, void *hdc);
void Platform_DrawEditText(PromptState *state, void *hdc);
void Platform_DrawEditCursor(PromptState *state, void *hdc);
void Platform_DrawSeparators(PromptState *state, void *hdc);
#else
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include "platform/x11/display.h"
void Platform_BeginListDraw(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap);
void Platform_EndListDraw(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap);
void Platform_DrawListBackground(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, int y, int h, bool selected);
void Platform_DrawListItem(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font, ListBoxInfo *lb, int idx, int y, int text_x, const char *icon_path);
void Platform_BeginEditDraw(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap);
void Platform_EndEditDraw(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap);
void Platform_DrawEditBackground(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font);
void Platform_DrawEditText(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font);
void Platform_DrawEditCursor(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap, FontHandle font);
void Platform_DrawSeparators(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap);
#endif
void Platform_GetItemColors(int idx, int count, unsigned long *num_color, unsigned long *body_color, unsigned long *plain_color);

#endif
