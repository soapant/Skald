#ifndef WINDOW_FACTORY_X11_H
#define WINDOW_FACTORY_X11_H

#include "core/types.h"
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <sys/types.h>

typedef struct {
  Display *dpy;
  int x;
  int y;
  int w;
  int h;
  unsigned long bg_color;
  const char *res_name;
  const char *res_class;
  long event_mask;
} X11CreateWindowEx;

Window x11_create_window_ex(X11CreateWindowEx *argspec);

typedef struct {
  Display *dpy;
  int x;
  int y;
  int w;
  int h;
} X11CreatePromptWindow;

Window x11_create_prompt_window(X11CreatePromptWindow *argspec);

typedef struct {
  Display *dpy;
  Drawable drw;
  Visual *vis;
  int w;
  int h;
  bool no_bg;
} X11DrawWindowBorder;

void x11_draw_window_border(X11DrawWindowBorder *argspec);

typedef struct {
  Display *dpy;
  int x;
  int y;
  int w;
  int h;
  bool clickable;
} X11CreateDisplayMessageWindow;

Window
x11_create_display_message_window(X11CreateDisplayMessageWindow *argspec);

typedef struct {
  Display *dpy;
  int x;
  int y;
  int w;
  int h;
  bool clickable;
  bool rounded;
  int radius;
} X11CreateDisplayMessageWindowEx;

Window
x11_create_display_message_window_ex(X11CreateDisplayMessageWindowEx *argspec);
void x11_apply_click_through(Display *dpy, Window win);
void x11_set_fixed_size_hints(Display *dpy, Window win, int w, int h);
void x11_apply_shape_rounded_window(Display *dpy, Window win, int w, int h);

typedef struct {
  Display *dpy;
  Window win;
  int w;
  int h;
  bool rounded;
  int radius;
} X11ApplyShape;

void x11_apply_shape(X11ApplyShape *argspec);
XftDraw *x11_create_xft_draw(Display *dpy, Window win);
#endif
