#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>

#include "helpers/helpers_listbox_backend.h"
#include "platform/platform.h"
#include "platform/draw.h"
#include "platform/x11/drawing_x11.h"
#include "core/prompt_geometry.h"
#include "core/types.h"
#include "core/types/window.h"
#include "core/window_manager.h"
#include "helpers/helpers_ui.h"
#include "config_values.h"

static __thread FontHandle g_b_font = FH_NULL;
static __thread Display *g_b_dpy = NULL;
static __thread XftDraw *g_b_xd = NULL;
static __thread Visual *g_b_vis = NULL;
static __thread Colormap g_b_cmap = 0;

bool ListboxBackend_Begin(int item_height_override, ListboxBackendInit *out) {
  x11_get_draw_context(&g_b_dpy, &g_b_xd, &g_b_vis, &g_b_cmap);
  if (!g_b_dpy || !g_b_xd)
    return false;
  g_b_font = x11_create_font_on_display(g_b_dpy, FontSize, UsedFont);
  out->item_h = item_height_override > 0 ? item_height_override
                                         : x11_font_row_height_of(g_b_font);
  out->list_top = x11_font_list_top_of(g_b_font);
  return true;
}

void ListboxBackend_DrawSelection(int bx, int bw, int y, int item_h, int fw,
                                  int frame_h, bool sel) {
  if (RoundedCorners && CornerRadius > 0) {
    if (sel) {
      int R = PROMPT_SEL_RADIUS;
      int t = PROMPT_SEL_INSET;
      x11_fill_rounded_rect(g_b_dpy, g_b_xd, g_b_vis, g_b_cmap,
                            rgb_to_hex(COLOR_SELECTED_TEXT), 0xFFFF, bx + t,
                            y + PROMPT_SEL_TOP_INSET + t, bw - 2 * t,
                            item_h - 2 - 2 * t, R - 1, fw, frame_h);
    }
  } else {
    PromptState tmp = {0};
    tmp.width = fw;
    tmp.height = frame_h;
    x11_draw_list_background(&tmp, g_b_dpy, g_b_xd, g_b_vis, g_b_cmap, y,
                             item_h, sel);
  }
}

void ListboxBackend_End(void) {
  if (g_b_font.xfont) {
    Platform_DestroyFont(g_b_font);
    g_b_font = FH_NULL;
  }
  g_b_dpy = NULL;
  g_b_xd = NULL;
  g_b_vis = NULL;
  g_b_cmap = 0;
}

PromptState *ListboxBackend_PromptState(const ListBoxInfo *lb) {
  return PromptPopup_FindState(lb);
}
