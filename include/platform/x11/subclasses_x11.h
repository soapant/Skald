#ifndef SUBCLASSES_X11_H
#define SUBCLASSES_X11_H

#include "core/types.h"
#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#endif

bool Linux_HandlePromptKey(const XEvent* ev, PromptState* state);
bool Linux_HandleSimplePromptKey(const XEvent* ev, SimplePromptState* state);
bool Linux_HandleEditPopupKey(const XEvent* ev, SimplePromptState* state, bool* confirmed, bool* cancelled);
void DrawSimplePromptUserEditControl(Display* dpy, Window* win, SimplePromptState* state);
void DrawPromptUserEditControl(Display* dpy, Window* win, PromptState* state);
void DrawPromptUserListBox(Display* dpy, Window* win, PromptState* state);
#if !defined(_WIN32)
void DrawSimplePromptUserEditControlOnDraw(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, SimplePromptState *state);
void DrawPromptUserEditControlOnDraw(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, PromptState *state);
void DrawPromptUserListBoxOnDraw(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, PromptState *state);
#endif
bool Linux_RenderPromptUser(Display* dpy, Window* win, PromptState* state);
bool Linux_RenderSimplePromptUser(Display* dpy, Window* win, SimplePromptState* state);

#endif
