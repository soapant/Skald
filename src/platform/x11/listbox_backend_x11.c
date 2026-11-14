#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>

#include "platform/platform.h"
#include "platform/draw.h"
#include "helpers/x11/drawing_x11.h"
#include "core/prompt_geometry.h"
#include "core/types.h"
#include "core/types/window.h"
#include "core/window_manager.h"
#include "helpers/helpers_ui.h"
#include "config_values.h"

static __thread FontHandle b_font = FH_NULL;
static __thread Display *b_dpy = NULL;
static __thread XftDraw *b_xd = NULL;
static __thread Visual *b_vis = NULL;
static __thread Colormap b_cmap = 0;

bool Platform_ListboxBackendBegin(int item_height_override,
                                  ListboxBackendInit *out) {
  x11_get_draw_context(&b_dpy, &b_xd, &b_vis, &b_cmap);
  if (!b_dpy || !b_xd)
    return false;
  b_font = x11_create_font_on_display(b_dpy, FontSize, UsedFont);
  out->item_h =
      item_height_override > 0 ? item_height_override : prompt_row_height();
  out->list_top = prompt_list_top();
  return true;
}

void Platform_ListboxBackendDrawSelection(int bx, int bw, int y, int item_h,
                                          int fw, int frame_h, bool sel) {
  if (RoundedCorners && CornerRadius > 0) {
    if (sel) {
      int R = PROMPT_SEL_RADIUS;
      int t = PROMPT_SEL_INSET;
      x11_fill_rounded_rect(b_dpy, b_xd, b_vis, b_cmap,
                            rgb_to_hex(COLOR_SELECTED_TEXT), 0xFFFF, bx + t,
                            y + PROMPT_SEL_TOP_INSET + t, bw - 2 * t,
                            item_h - 2 - 2 * t, R - 1);
    }
  } else {
    PromptState tmp = {0};
    tmp.width = fw;
    tmp.height = frame_h;
    x11_draw_list_background(&tmp, b_dpy, b_xd, b_vis, b_cmap, y, item_h, sel);
  }
}

void Platform_ListboxBackendEnd(void) {
  if (b_font.xfont) {
    Platform_DestroyFont(b_font);
    b_font = FH_NULL;
  }
  b_dpy = NULL;
  b_xd = NULL;
  b_vis = NULL;
  b_cmap = 0;
}

PromptState *Platform_ListboxBackendPromptState(const ListBoxInfo *lb) {
  return prompt_popup_find_state(lb);
}
