#ifndef WINDOW_FACTORY_X11_H
#define WINDOW_FACTORY_X11_H

#include "core/types.h"
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <sys/types.h>

Window x11_create_window_ex(Display *dpy, int x, int y, int w, int h,
                            unsigned long bg_color, const char *res_name,
                            const char *res_class, long event_mask);
Window x11_create_prompt_window(Display *dpy, int x, int y, int w, int h);
void x11_draw_window_border(Display *dpy, Drawable drw, Visual *vis,
                            int w, int h, bool no_bg);
Window x11_create_display_message_window(Display *dpy, int x, int y, int w,
                                         int h, bool clickable);
Window x11_create_display_message_window_ex(Display *dpy, int x, int y, int w,
                                            int h, bool clickable, bool rounded,
                                            int radius);
void x11_apply_click_through(Display *dpy, Window win);
void x11_apply_shape_rounded_window(Display *dpy, Window win, int w, int h);
void x11_apply_shape(Display *dpy, Window win, int w, int h, bool rounded,
                     int radius);
XftDraw *x11_create_xft_draw(Display *dpy, Window win);
#endif
