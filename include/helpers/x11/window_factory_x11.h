#ifndef WINDOW_FACTORY_X11_H
#define WINDOW_FACTORY_X11_H

#include "core/types.h"
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <sys/types.h>

typedef struct {
  Display *f_dpy;
  int f_x;
  int f_y;
  int f_w;
  int f_h;
  unsigned long f_bg_color;
  const char *f_res_name;
  const char *f_res_class;
  long f_event_mask;
} X11CreateWindowEx;

Window x11_create_window_ex(X11CreateWindowEx *argspec);
typedef struct {
  Display *f_dpy;
  int f_x;
  int f_y;
  int f_w;
  int f_h;
} X11CreatePromptWindow;

Window x11_create_prompt_window(X11CreatePromptWindow *argspec);
typedef struct {
  Display *f_dpy;
  Drawable f_drw;
  Visual *f_vis;
  int f_w;
  int f_h;
  bool f_no_bg;
} X11DrawWindowBorder;

void x11_draw_window_border(X11DrawWindowBorder *argspec);
typedef struct {
  Display *f_dpy;
  int f_x;
  int f_y;
  int f_w;
  int f_h;
  bool f_clickable;
} X11CreateDisplayMessageWindow;

Window x11_create_display_message_window(X11CreateDisplayMessageWindow *argspec);
typedef struct {
  Display *f_dpy;
  int f_x;
  int f_y;
  int f_w;
  int f_h;
  bool f_clickable;
  bool f_rounded;
  int f_radius;
} X11CreateDisplayMessageWindowEx;

Window x11_create_display_message_window_ex(X11CreateDisplayMessageWindowEx *argspec);
void x11_apply_click_through(Display *dpy, Window win);
void x11_set_fixed_size_hints(Display *dpy, Window win, int w, int h);
void x11_apply_shape_rounded_window(Display *dpy, Window win, int w, int h);
typedef struct {
  Display *f_dpy;
  Window f_win;
  int f_w;
  int f_h;
  bool f_rounded;
  int f_radius;
} X11ApplyShape;

void x11_apply_shape(X11ApplyShape *argspec);
XftDraw *x11_create_xft_draw(Display *dpy, Window win);
#endif

