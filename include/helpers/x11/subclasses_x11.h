#ifndef SUBCLASSES_X11_H
#define SUBCLASSES_X11_H

#include "core/types.h"
#include "core/window_manager.h"
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

bool x11_render_prompt_user(Display *dpy, Window *win, PromptState *state);
void x11_draw_prompt_user_edit_control(Display *dpy, Window *win,
                                       PromptState *state);
void x11_draw_prompt_user_listbox(Display *dpy, Window *win,
                                  PromptState *state);
void x11_draw_prompt_user_edit_control_on_draw(Display *dpy, XftDraw *xd,
                                               Visual *vis, Colormap cmap,
                                               PromptState *state);
void x11_draw_prompt_user_listbox_on_draw(Display *dpy, XftDraw *xd,
                                          Visual *vis, Colormap cmap,
                                          PromptState *state);

#endif
