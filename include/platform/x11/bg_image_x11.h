#ifndef BG_IMAGE_X11_H
#define BG_IMAGE_X11_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdbool.h>

/* Decodes path into a malloc'd ARGB buffer at exactly *out_w x *out_h.
   Caller frees. Returns NULL on unsupported format / decode failure. */
unsigned char *x11_decode_image_argb(const char *path, int *out_w, int *out_h);

bool   x11_bg_image_enabled(void);
Pixmap x11_bg_image_tile(Display *dpy, int w, int h);
void   x11_bg_image_purge(void);
int    x11_bg_image_prewarm(Display *dpy, int w, int h);

/* Composites the cached tile onto dst at (0,0). Returns false when no
   background image is configured or nothing could be composed. */
bool x11_bg_image_blit(Display *dpy, Drawable dst, Visual *vis, int depth, int w, int h);

#endif