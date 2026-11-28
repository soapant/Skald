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

void Platform_ListboxBackendDrawSelection(ListboxBackendDrawSelection *argspec) {
  if (RoundedCorners && CornerRadius > 0) {
    if (argspec->f_sel) {
      int R = PROMPT_SEL_RADIUS;
      int t = PROMPT_SEL_INSET;
      x11_fill_rounded_rect(&(X11FillRoundedRect){.f_dpy = b_dpy, .f_xd = b_xd, .f_vis = b_vis, .f_cmap = b_cmap, .f_hex = rgb_to_hex(COLOR_SELECTED_TEXT), .f_alpha16 = 0xFFFF, .f_x = argspec->f_bx + t, .f_y = argspec->f_y + PROMPT_SEL_TOP_INSET + t, .f_w = argspec->f_bw - 2 * t, .f_h = argspec->f_item_h - 2 - 2 * t, .f_radius = R - 1});
    }
  } else {
    PromptState tmp = {0};
    tmp.width = argspec->f_fw;
    tmp.height = argspec->f_frame_h;
    x11_draw_list_background(&(X11DrawListBackground){.f_state = &tmp, .f_dpy = b_dpy, .f_xd = b_xd, .f_vis = b_vis, .f_cmap = b_cmap, .f_y = argspec->f_y, .f_h = argspec->f_item_h, .f_selected = argspec->f_sel});
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

PromptState *Platform_ListboxBackendPromptState(const ListBox *lb) {
  return prompt_popup_find_state(lb);
}

