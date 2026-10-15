#ifndef DISPLAY_X11_H
#define DISPLAY_X11_H

#include <X11/Xlib.h>
#include <stdbool.h>
#include "core/types.h"

Display* get_display(void);
bool init_display(void);
void cleanup_display(void);
int x11_error_handler(Display *d, XErrorEvent *e);

#endif
