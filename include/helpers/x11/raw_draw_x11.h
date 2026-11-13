#ifndef HELPERS_X11_DRAW_H
#define HELPERS_X11_DRAW_H

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

extern __thread Display *g_draw_dpy;
extern __thread XftDraw *g_draw_xd;
extern __thread Visual *g_draw_vis;
extern __thread Colormap g_draw_cmap;

void x11_raw_line_flush(void);
bool x11_raw_line_push(int x, int y, char ch, unsigned long color,
                       FontHandle fh, bool owned, int fontsize);
XftFont *x11_line_font(Display *dpy, int size);
void x11_prompt_editbox_spec(PromptState *state, FontHandle font,
                             EditboxSpec *eb, EditboxX11Metrics *metrics);
bool x11_raw_can_draw(void);
GC x11_raw_gc_for(Display *dpy, Drawable d);
GC x11_raw_prepare(unsigned long color, int thickness);

#endif
