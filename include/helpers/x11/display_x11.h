#ifndef DISPLAY_X11_H
#define DISPLAY_X11_H

#include <X11/Xlib.h>
#include <stdbool.h>

#include "core/types.h"
#include "core/types/window.h"

Display *x11_get_display(void);
bool x11_init_display(void);
void x11_cleanup_display(void);
int x11_error_handler(Display *d, XErrorEvent *e);

#endif
