#ifndef SUBCLASSES_X11_H
#define SUBCLASSES_X11_H

#include "core/types.h"
#include "core/window_manager.h"
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

bool x11_render_prompt_user(Display* dpy, Window* win, PromptState* state);
void DrawPromptUserEditControl(Display* dpy, Window* win, PromptState* state);
void DrawPromptUserListBox(Display* dpy, Window* win, PromptState* state);
void DrawPromptUserEditControlOnDraw(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, PromptState *state);
void DrawPromptUserListBoxOnDraw(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, PromptState *state);
bool x11_render_prompt_user(Display* dpy, Window* win, PromptState* state);

#endif
