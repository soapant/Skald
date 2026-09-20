#ifndef SUBCLASSES_X11_H
#define SUBCLASSES_X11_H

#include "core/types.h"
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

bool Linux_HandlePromptKey(const XEvent* ev, PromptState* state);
void DrawPromptUserEditControl(Display* dpy, Window* win, PromptState* state);
void DrawPromptUserListBox(Display* dpy, Window* win, PromptState* state);
void DrawPromptUserEditControlOnDraw(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, PromptState *state);
void DrawPromptUserListBoxOnDraw(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, PromptState *state);
bool Linux_RenderPromptUser(Display* dpy, Window* win, PromptState* state);

#endif
