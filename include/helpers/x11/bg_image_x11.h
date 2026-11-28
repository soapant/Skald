#ifndef BG_IMAGE_X11_H
#define BG_IMAGE_X11_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdbool.h>
#include <X11/X.h>

unsigned char *x11_decode_image_argb(const char *path, int *out_w, int *out_h);
bool x11_get_image_size(const char *path, int *out_w, int *out_h);

bool x11_bg_image_enabled(void);
Pixmap x11_bg_image_tile(Display *dpy, int w, int h);
int x11_bg_image_prewarm(Display *dpy, int w, int h);

typedef struct {
  Display *dpy;
  Drawable dst;
  Visual *vis;
  int w;
  int h;
} X11BgImageBlit;

bool x11_bg_image_blit(X11BgImageBlit *argspec);

#endif
