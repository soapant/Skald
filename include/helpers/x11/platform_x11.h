#ifndef HELPERS_X11_PLATFORM_H
#define HELPERS_X11_PLATFORM_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <stdbool.h>
#include <pthread.h>
#include "core/types.h"

extern pthread_mutex_t g_font_cache_mtx;

bool x11_font_is_cached(Display *dpy, XftFont *xft);
bool x11_font_was_retired(Display *dpy, XftFont *xft);
FontHandle x11_create_font_on_display(Display *dpy, int size,
                                      const char *face_name);
void x11_cleanup_per_feature_edit_map(void);
int x11_x11_button(int button);
unsigned char x11_mask_extract(unsigned long pixel, unsigned long mask);

#endif
