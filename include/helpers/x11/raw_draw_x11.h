#ifndef RAW_DRAW_X11_H
#define RAW_DRAW_X11_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <stdbool.h>
#include <stdint.h>
#include "core/types.h"
#include "helpers/helpers_text.h"
#include <X11/extensions/Xrender.h>
#include <string.h>
#include <X11/X.h>
#include <fontconfig/fontconfig.h>
#include "platform/draw.h"
#include "platform/platform.h"
#include "helpers/helpers_ui.h"
#include "core/prompt_geometry.h"
#include "core/draw_state.h"
#include "core/listbox_hover.h"
#include "core/editbox.h"
#include "helpers/x11/drawing_x11.h"
#include "helpers/x11/display_x11.h"
#include "config_values.h"
#include "core/context.h"
#include "core/types/window.h"

typedef struct {
  const PromptState *f_state;
  Display *f_dpy;
  XftDraw *f_xd;
  Visual *f_vis;
  Colormap f_cmap;
  FontHandle f_font;
} X11DrawEditBackground;

void x11_draw_edit_background(X11DrawEditBackground *argspec);
typedef struct {
  PromptState *f_state;
  Display *f_dpy;
  XftDraw *f_xd;
  Visual *f_vis;
  Colormap f_cmap;
  FontHandle f_font;
} X11DrawEditTint;

void x11_draw_edit_tint(X11DrawEditTint *argspec);
typedef struct {
  PromptState *f_state;
  Display *f_dpy;
  XftDraw *f_xd;
  Visual *f_vis;
  Colormap f_cmap;
  FontHandle f_font;
} X11DrawEditText;

void x11_draw_edit_text(X11DrawEditText *argspec);
typedef struct {
  PromptState *f_state;
  Display *f_dpy;
  XftDraw *f_xd;
  Visual *f_vis;
  Colormap f_cmap;
  FontHandle f_font;
} X11DrawEditCursor;

void x11_draw_edit_cursor(X11DrawEditCursor *argspec);
typedef struct {
  Display *f_dpy;
  XftDraw *f_xd;
  Visual *f_vis;
  Colormap f_cmap;
  FontHandle f_font;
  ListBox *f_lb;
  int f_idx;
  int f_y;
  int f_text_x;
  const char *f_icon_path;
} X11DrawListItem;

void x11_draw_list_item(X11DrawListItem *argspec);

extern __thread Display *g_draw_dpy;
extern __thread XftDraw *g_draw_xd;
extern __thread Visual *g_draw_vis;
extern __thread Colormap g_draw_cmap;

void x11_raw_line_flush(void);
typedef struct {
  int f_x;
  int f_y;
  char f_ch;
  unsigned long f_color;
  FontHandle f_fh;
  bool f_owned;
  int f_font_size;
} X11RawLinePush;

bool x11_raw_line_push(X11RawLinePush *argspec);
XftFont *x11_line_font(Display *dpy, int size);
void x11_line_font_cache_free_all(void);
void x11_prompt_editbox_spec(PromptState *state, FontHandle font,
                             EditboxSpec *eb, EditboxX11Metrics *metrics);
bool x11_raw_can_draw(void);
bool x11_raw_draw_rect_rotated(const DrawRectRawSpec *spec);
bool x11_raw_draw_text_rotated(const DrawTextRawSpec *spec);
GC x11_raw_gc_for(Display *dpy, Drawable d);
GC x11_raw_prepare(unsigned long color, int thickness);

#endif

