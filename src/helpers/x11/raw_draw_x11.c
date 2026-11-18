#include "helpers/x11/raw_draw_x11.h"
#include "config_values.h"

#include <math.h>

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
  listbox_hover_focus(lb, y0, item_h, lb->view_offset, visible, PROMPT_SEL_X,
                      PROMPT_SEL_X + state->width - PROMPT_SEL_W_SHRINK);

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

static float x11_rot_norm(float angle) {
  float a = fmodf(angle, 360.0f);
  if (a < 0.0f)
    a += 360.0f;
  if (a >= 360.0f)
    a -= 360.0f;
  return a;
}

static void x11_rot_bbox(float angle, int w, int h, int *out_w, int *out_h) {
  float a = x11_rot_norm(angle) * (float)(M_PI / 180.0);
  float c = fabsf(cosf(a)), s = fabsf(sinf(a));
  *out_w = (int)ceilf((float)w * c + (float)h * s);
  *out_h = (int)ceilf((float)w * s + (float)h * c);
}

static void x11_rot_transform(XTransform *t, float angle, double dst_cx,
                              double dst_cy, double src_cx, double src_cy) {
  double a = (double)x11_rot_norm(angle) * (M_PI / 180.0);
  double c = cos(a), s = sin(a);
  double e11 = c, e12 = s;
  double e21 = -s, e22 = c;
  double e13 = src_cx - (e11 * dst_cx + e12 * dst_cy);
  double e23 = src_cy - (e21 * dst_cx + e22 * dst_cy);
  t->matrix[0][0] = XDoubleToFixed(e11);
  t->matrix[0][1] = XDoubleToFixed(e12);
  t->matrix[0][2] = XDoubleToFixed(e13);
  t->matrix[1][0] = XDoubleToFixed(e21);
  t->matrix[1][1] = XDoubleToFixed(e22);
  t->matrix[1][2] = XDoubleToFixed(e23);
  t->matrix[2][0] = XDoubleToFixed(0.0);
  t->matrix[2][1] = XDoubleToFixed(0.0);
  t->matrix[2][2] = XDoubleToFixed(1.0);
}

static unsigned char *x11_rot_rect_pixels(int w, int h, float radius,
                                          bool filled, float thickness,
                                          unsigned long hex,
                                          unsigned short alpha16) {
  unsigned char *buf = (unsigned char *)calloc((size_t)w * (size_t)h * 4, 1);
  if (!buf)
    return NULL;
  float bx = (float)w * 0.5f, by = (float)h * 0.5f;
  float r = radius;
  if (r > bx)
    r = bx;
  if (r > by)
    r = by;
  if (r < 0.0f)
    r = 0.0f;
  float t = thickness > 0.0f ? thickness : 1.0f;
  unsigned char cr = (unsigned char)((hex >> 16) & 0xFF);
  unsigned char cg = (unsigned char)((hex >> 8) & 0xFF);
  unsigned char cb = (unsigned char)(hex & 0xFF);
  for (int py = 0; py < h; py++) {
    for (int px = 0; px < w; px++) {
      float dx = fabsf((float)px + 0.5f - bx) - (bx - r);
      float dy = fabsf((float)py + 0.5f - by) - (by - r);
      float ox = fmaxf(dx, 0.0f), oy = fmaxf(dy, 0.0f);
      float sd = sqrtf(ox * ox + oy * oy) + fminf(fmaxf(dx, dy), 0.0f) - r;
      float cov;
      if (filled) {
        cov = 0.5f - sd;
      } else {
        float ad = fabsf(sd) - t * 0.5f;
        cov = 0.5f - ad;
      }
      if (cov <= 0.0f)
        continue;
      if (cov > 1.0f)
        cov = 1.0f;
      float a8 = ((float)alpha16 / 65535.0f) * cov;
      unsigned char A = (unsigned char)(a8 * 255.0f + 0.5f);
      if (A == 0)
        continue;
      size_t o = ((size_t)py * (size_t)w + (size_t)px) * 4;
      buf[o + 0] = A;
      buf[o + 1] = (unsigned char)((float)cr * a8 + 0.5f);
      buf[o + 2] = (unsigned char)((float)cg * a8 + 0.5f);
      buf[o + 3] = (unsigned char)((float)cb * a8 + 0.5f);
    }
  }
  return buf;
}

static void x11_rot_composite(Display *dpy, XftDraw *xd, Visual *vis,
                              Picture src, int dst_x, int dst_y, int bw, int bh,
                              float angle, double dst_cx, double dst_cy,
                              double src_cx, double src_cy) {
  XRenderSetPictureFilter(dpy, src, FilterBilinear, NULL, 0);
  XTransform t;
  x11_rot_transform(&t, angle, dst_cx, dst_cy, src_cx, src_cy);
  XRenderSetPictureTransform(dpy, src, &t);
  XRenderPictFormat *dfmt = XRenderFindVisualFormat(dpy, vis);
  Picture dst =
      dfmt ? XRenderCreatePicture(dpy, XftDrawDrawable(xd), dfmt, 0, NULL) : 0;
  if (dst) {
    XRenderComposite(dpy, PictOpOver, src, None, dst, dst_x, dst_y, 0, 0, dst_x,
                     dst_y, (unsigned int)bw, (unsigned int)bh);
    XRenderFreePicture(dpy, dst);
  }
  return;
}

bool x11_raw_draw_rect_rotated(const DrawRectRawSpec *spec) {
  if (!x11_raw_can_draw() || !spec)
    return false;
  Display *dpy = g_draw_dpy;
  XftDraw *xd = g_draw_xd;
  int w = spec->w, h = spec->h;
  if (w <= 0 || h <= 0)
    return false;
  int x = spec->x + draw_state_get()->off_x;
  int y = spec->y + draw_state_get()->off_y;
  float angle = spec->angle;
  float cx = (float)x + (float)w * 0.5f;
  float cy = (float)y + (float)h * 0.5f;
  int bw = 0, bh = 0;
  x11_rot_bbox(angle, w, h, &bw, &bh);
  int bx = (int)lroundf(cx - (float)bw * 0.5f);
  int by = (int)lroundf(cy - (float)bh * 0.5f);
  int t = draw_thickness_px(spec->thickness);
  if (spec->filled) {
    draw_measure_track(bx, by, bw, bh);
  } else {
    draw_measure_track(bx - t, by - t, bw + 2 * t, bh + 2 * t);
  }
  float radius = 0.0f;
  if (spec->rounded && spec->radius > 0) {
    radius = (float)spec->radius;
    if (radius > (float)w * 0.5f)
      radius = (float)w * 0.5f;
    if (radius > (float)h * 0.5f)
      radius = (float)h * 0.5f;
  }
  unsigned short target_alpha = spec->opaque ? 0xFFFFu : opacity_alpha16();
  unsigned char *buf =
      x11_rot_rect_pixels(w, h, radius, spec->filled, t > 0 ? (float)t : 1.0f,
                          spec->color, target_alpha);
  if (!buf)
    return false;
  Pixmap pm = 0;
  Picture pic = 0;
  bool ok = x11_upload_argb(dpy, 0, buf, w, h, &pm, &pic);
  free(buf);
  if (!ok)
    return false;
  x11_rot_composite(dpy, xd, g_draw_vis, pic, bx, by, bw, bh, angle, (double)cx,
                    (double)cy, (double)w * 0.5, (double)h * 0.5);
  if (pic)
    XRenderFreePicture(dpy, pic);
  if (pm)
    XFreePixmap(dpy, pm);
  XFlush(dpy);
  return ok;
}

bool x11_raw_draw_text_rotated(const DrawTextRawSpec *spec) {
  if (!x11_raw_can_draw() || !spec || !spec->text || !spec->text[0])
    return false;
  Display *dpy = g_draw_dpy;
  XftDraw *xd = g_draw_xd;
  int off_x = draw_state_get()->off_x, off_y = draw_state_get()->off_y;
  int x = spec->x + off_x;
  int y = spec->y + off_y;
  float angle = spec->angle;
  FontHandle fh = spec->font;
  bool need_destroy = false;
  if (!FH_TO_XFT(fh)) {
    if (spec->fontsize) {
      fh = x11_create_font_on_display(dpy, spec->fontsize, UsedFont);
    } else {
      fh = x11_create_font_on_display(dpy, FontSize, UsedFont);
    }
    need_destroy = true;
  }
  XftFont *xf = FH_TO_XFT(fh);
  if (!xf) {
    if (need_destroy)
      Platform_DestroyFont(fh);
    return false;
  }
  int fontsize = spec->fontsize > 0 ? spec->fontsize : FontSize;
  int len = (int)strlen(spec->text);
  int tw = x11_measure_utf8_fallback(dpy, xf, spec->text, len, fontsize);
  int lh = xf->ascent + xf->descent;
  if (tw <= 0 || lh <= 0) {
    if (need_destroy)
      Platform_DestroyFont(fh);
    return false;
  }
  const char *a = spec->align ? spec->align : "CENTER";
  bool centered = string_contains_ci(a, "CENTER");
  bool right = !centered && string_contains_ci(a, "RIGHT");
  int ux = centered ? x - tw / 2 : (right ? x - tw : x);
  float cx = (float)ux + (float)tw * 0.5f;
  float cy = (float)y + (float)lh * 0.5f;
  int bw = 0, bh = 0;
  x11_rot_bbox(angle, tw, lh, &bw, &bh);
  int bx = (int)lroundf(cx - (float)bw * 0.5f);
  int by = (int)lroundf(cy - (float)bh * 0.5f);
  draw_measure_track(bx, by, bw, bh);

  int screen = DefaultScreen(dpy);
  XVisualInfo vinfo;
  if (!XMatchVisualInfo(dpy, screen, 32, TrueColor, &vinfo)) {
    if (need_destroy)
      Platform_DestroyFont(fh);
    return false;
  }
  Pixmap pm = XCreatePixmap(dpy, RootWindow(dpy, screen), (unsigned int)tw,
                            (unsigned int)lh, 32);
  if (!pm) {
    if (need_destroy)
      Platform_DestroyFont(fh);
    return false;
  }
  Colormap cmap =
      XCreateColormap(dpy, RootWindow(dpy, screen), vinfo.visual, AllocNone);
  {
    GC gc = XCreateGC(dpy, pm, 0, NULL);
    XSetForeground(dpy, gc, 0);
    XFillRectangle(dpy, pm, gc, 0, 0, (unsigned int)tw, (unsigned int)lh);
    XFreeGC(dpy, gc);
  }
  XftDraw *td = XftDrawCreate(dpy, pm, vinfo.visual, cmap);
  if (!td) {
    XFreePixmap(dpy, pm);
    XFreeColormap(dpy, cmap);
    if (need_destroy)
      Platform_DestroyFont(fh);
    return false;
  }
  unsigned long color =
      spec->color == COLOR_AUTO ? rgb_to_hex(COLOR_TEXT) : spec->color;
  XftColor col;
  x11_xft_color_cached(dpy, g_draw_vis, g_draw_cmap, color, 0xFFFF, &col);
  XftDrawStringUtf8(td, &col, xf, 0, xf->ascent, (FcChar8 *)spec->text, len);
  XftDrawDestroy(td);

  Picture src = XRenderCreatePicture(
      dpy, pm, XRenderFindStandardFormat(dpy, PictStandardARGB32), 0, NULL);
  bool ok = false;
  if (src) {
    XRenderSetPictureFilter(dpy, src, FilterBilinear, NULL, 0);
    XTransform t;
    x11_rot_transform(&t, angle, (double)bx + (double)bw * 0.5,
                      (double)by + (double)bh * 0.5, (double)tw * 0.5,
                      (double)lh * 0.5);
    XRenderSetPictureTransform(dpy, src, &t);
    XRenderPictFormat *dfmt = XRenderFindVisualFormat(dpy, g_draw_vis);
    Picture dst =
        dfmt ? XRenderCreatePicture(dpy, XftDrawDrawable(xd), dfmt, 0, NULL)
             : 0;
    if (dst) {
      XRenderComposite(dpy, PictOpOver, src, None, dst, bx, by, 0, 0, bx, by,
                       (unsigned int)bw, (unsigned int)bh);
      XRenderFreePicture(dpy, dst);
      ok = true;
    }
    XRenderFreePicture(dpy, src);
  }
  XFreePixmap(dpy, pm);
  XFreeColormap(dpy, cmap);
  if (need_destroy)
    Platform_DestroyFont(fh);
  XFlush(dpy);
  return ok;
}
