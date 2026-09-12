#ifndef EDIT_RENDERER_H
#define EDIT_RENDERER_H
#include "core/types.h"
#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
void Core_DrawEditBox(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap);
#else
void Core_DrawEditBox(PromptState *state, void *hdc);
#endif
#endif
