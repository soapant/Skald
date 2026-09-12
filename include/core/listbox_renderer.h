#ifndef LISTBOX_RENDERER_H
#define LISTBOX_RENDERER_H
#include "core/types.h"
#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
void Core_DrawListBox(PromptState *state, Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap);
#else
void Core_DrawListBox(PromptState *state, void *hdc);
#endif
#endif
