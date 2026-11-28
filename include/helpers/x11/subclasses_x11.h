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
  Display *f_dpy;
  XftDraw *f_xd;
  Visual *f_vis;
  Colormap f_cmap;
  PromptState *f_state;
} FnX11DrawPromptUserEditControlOnDrawSpec;

void x11_draw_prompt_user_edit_control_on_draw(FnX11DrawPromptUserEditControlOnDrawSpec *argspec);
typedef struct {
  Display *f_dpy;
  XftDraw *f_xd;
  Visual *f_vis;
  Colormap f_cmap;
  PromptState *f_state;
} FnX11DrawPromptUserListboxOnDrawSpec;

void x11_draw_prompt_user_listbox_on_draw(FnX11DrawPromptUserListboxOnDrawSpec *argspec);

#endif
