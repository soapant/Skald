#ifndef HELPERS_X11_H
#define HELPERS_X11_H

#include <pthread.h>
#include <stdint.h>
#include <X11/X.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/render.h>
#include <X11/extensions/shapeconst.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "helpers/x11/display_x11.h"
#include "deps/log.h"

void x11_img_cache_free_all(void);
void x11_border_cache_free_all(void);
bool x11_prewarm_image(Display *dpy, const char *path);

#endif
