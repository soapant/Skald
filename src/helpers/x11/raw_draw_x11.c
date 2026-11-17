#include "helpers/x11/raw_draw_x11.h"
#include "config_values.h"

#define RAW_LINE_CAP 4096

__thread Display *g_draw_dpy = NULL;

__thread XftDraw *g_draw_xd = NULL;

__thread Visual *g_draw_vis = NULL;

__thread Colormap g_draw_cmap = 0;

typedef struct {
  char ch;
  unsigned long color;
} RawCell;

__thread RawCell raw_cells[RAW_LINE_CAP];

__thread char raw_text[RAW_LINE_CAP + 1];

__thread int raw_map_x[RAW_LINE_CAP];

__thread int raw_map_top[RAW_LINE_CAP];

__thread int raw_count = 0;

__thread int raw_y = 0;

__thread int raw_x0 = 0;

__thread int raw_pitch = 0;

__thread int raw_fontsize = 0;

__thread FontHandle raw_font;

__thread bool raw_font_owned = false;

__thread Display *raw_dpy = NULL;

__thread XftDraw *raw_xd = NULL;

__thread Visual *raw_vis = NULL;

__thread Colormap raw_cmap = 0;

__thread int raw_map_count = 0;

__thread int raw_map_y = 0;

__thread int raw_map_x0 = 0;

__thread int raw_map_pitch = 0;

__thread XftDraw *raw_map_xd = NULL;

static int x11_raw_measure(Display *dpy, XftFont *xf, const char *s, int n) {
  return x11_measure_utf8_fallback(dpy, xf, s, n, 0);
}

static void x11_raw_emit_line(Display *dpy, XftDraw *xd, XftFont *xf,
                              XftColor *xcols, const unsigned long *ucols,
                              int nu, int ls, int le, int draw_x, int baseline,
                              int row_top) {
  int cx = draw_x;
  int i = ls;
  while (i < le) {
    int j = i + 1;
    while (j < le && raw_cells[j].color == raw_cells[i].color)
      j++;
    int w = x11_raw_measure(dpy, xf, raw_text + i, j - i);
    int k = 0;
    while (k < nu && ucols[k] != raw_cells[i].color)
      k++;
    if (k >= nu)
      k = 0;
    x11_draw_utf8_fallback(dpy, xd, &xcols[k], cx, baseline, xf, raw_text + i,
                           j - i, 0);
    for (int m = i; m < j; m++) {
      raw_map_x[m] = cx;
      raw_map_top[m] = row_top;
    }
    cx += w;
    i = j;
  }
}

void x11_raw_line_flush(void) {
  int count = raw_count;
  raw_count = 0;
  if (count <= 0)
    return;
  Display *dpy = raw_dpy;
  XftDraw *xd = raw_xd;
  Visual *vis = raw_vis;
  Colormap cmap = raw_cmap;
  XftFont *xf = FH_TO_XFT(raw_font);
  int x0 = raw_x0;
  int y = raw_y;
  int pitch = raw_pitch;
  bool owned = raw_font_owned;
  raw_font_owned = false;
  if (!dpy || !xd || !xf) {
    if (owned)
      Platform_DestroyFont(raw_font);
    return;
  }
  for (int i = 0; i < count; i++)
    raw_text[i] = raw_cells[i].ch;
  raw_text[count] = '\0';
  int avail = PromptWidth - x0 - 10;
  if (avail < 40)
    avail = 40;
  int ascent = xf->ascent;
  int line_h = ascent + xf->descent;
  int space_w = x11_raw_measure(dpy, xf, " ", 1);
  unsigned long ucols[64];
  int nu = 0;
  for (int i = 0; i < count && nu < 64; i++) {
    int k = 0;
    while (k < nu && ucols[k] != raw_cells[i].color)
      k++;
    if (k >= nu)
      ucols[nu++] = raw_cells[i].color;
  }
  XftColor xcols[64];
  for (int k = 0; k < nu; k++)
    x11_xft_color_cached(dpy, vis, cmap, ucols[k], 0xFFFF, &xcols[k]);
  int row = 0;
  int p = 0;
  int ls = 0;
  int le = 0;
  int lw = 0;
  while (p < count) {
    int ws = p;
    while (ws < count && raw_text[ws] == ' ')
      ws++;
    if (ws >= count) {
      for (int m = p; m < count; m++) {
        raw_map_x[m] = x0 + lw;
        raw_map_top[m] = y + row * line_h;
      }
      break;
    }
    for (int m = p; m < ws; m++) {
      raw_map_x[m] = x0;
      raw_map_top[m] = y + row * line_h;
    }
    int we = ws;
    while (we < count && raw_text[we] != ' ')
      we++;
    int ww = x11_raw_measure(dpy, xf, raw_text + ws, we - ws);
    if (ww > avail) {
      if (le > ls) {
        x11_raw_emit_line(dpy, xd, xf, xcols, ucols, nu, ls, le, x0,
                          y + ascent + row * line_h, y + row * line_h);
        row++;
      }
      int c = ws;
      while (c < we) {
        int lo = 0, hi = we - c;
        while (lo < hi) {
          int mid = (lo + hi + 1) / 2;
          if (x11_raw_measure(dpy, xf, raw_text + c, mid) <= avail)
            lo = mid;
          else
            hi = mid - 1;
        }
        int n = lo;
        while (n > 0 && (((unsigned char)raw_text[c + n] & 0xC0) == 0x80))
          n--;
        if (n <= 0) {
          unsigned char lc = (unsigned char)raw_text[c];
          if ((lc & 0x80) == 0)
            n = 1;
          else if ((lc & 0xE0) == 0xC0)
            n = 2;
          else if ((lc & 0xF0) == 0xE0)
            n = 3;
          else if ((lc & 0xF8) == 0xF0)
            n = 4;
          else
            n = 1;
          if (n > we - c)
            n = we - c;
        }
        if (c + n >= we) {
          ls = c;
          le = we;
          lw = x11_raw_measure(dpy, xf, raw_text + c, we - c);
          c = we;
          break;
        }
        x11_raw_emit_line(dpy, xd, xf, xcols, ucols, nu, c, c + n, x0,
                          y + ascent + row * line_h, y + row * line_h);
        row++;
        c += n;
      }
      p = we;
      continue;
    }
    int add = ww + (le > ls ? space_w : 0);
    if (le > ls && lw + add > avail) {
      x11_raw_emit_line(dpy, xd, xf, xcols, ucols, nu, ls, le, x0,
                        y + ascent + row * line_h, y + row * line_h);
      row++;
      ls = ws;
      le = we;
      lw = ww;
    } else {
      if (le <= ls)
        ls = ws;
      le = we;
      lw += add;
    }
    p = we;
  }
  if (le > ls) {
    x11_raw_emit_line(dpy, xd, xf, xcols, ucols, nu, ls, le, x0,
                      y + ascent + row * line_h, y + row * line_h);
  }
  raw_map_count = count;
  raw_map_y = y;
  raw_map_x0 = x0;
  raw_map_pitch = pitch;
  raw_map_xd = xd;
  for (int k = 0; k < nu; k++)
    if (owned)
      Platform_DestroyFont(raw_font);
}

bool x11_raw_line_push(int x, int y, char ch, unsigned long color,
                       FontHandle fh, bool owned, int fontsize) {
  if (raw_count > 0) {
    bool same =
        (y == raw_y) && (fontsize == raw_fontsize) && (raw_xd == g_draw_xd);
    if (same && raw_count >= 2)
      same = (x == raw_x0 + raw_count * raw_pitch);
    if (same && raw_count == 1) {
      int p = x - raw_x0;
      if (p <= 0)
        same = false;
      else
        raw_pitch = p;
    }
    if (same && owned && !raw_font_owned)
      same = false;
    if (same && !owned)
      same = (FH_TO_XFT(fh) == FH_TO_XFT(raw_font));
    if (!same)
      x11_raw_line_flush();
  }
  if (raw_count >= RAW_LINE_CAP)
    return false;
  if (raw_count == 0) {
    raw_x0 = x;
    raw_y = y;
    raw_pitch = 0;
    raw_fontsize = fontsize;
    raw_font = fh;
    raw_font_owned = owned;
    raw_dpy = g_draw_dpy;
    raw_xd = g_draw_xd;
    raw_vis = g_draw_vis;
    raw_cmap = g_draw_cmap;
  } else if (owned) {
    Platform_DestroyFont(fh);
  }
  raw_cells[raw_count].ch = ch;
  raw_cells[raw_count].color = color;
  raw_count++;
  return true;
}

void x11_set_draw_context(Display *dpy, XftDraw *xd, Visual *vis,
                          Colormap cmap) {
  x11_raw_line_flush();
  g_draw_dpy = dpy;
  g_draw_xd = xd;
  g_draw_vis = vis;
  g_draw_cmap = cmap;
}

void x11_get_draw_context(Display **dpy, XftDraw **xd, Visual **vis,
                          Colormap *cmap) {

  if (dpy)
    *dpy = g_draw_dpy;
  if (xd)
    *xd = g_draw_xd;
  if (vis)
    *vis = g_draw_vis;
  if (cmap)
    *cmap = g_draw_cmap;
}

#define X11_LINE_FONT_SLOTS 12
typedef struct {
  Display *dpy;
  int size;
  XftFont *f;
  unsigned long stamp;
} LineFontSlot;

static LineFontSlot line_slots[X11_LINE_FONT_SLOTS];
static unsigned long line_stamp = 0;

XftFont *x11_line_font(Display *dpy, int size) {
  if (size <= 0)
    size = FontSize;
  for (int i = 0; i < X11_LINE_FONT_SLOTS; i++) {
    LineFontSlot *s = &line_slots[i];
    if (s->f && s->dpy == dpy && s->size == size) {
      s->stamp = ++line_stamp;
      return s->f;
    }
  }
  if (!dpy)
    return NULL;
  int screen = DefaultScreen(dpy);
  XLockDisplay(dpy);
  XftFont *xf = XftFontOpen(dpy, screen, FC_FAMILY, FcTypeString, UsedFont,
                            FC_PIXEL_SIZE, FcTypeDouble, (double)size, NULL);
  if (!xf)
    xf = XftFontOpen(dpy, screen, FC_FAMILY, FcTypeString, "sans",
                     FC_PIXEL_SIZE, FcTypeDouble, (double)size, NULL);
  if (!xf)
    xf = XftFontOpenName(dpy, screen, "fixed");
  XUnlockDisplay(dpy);
  if (!xf)
    return NULL;
  int slot = -1;
  unsigned long oldest = ~0UL;
  for (int i = 0; i < X11_LINE_FONT_SLOTS; i++) {
    if (!line_slots[i].f) {
      slot = i;
      break;
    }
    if (line_slots[i].stamp < oldest) {
      oldest = line_slots[i].stamp;
      slot = i;
    }
  }
  if (line_slots[slot].f) {
    XftFontClose(line_slots[slot].dpy, line_slots[slot].f);
    x11_bump_measure_epoch();
  }
  line_slots[slot].dpy = dpy;
  line_slots[slot].size = size;
  line_slots[slot].f = xf;
  line_slots[slot].stamp = ++line_stamp;
  return xf;
}

int x11_editbox_measure(const char *text, int len, void *user) {
  EditboxX11Metrics *m = (EditboxX11Metrics *)user;
  if (!m || !m->dpy || !m->xf || !text || len <= 0)
    return 0;
  XGlyphInfo ext;
  XftTextExtentsUtf8(m->dpy, m->xf, (FcChar8 *)text, len, &ext);
  return ext.xOff;
}

void x11_draw_list_background(PromptState *state, Display *dpy, XftDraw *xd,
                              Visual *vis, Colormap cmap, int y, int h,
                              bool selected) {
  if (selected) {
    if (state->no_bg) {
      x11_draw_selection_background(dpy, xd, vis, cmap, 0, y, state->width, h,
                                    rgb_to_hex(COLOR_SELECTED_TEXT));
    } else {
      x11_draw_selection_background(dpy, xd, vis, cmap, 0, y, state->width, h,
                                    rgb_to_hex(COLOR_SELECTED_TEXT));
    }
  }
}

void x11_prompt_editbox_spec(PromptState *state, FontHandle font,
                             EditboxSpec *eb, EditboxX11Metrics *metrics) {
  XftFont *xf = FH_TO_XFT(font);
  int text_len = (int)strlen(state->eb->filter);
  int cursor_x = 0;
  int text_w = 0;
  if (xf) {
    XGlyphInfo cext;
    XftTextExtentsUtf8(metrics->dpy, xf, (FcChar8 *)state->eb->filter,
                       state->eb->cursor, &cext);
    cursor_x = cext.xOff;
    XGlyphInfo text_ext;
    XftTextExtentsUtf8(metrics->dpy, xf, (FcChar8 *)state->eb->filter, text_len,
                       &text_ext);
    text_w = text_ext.xOff;
  }
  int asc = xf ? xf->ascent : 0;
  int desc = xf ? xf->descent : 0;
  editbox_spec_fill_from_prompt(eb, state, text_len,
                                &(EditboxMetrics){.cursor_x = cursor_x,
                                                  .text_w = text_w,
                                                  .ascent = asc,
                                                  .descent = desc,
                                                  .font = font});
  eb->measure = x11_editbox_measure;
  eb->measure_user = metrics;
}

GC x11_raw_gc_for(Display *dpy, Drawable d);

bool x11_raw_can_draw(void) { return g_draw_dpy != NULL && g_draw_xd != NULL; }

GC x11_raw_gc_for(Display *dpy, Drawable d) {
  static GC gc = NULL;
  static Drawable gc_owner = 0;
  if (gc != NULL && gc_owner == (Drawable)(uintptr_t)d)
    return gc;
  if (gc != NULL)
    XFreeGC(dpy, gc);
  gc = XCreateGC(dpy, d, 0, NULL);
  if (!gc)
    return NULL;
  gc_owner = (Drawable)(uintptr_t)d;
  return gc;
}

GC x11_raw_prepare(unsigned long color, int thickness) {
  if (!x11_raw_can_draw())
    return NULL;
  Drawable d = XftDrawDrawable(g_draw_xd);
  GC gc = x11_raw_gc_for(g_draw_dpy, d);
  if (!gc)
    return NULL;
  XftColor col;
  x11_xft_color_cached(g_draw_dpy, g_draw_vis, g_draw_cmap, color,
                       opacity_alpha16(), &col);
  XSetForeground(g_draw_dpy, gc, col.pixel);
  XSetLineAttributes(g_draw_dpy, gc, thickness > 0 ? thickness : 1, LineSolid,
                     CapButt, JoinMiter);
  return gc;
}

void x11_draw_editbox(PromptState *state, Display *dpy, XftDraw *xd,
                      Visual *vis, Colormap cmap) {
  if (!state)
    return;
  SkaldContext_Get()->window.editbox_placed = true;
  FontHandle font = x11_create_font_on_display(dpy, FontSize, UsedFont);
  x11_draw_edit_background(state, dpy, xd, vis, cmap, font);
  x11_draw_edit_tint(state, dpy, xd, vis, cmap, font);
  x11_draw_edit_text(state, dpy, xd, vis, cmap, font);
  x11_draw_edit_cursor(state, dpy, xd, vis, cmap, font);
  Platform_DestroyFont(font);
}

void x11_draw_listbox(PromptState *state, Display *dpy, XftDraw *xd,
                      Visual *vis, Colormap cmap) {

  if (!state || !state->lb)
    return;
  FontHandle font = x11_create_font_on_display(dpy, FontSize, UsedFont);
  ListBoxInfo *lb = state->lb;

  int item_h = prompt_row_height();
  int y0 = prompt_list_top();
  draw_prompt_separator(
      (DrawPromptSeparatorSpec){.y = y0, .width = state->width});
  int y_offset = PROMPT_LIST_BOTTOM_TITLE;
  int visible = (state->height - y0 - y_offset) / item_h;
  if (visible < 1)
    visible = 1;
  if (visible > lb->item_count)
    visible = lb->item_count;
  UI_LB_SetVisibleCount(visible);

  listbox_apply_pending_scroll(lb, visible);

  listbox_set_geometry(y0, item_h, visible);
  listbox_hover_focus(lb, y0, item_h, lb->view_offset, visible);

  for (int i = 0; i < visible; i++) {
    int idx = lb->view_offset + i;
    int y = y0 + i * item_h;
    bool sel = (idx == lb->selected_index);
    if (RoundedCorners && CornerRadius > 0) {
      int R = PROMPT_SEL_RADIUS;
      int t = PROMPT_SEL_INSET;
      int bx = PROMPT_SEL_X, bw = state->width - PROMPT_SEL_W_SHRINK;
      if (sel) {
        x11_fill_rounded_rect(dpy, xd, vis, cmap,
                              rgb_to_hex(COLOR_SELECTED_TEXT), 0xFFFF, bx + t,
                              y + PROMPT_SEL_TOP_INSET + t, bw - 2 * t,
                              item_h - 2 - 2 * t, R - 1);
      }
    } else {
      x11_draw_list_background(state, dpy, xd, vis, cmap, y, item_h, sel);
    }
    x11_draw_list_item(dpy, xd, vis, cmap, font, lb, idx, y, PROMPT_ITEM_TEXT_X,
                       NULL);
  }

  if (prompt_list_bottom_margin() > 0) {
    draw_prompt_separator((DrawPromptSeparatorSpec){
        .y = state->height - PROMPT_LIST_BOTTOM_TITLE, .width = state->width});
  }

  Platform_DestroyFont(font);
}
