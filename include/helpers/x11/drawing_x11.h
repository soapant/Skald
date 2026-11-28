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
  Display *f_dpy;
  XftDraw *f_xd;
  XftColor *f_color;
  FontHandle f_font;
  const char *f_text;
  int f_start_y;
  char *f_centered_to;
  int f_x_offset;
  int f_box_width;
} X11DrawWrappedText;

void x11_draw_wrapped_text(X11DrawWrappedText *argspec);
typedef struct {
  Display *f_dpy;
  XftDraw *f_xd;
  XftColor *f_color;
  int f_x;
  int f_y;
  XftFont *f_xf;
  const char *f_s;
  int f_n;
  int f_px;
} X11DrawUtf8Fallback;

int x11_draw_utf8_fallback(X11DrawUtf8Fallback *argspec);
typedef struct {
  Display *f_dpy;
  XftFont *f_xf;
  const char *f_s;
  int f_n;
  int f_px;
} X11MeasureUtf8Fallback;

int x11_measure_utf8_fallback(X11MeasureUtf8Fallback *argspec);
void x11_bump_measure_epoch(void);
typedef struct {
  Display *f_dpy;
  Visual *f_vis;
  Colormap f_cmap;
  unsigned long f_hex;
  unsigned short f_alpha;
  XftColor *f_out;
} X11XftColorCached;

bool x11_xft_color_cached(X11XftColorCached *argspec);
typedef struct {
  Display *f_dpy;
  XImage *f_img;
  unsigned char *f_data;
  int f_w;
  int f_h;
  unsigned long f_red_mask;
  unsigned long f_green_mask;
  unsigned long f_blue_mask;
} X11InitArgbImage;

void x11_init_argb_image(X11InitArgbImage *argspec);
typedef struct {
  Display *f_dpy;
  Drawable f_ref;
  unsigned char *f_argb;
  int f_w;
  int f_h;
  Pixmap *f_out_pm;
  Picture *f_out_pic;
} X11UploadArgb;

bool x11_upload_argb(X11UploadArgb *argspec);

const char *x11_listbox_item_display_text(ListBox *lb, int idx, char *trunc,
                                          size_t trunc_size);
typedef struct {
  Display *f_dpy;
  XftDraw *f_xd;
  Visual *f_vis;
  Colormap f_cmap;
  FontHandle f_font;
  XftColor *f_plain_color;
  ListBox *f_lb;
  const char *f_icon_path;
  int f_idx;
  int f_y;
} X11DrawListboxItemText;

void x11_draw_listbox_item_text(X11DrawListboxItemText *argspec);
typedef struct {
  Display *f_dpy;
  XftDraw *f_xd;
  Visual *f_vis;
  Colormap f_cmap;
  unsigned long f_hex;
  unsigned short f_alpha16;
  int f_x;
  int f_y;
  int f_w;
  int f_h;
  int f_radius;
} X11FillRoundedRect;

void x11_fill_rounded_rect(X11FillRoundedRect *argspec);
void x11_draw_background(Display *dpy, Window win, int w, int h);
void x11_draw_background_to_drawable(Display *dpy, Drawable drw, int w, int h);
Pixmap x11_get_cached_background(Display *dpy, Window win, int w, int h);
void x11_set_dbl_visual(Display *dpy, Visual *vis, Colormap cmap, int depth);
void x11_clear_dbl_visual(void);
typedef struct {
  Display *f_dpy;
  Drawable f_d;
  Visual **f_vis_out;
  Colormap *f_cmap_out;
  int *f_depth_out;
} X11GetDrawableVisual;

bool x11_get_drawable_visual(X11GetDrawableVisual *argspec);
typedef struct {
  Display *f_dpy;
  XftDraw *f_xd;
  Visual *f_vis;
  Colormap f_cmap;
  int f_x;
  int f_y;
  int f_w;
  int f_h;
  unsigned long f_baseHex;
} X11DrawSelectionBackground;

void x11_draw_selection_background(X11DrawSelectionBackground *argspec);
void x11_release_prompt_buffer(Display *dpy, Window win);

void x11_get_draw_context(Display **dpy, XftDraw **xd, Visual **vis,
                          Colormap *cmap);
void x11_set_draw_context(Display *dpy, XftDraw *xd, Visual *vis,
                          Colormap cmap);
typedef struct {
  PromptState *f_state;
  Display *f_dpy;
  XftDraw *f_xd;
  Visual *f_vis;
  Colormap f_cmap;
} X11DrawListbox;

void x11_draw_listbox(X11DrawListbox *argspec);
typedef struct {
  PromptState *f_state;
  Display *f_dpy;
  XftDraw *f_xd;
  Visual *f_vis;
  Colormap f_cmap;
  int f_y;
  int f_h;
  bool f_selected;
} X11DrawListBackground;

void x11_draw_list_background(X11DrawListBackground *argspec);
typedef struct {
  Display *f_dpy;
  XftDraw *f_xd;
  Visual *f_vis;
  const char *f_path;
  int f_x;
  int f_y;
  int f_tw;
  int f_th;
} X11DrawImageScaled;

bool x11_draw_image_scaled(X11DrawImageScaled *argspec);

typedef struct {
  PromptState *f_state;
  Display *f_dpy;
  XftDraw *f_xd;
  Visual *f_vis;
  Colormap f_cmap;
} X11DrawEditbox;

void x11_draw_editbox(X11DrawEditbox *argspec);

FontHandle x11_create_font_on_display(Display *dpy, int size,
                                      const char *face_name);

int x11_editbox_measure(const char *text, int len, void *user);
#endif

