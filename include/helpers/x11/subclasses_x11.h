#ifndef SUBCLASSES_X11_H
#define SUBCLASSES_X11_H

#include "core/types.h"
#include "core/window_manager.h"
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

bool x11_render_prompt_user(Display *dpy, Window *win, PromptState *state,
                            bool suppress_content);
void x11_draw_prompt_user_edit_control(Display *dpy, Window *win,
                                       PromptState *state);
void x11_draw_prompt_user_listbox(Display *dpy, Window *win,
                                  PromptState *state);
typedef struct {
  Display *dpy;
  XftDraw *xd;
  Visual *vis;
  Colormap cmap;
  PromptState *state;
} X11DrawPromptUserEditControlOnDraw;

void x11_draw_prompt_user_edit_control_on_draw(X11DrawPromptUserEditControlOnDraw *argspec);
typedef struct {
  Display *dpy;
  XftDraw *xd;
  Visual *vis;
  Colormap cmap;
  PromptState *state;
} X11DrawPromptUserListboxOnDraw;

void x11_draw_prompt_user_listbox_on_draw(X11DrawPromptUserListboxOnDraw *argspec);

#endif

