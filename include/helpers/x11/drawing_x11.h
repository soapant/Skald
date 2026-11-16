#ifndef DRAWING_X11_H
#define DRAWING_X11_H

#include "core/types.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xft/Xft.h>

typedef struct {
  Display *dpy;
  XftFont *xf;
} EditboxX11Metrics;

void x11_draw_wrapped_text(Display *dpy, XftDraw *xd, XftColor *color,
                           FontHandle font, const char *text, int start_y,
                           char *centered_to, int x_offset, int box_width);
int x11_draw_utf8_fallback(Display *dpy, XftDraw *xd, XftColor *color, int x,
                           int y, XftFont *xf, const char *s, int n, int px);
int x11_measure_utf8_fallback(Display *dpy, XftFont *xf, const char *s, int n,
                              int px);
bool x11_xft_color_cached(Display *dpy, Visual *vis, Colormap cmap,
                          unsigned long hex, unsigned short alpha,
                          XftColor *out);
void x11_init_argb_image(Display *dpy, XImage *img, unsigned char *data, int w,
                         int h, unsigned long red_mask,
                         unsigned long green_mask, unsigned long blue_mask);
bool x11_upload_argb(Display *dpy, Drawable ref, unsigned char *argb, int w,
                     int h, Pixmap *out_pm, Picture *out_pic);

const char *x11_listbox_item_display_text(ListBoxInfo *lb, int idx, char *trunc,
                                          size_t trunc_size);
void x11_draw_listbox_item_text(Display *dpy, XftDraw *xd, Visual *vis,
                                Colormap cmap, FontHandle font,
                                XftColor *plain_color, ListBoxInfo *lb,
                                const char *icon_path, int idx, int y);
void x11_fill_rounded_rect(Display *dpy, XftDraw *xd, Visual *vis,
                           Colormap cmap, unsigned long hex,
                           unsigned short alpha16, int x, int y, int w, int h,
                           int radius);
void x11_draw_background(Display *dpy, Window win, int w, int h);
void x11_draw_background_to_drawable(Display *dpy, Drawable drw, int w, int h);
Pixmap x11_get_cached_background(Display *dpy, Window win, int w, int h);
void x11_set_dbl_visual(Display *dpy, Visual *vis, Colormap cmap, int depth);
void x11_clear_dbl_visual(void);
bool x11_get_drawable_visual(Display *dpy, Drawable d, Visual **vis_out,
                             Colormap *cmap_out, int *depth_out);
void x11_draw_selection_background(Display *dpy, XftDraw *xd, Visual *vis,
                                   Colormap cmap, int x, int y, int w, int h,
                                   unsigned long baseHex);
void x11_release_prompt_buffer(Display *dpy, Window win);

void x11_get_draw_context(Display **dpy, XftDraw **xd, Visual **vis,
                          Colormap *cmap);
void x11_set_draw_context(Display *dpy, XftDraw *xd, Visual *vis,
                          Colormap cmap);
void x11_draw_listbox(PromptState *state, Display *dpy, XftDraw *xd,
                      Visual *vis, Colormap cmap);
void x11_draw_list_background(PromptState *state, Display *dpy, XftDraw *xd,
                              Visual *vis, Colormap cmap, int y, int h,
                              bool selected);
bool x11_draw_image_scaled(Display *dpy, XftDraw *xd, Visual *vis,
                           const char *path, int x, int y, int tw, int th);

void x11_draw_editbox(PromptState *state, Display *dpy, XftDraw *xd,
                      Visual *vis, Colormap cmap);

FontHandle x11_create_font_on_display(Display *dpy, int size,
                                      const char *face_name);

int x11_editbox_measure(const char *text, int len, void *user);
#endif
