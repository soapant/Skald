#ifndef PLATFORM_X11_DISPLAY_H
#define PLATFORM_X11_DISPLAY_H

#include <X11/Xlib.h>
#include <stdbool.h>

Display* get_display(void);

bool init_display(void);

void cleanup_display(void);

int x11_error_handler(Display *d, XErrorEvent *e);

#endif
