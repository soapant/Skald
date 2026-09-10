#ifndef SUBCLASSES_X11_H
#define SUBCLASSES_X11_H

#include "core/types.h"
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

bool Linux_HandlePromptKey(const XEvent* ev, PromptState* state);
bool Linux_HandleSimplePromptKey(const XEvent* ev, SimplePromptState* state);
void DrawSimplePromptUserEditControl(Display* dpy, Window* win, SimplePromptState* state);
void DrawPromptUserEditControl(Display* dpy, Window* win, PromptState* state);
void DrawPromptUserListBox(Display* dpy, Window* win, PromptState* state);
void DrawSimplePromptUserEditControlOnDraw(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, SimplePromptState *state);
void DrawPromptUserEditControlOnDraw(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, PromptState *state);
void DrawPromptUserListBoxOnDraw(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, PromptState *state);
bool Linux_RenderPromptUser(Display* dpy, Window* win, PromptState* state);
bool Linux_RenderSimplePromptUser(Display* dpy, Window* win, SimplePromptState* state);

#endif