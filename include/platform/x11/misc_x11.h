#ifndef MISC_X11_H
#define MISC_X11_H

#include <X11/Xlib.h>
#include "platform/platform.h"

FontHandle x11_create_font_on_display(Display *dpy, int size,
                                      const char *face_name);

#endif
