#ifndef SUBCLASSES_X11_H
#define SUBCLASSES_X11_H

#include "core/types.h"

bool Linux_HandlePromptKey(const XEvent* ev, PromptState* state);
bool Linux_HandleSimplePromptKey(const XEvent* ev, SimplePromptState* state);
bool Linux_HandleEditPopupKey(const XEvent* ev, SimplePromptState* state, bool* confirmed, bool* cancelled);
void DrawSimplePromptUserEditControl(Display* dpy, Window* win, SimplePromptState* state);
void DrawPromptUserEditControl(Display* dpy, Window* win, PromptState* state);
void DrawPromptUserListBox(Display* dpy, Window* win, PromptState* state);
bool Linux_RenderPromptUser(Display* dpy, Window* win, PromptState* state);
bool Linux_RenderSimplePromptUser(Display* dpy, Window* win, SimplePromptState* state);

#endif
