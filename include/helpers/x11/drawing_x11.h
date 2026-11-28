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

typedef struct {
  Display *dpy;
  XftDraw *xd;
  XftColor *color;
  FontHandle font;
  const char *text;
  int start_y;
  char *centered_to;
  int x_offset;
  int box_width;
} X11DrawWrappedText;

void x11_draw_wrapped_text(X11DrawWrappedText *argspec);
typedef struct {
  Display *dpy;
  XftDraw *xd;
  XftColor *color;
  int x;
  int y;
  XftFont *xf;
  const char *s;
  int n;
  int px;
} X11DrawUtf8Fallback;

int x11_draw_utf8_fallback(X11DrawUtf8Fallback *argspec);
typedef struct {
  Display *dpy;
  XftFont *xf;
  const char *s;
  int n;
  int px;
} X11MeasureUtf8Fallback;

int x11_measure_utf8_fallback(X11MeasureUtf8Fallback *argspec);
void x11_bump_measure_epoch(void);
typedef struct {
  Display *dpy;
  Visual *vis;
  Colormap cmap;
  unsigned long hex;
  unsigned short alpha;
  XftColor *out;
} X11XftColorCached;

bool x11_xft_color_cached(X11XftColorCached *argspec);
typedef struct {
  Display *dpy;
  XImage *img;
  unsigned char *data;
  int w;
  int h;
  unsigned long red_mask;
  unsigned long green_mask;
  unsigned long blue_mask;
} X11InitArgbImage;

void x11_init_argb_image(X11InitArgbImage *argspec);
typedef struct {
  Display *dpy;
  Drawable ref;
  unsigned char *argb;
  int w;
  int h;
  Pixmap *out_pm;
  Picture *out_pic;
} X11UploadArgb;

bool x11_upload_argb(X11UploadArgb *argspec);

const char *x11_listbox_item_display_text(ListBox *lb, int idx, char *trunc,
                                          size_t trunc_size);
typedef struct {
  Display *dpy;
  XftDraw *xd;
  Visual *vis;
  Colormap cmap;
  FontHandle font;
  XftColor *plain_color;
  ListBox *lb;
  const char *icon_path;
  int idx;
  int y;
} X11DrawListboxItemText;

void x11_draw_listbox_item_text(X11DrawListboxItemText *argspec);
typedef struct {
  Display *dpy;
  XftDraw *xd;
  Visual *vis;
  Colormap cmap;
  unsigned long hex;
  unsigned short alpha16;
  int x;
  int y;
  int w;
  int h;
  int radius;
} X11FillRoundedRect;

void x11_fill_rounded_rect(X11FillRoundedRect *argspec);
void x11_draw_background(Display *dpy, Window win, int w, int h);
void x11_draw_background_to_drawable(Display *dpy, Drawable drw, int w, int h);
Pixmap x11_get_cached_background(Display *dpy, Window win, int w, int h);
void x11_set_dbl_visual(Display *dpy, Visual *vis, Colormap cmap, int depth);
void x11_clear_dbl_visual(void);
typedef struct {
  Display *dpy;
  Drawable d;
  Visual **vis_out;
  Colormap *cmap_out;
  int *depth_out;
} X11GetDrawableVisual;

bool x11_get_drawable_visual(X11GetDrawableVisual *argspec);
typedef struct {
  Display *dpy;
  XftDraw *xd;
  Visual *vis;
  Colormap cmap;
  int x;
  int y;
  int w;
  int h;
  unsigned long baseHex;
} X11DrawSelectionBackground;

void x11_draw_selection_background(X11DrawSelectionBackground *argspec);
void x11_release_prompt_buffer(Display *dpy, Window win);

void x11_get_draw_context(Display **dpy, XftDraw **xd, Visual **vis,
                          Colormap *cmap);
void x11_set_draw_context(Display *dpy, XftDraw *xd, Visual *vis,
                          Colormap cmap);
typedef struct {
  PromptState *state;
  Display *dpy;
  XftDraw *xd;
  Visual *vis;
  Colormap cmap;
} X11DrawListbox;

void x11_draw_listbox(X11DrawListbox *argspec);
typedef struct {
  PromptState *state;
  Display *dpy;
  XftDraw *xd;
  Visual *vis;
  Colormap cmap;
  int y;
  int h;
  bool selected;
} X11DrawListBackground;

void x11_draw_list_background(X11DrawListBackground *argspec);
typedef struct {
  Display *dpy;
  XftDraw *xd;
  Visual *vis;
  const char *path;
  int x;
  int y;
  int tw;
  int th;
} X11DrawImageScaled;

bool x11_draw_image_scaled(X11DrawImageScaled *argspec);

typedef struct {
  PromptState *state;
  Display *dpy;
  XftDraw *xd;
  Visual *vis;
  Colormap cmap;
} X11DrawEditbox;

void x11_draw_editbox(X11DrawEditbox *argspec);

FontHandle x11_create_font_on_display(Display *dpy, int size,
                                      const char *face_name);

int x11_editbox_measure(const char *text, int len, void *user);
#endif

