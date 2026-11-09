#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xlib.h>
#include <string.h>
#include <X11/X.h>
#include <fontconfig/fontconfig.h>
#include <stdbool.h>
#include <stdint.h>

#include "platform/draw.h"
#include "platform/platform.h"
#include "platform/ui.h"
#include "core/prompt_geometry.h"
#include "helpers/helpers_ui.h"
#include "core/draw_state.h"
#include "core/listbox_hover.h"
#include "core/editbox.h"
#include "platform/x11/drawing_x11.h"
#include "platform/x11/display_x11.h"
#include "config_values.h"
#include "core/context.h"
#include "core/types/window.h"

void Platform_SetTransparentWindow(bool on) {
  PlDrawState_Get()->transparent = on;
}
bool Platform_GetTransparentWindow(void) {
  return PlDrawState_Get()->transparent;
}

void Platform_SetDrawBgColor(unsigned long hex) {
  PlDrawState_Get()->bg_override = hex;
}
unsigned long Platform_GetDrawBgColor(void) {
  return PlDrawState_Get()->bg_override;
}

void Platform_BeginMeasure(void) { PlDrawState_BeginMeasure(); }
void Platform_EndMeasure(int *out_w, int *out_h) {
  PlDrawState_EndMeasure(out_w, out_h);
}
void Platform_GetLastMeasureOrigin(int *out_x, int *out_y) {
  if (out_x)
    *out_x = PlDrawState_Get()->meas_origin_x;
  if (out_y)
    *out_y = PlDrawState_Get()->meas_origin_y;
}

void Platform_SetDrawOffset(int dx, int dy) {
  PlDrawState_Get()->off_x = dx;
  PlDrawState_Get()->off_y = dy;
}

void Platform_GetDrawOffset(int *out_x, int *out_y) {
  if (out_x)
    *out_x = PlDrawState_Get()->off_x;
  if (out_y)
    *out_y = PlDrawState_Get()->off_y;
}

bool Platform_WindowToRoot(WindowHandle h, int *out_x, int *out_y) {
  Display *dpy = get_display();
  if (!dpy)
    return false;
  Window w = (Window)WH_TO_HWND(h);
  if (w == None)
    return false;
  Window child = None;
  int ax = 0, ay = 0;
  if (!XTranslateCoordinates(dpy, w, DefaultRootWindow(dpy), 0, 0, &ax, &ay,
                             &child))
    return false;
  if (out_x)
    *out_x = ax;
  if (out_y)
    *out_y = ay;
  return true;
}
static void measure_track(int x, int y, int w, int h) {
  PlDrawState_TrackMeasure(x, y, w, h);
}

static __thread Display *g_draw_dpy = NULL;
static __thread XftDraw *g_draw_xd = NULL;
static __thread Visual *g_draw_vis = NULL;
static __thread Colormap g_draw_cmap = 0;
#define RAW_LINE_CAP 4096
typedef struct {
  char ch;
  unsigned long color;
} RawCell;
static __thread RawCell g_raw_cells[RAW_LINE_CAP];
static __thread char g_raw_text[RAW_LINE_CAP + 1];
static __thread int g_raw_map_x[RAW_LINE_CAP];
static __thread int g_raw_map_top[RAW_LINE_CAP];
static __thread int g_raw_count = 0;
static __thread int g_raw_y = 0;
static __thread int g_raw_x0 = 0;
static __thread int g_raw_pitch = 0;
static __thread int g_raw_fontsize = 0;
static __thread FontHandle g_raw_font;
static __thread bool g_raw_font_owned = false;
static __thread Display *g_raw_dpy = NULL;
static __thread XftDraw *g_raw_xd = NULL;
static __thread Visual *g_raw_vis = NULL;
static __thread Colormap g_raw_cmap = 0;
static __thread int g_raw_map_count = 0;
static __thread int g_raw_map_y = 0;
static __thread int g_raw_map_x0 = 0;
static __thread int g_raw_map_pitch = 0;
static __thread XftDraw *g_raw_map_xd = NULL;
static int RawMeasure(Display *dpy, XftFont *xf, const char *s, int n) {
  return x11_measure_utf8_fallback(dpy, xf, s, n, 0);
}
static void RawEmitLine(Display *dpy, XftDraw *xd, XftFont *xf, XftColor *xcols,
                        unsigned long *ucols, int nu, int ls, int le, int drawX,
                        int baseline, int rowTop) {
  int cx = drawX;
  int i = ls;
  while (i < le) {
    int j = i + 1;
    while (j < le && g_raw_cells[j].color == g_raw_cells[i].color)
      j++;
    int w = RawMeasure(dpy, xf, g_raw_text + i, j - i);
    int k = 0;
    while (k < nu && ucols[k] != g_raw_cells[i].color)
      k++;
    if (k >= nu)
      k = 0;
    x11_draw_utf8_fallback(dpy, xd, &xcols[k], cx, baseline, xf, g_raw_text + i,
                           j - i, 0);
    for (int m = i; m < j; m++) {
      g_raw_map_x[m] = cx;
      g_raw_map_top[m] = rowTop;
    }
    cx += w;
    i = j;
  }
}

static void RawLine_Flush(void) {
  int count = g_raw_count;
  g_raw_count = 0;
  if (count <= 0)
    return;
  Display *dpy = g_raw_dpy;
  XftDraw *xd = g_raw_xd;
  Visual *vis = g_raw_vis;
  Colormap cmap = g_raw_cmap;
  XftFont *xf = FH_TO_XFT(g_raw_font);
  int x0 = g_raw_x0;
  int y = g_raw_y;
  int pitch = g_raw_pitch;
  bool owned = g_raw_font_owned;
  g_raw_font_owned = false;
  if (!dpy || !xd || !xf) {
    if (owned)
      Platform_DestroyFont(g_raw_font);
    return;
  }
  for (int i = 0; i < count; i++)
    g_raw_text[i] = g_raw_cells[i].ch;
  g_raw_text[count] = '\0';
  int avail = 900 - x0 - 10;
  if (avail < 40)
    avail = 40;
  int ascent = xf->ascent;
  int lineH = ascent + xf->descent;
  int spaceW = RawMeasure(dpy, xf, " ", 1);
  unsigned long ucols[64];
  int nu = 0;
  for (int i = 0; i < count && nu < 64; i++) {
    int k = 0;
    while (k < nu && ucols[k] != g_raw_cells[i].color)
      k++;
    if (k >= nu)
      ucols[nu++] = g_raw_cells[i].color;
  }
  XftColor xcols[64];
  for (int k = 0; k < nu; k++)
    x11_xft_alloc_color(dpy, vis, cmap, ucols[k], 0xFFFF, &xcols[k]);
  int row = 0;
  int p = 0;
  int ls = 0;
  int le = 0;
  int lw = 0;
  while (p < count) {
    int ws = p;
    while (ws < count && g_raw_text[ws] == ' ')
      ws++;
    if (ws >= count) {
      for (int m = p; m < count; m++) {
        g_raw_map_x[m] = x0 + lw;
        g_raw_map_top[m] = y + row * lineH;
      }
      break;
    }
    for (int m = p; m < ws; m++) {
      g_raw_map_x[m] = x0;
      g_raw_map_top[m] = y + row * lineH;
    }
    int we = ws;
    while (we < count && g_raw_text[we] != ' ')
      we++;
    int ww = RawMeasure(dpy, xf, g_raw_text + ws, we - ws);
    if (ww > avail) {
      if (le > ls) {
        RawEmitLine(dpy, xd, xf, xcols, ucols, nu, ls, le, x0,
                    y + ascent + row * lineH, y + row * lineH);
        row++;
      }
      int c = ws;
      while (c < we) {
        int lo = 0, hi = we - c;
        while (lo < hi) {
          int mid = (lo + hi + 1) / 2;
          if (RawMeasure(dpy, xf, g_raw_text + c, mid) <= avail)
            lo = mid;
          else
            hi = mid - 1;
        }
        int n = lo;
        while (n > 0 && (((unsigned char)g_raw_text[c + n] & 0xC0) == 0x80))
          n--;
        if (n <= 0) {
          unsigned char lc = (unsigned char)g_raw_text[c];
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
          lw = RawMeasure(dpy, xf, g_raw_text + c, we - c);
          c = we;
          break;
        }
        RawEmitLine(dpy, xd, xf, xcols, ucols, nu, c, c + n, x0,
                    y + ascent + row * lineH, y + row * lineH);
        row++;
        c += n;
      }
      p = we;
      continue;
    }
    int add = ww + (le > ls ? spaceW : 0);
    if (le > ls && lw + add > avail) {
      RawEmitLine(dpy, xd, xf, xcols, ucols, nu, ls, le, x0,
                  y + ascent + row * lineH, y + row * lineH);
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
    RawEmitLine(dpy, xd, xf, xcols, ucols, nu, ls, le, x0,
                y + ascent + row * lineH, y + row * lineH);
  }
  g_raw_map_count = count;
  g_raw_map_y = y;
  g_raw_map_x0 = x0;
  g_raw_map_pitch = pitch;
  g_raw_map_xd = xd;
  for (int k = 0; k < nu; k++)
    XftColorFree(dpy, vis, cmap, &xcols[k]);
  if (owned)
    Platform_DestroyFont(g_raw_font);
}
static bool RawLine_Push(int x, int y, char ch, unsigned long color,
                         FontHandle fh, bool owned, int fontsize) {
  if (g_raw_count > 0) {
    bool same = (y == g_raw_y) && (fontsize == g_raw_fontsize) &&
                (g_raw_xd == g_draw_xd);
    if (same && g_raw_count >= 2)
      same = (x == g_raw_x0 + g_raw_count * g_raw_pitch);
    if (same && g_raw_count == 1) {
      int p = x - g_raw_x0;
      if (p <= 0)
        same = false;
      else
        g_raw_pitch = p;
    }
    if (same && owned && !g_raw_font_owned)
      same = false;
    if (same && !owned)
      same = (FH_TO_XFT(fh) == FH_TO_XFT(g_raw_font));
    if (!same)
      RawLine_Flush();
  }
  if (g_raw_count >= RAW_LINE_CAP)
    return false;
  if (g_raw_count == 0) {
    g_raw_x0 = x;
    g_raw_y = y;
    g_raw_pitch = 0;
    g_raw_fontsize = fontsize;
    g_raw_font = fh;
    g_raw_font_owned = owned;
    g_raw_dpy = g_draw_dpy;
    g_raw_xd = g_draw_xd;
    g_raw_vis = g_draw_vis;
    g_raw_cmap = g_draw_cmap;
  } else if (owned) {
    Platform_DestroyFont(fh);
  }
  g_raw_cells[g_raw_count].ch = ch;
  g_raw_cells[g_raw_count].color = color;
  g_raw_count++;
  return true;
}

void Platform_SetDrawContext(Display *dpy, XftDraw *xd, Visual *vis,
                             Colormap cmap) {
  RawLine_Flush();
  g_draw_dpy = dpy;
  g_draw_xd = xd;
  g_draw_vis = vis;
  g_draw_cmap = cmap;
}

void Platform_FlushDraw(void) { RawLine_Flush(); }
bool Platform_DrawImageAt(const char *path, int x, int y, int w, int h) {
  if (!path || !path[0] || !g_draw_dpy || !g_draw_xd)
    return false;
  int _offx = PlDrawState_Get()->off_x;
  int _offy = PlDrawState_Get()->off_y;
  x += _offx;
  y += _offy;
  if (w <= 0)
    w = IconSize;
  if (h <= 0)
    h = IconSize;
  measure_track(x, y, w, h);
  return Platform_DrawImageScaled(g_draw_dpy, g_draw_xd, g_draw_vis, path, x, y,
                                  w, h);
}
void Platform_GetDrawContext(Display **dpy, XftDraw **xd, Visual **vis,
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
bool Platform_DrawTextRaw(PlDrawTextRawSpec spec) {
  if (!spec.text || !g_draw_dpy || !g_draw_xd)
    return false;
  int _ox = PlDrawState_Get()->off_x;
  int _oy = PlDrawState_Get()->off_y;
  int x = spec.x + _ox;
  int y = spec.y + _oy;
  FontHandle fh = spec.font;
  bool needDestroy = false;
  if (!FH_TO_XFT(fh)) {
    if (spec.fontsize) {
      fh = Platform_CreateFontOnDisplay(g_draw_dpy, spec.fontsize, UsedFont);
    } else {
      fh = Platform_CreateFontOnDisplay(g_draw_dpy, FontSize, UsedFont);
    }
    needDestroy = true;
  }
  unsigned long color = spec.color == 0 ? rgb_to_hex(COLOR_TEXT) : spec.color;
  const char *a = spec.align ? spec.align : "CENTER";
  if (PlDrawState_Get()->measuring) {
    XftFont *mf = FH_TO_XFT(fh);
    int tw = 0;
    if (mf) {
      tw = x11_measure_utf8_fallback(
          g_draw_dpy, mf, spec.text, (int)strlen(spec.text),
          spec.fontsize > 0 ? spec.fontsize : FontSize);
    }
    int th = mf ? (mf->ascent + mf->descent)
                : (spec.fontsize > 0 ? spec.fontsize : FontSize);
    int bx = x;
    if (Platform_StringContainsCaseInsensitive(a, "CENTER"))
      bx = x - tw / 2;
    else if (Platform_StringContainsCaseInsensitive(a, "RIGHT"))
      bx = x - tw;
    measure_track(bx, y, tw, th);
  }
  XftColor col;
  x11_xft_alloc_color(g_draw_dpy, g_draw_vis, g_draw_cmap, color, 0xFFFF, &col);
  if (Platform_StringContainsCaseInsensitive(a, "CENTER") ||
      Platform_StringContainsCaseInsensitive(a, "LEFT") ||
      Platform_StringContainsCaseInsensitive(a, "RIGHT")) {
    int box = 0;
    if (spec.pad == 0 && !Platform_StringContainsCaseInsensitive(a, "LEFT"))
      box = x;
    draw_wrapped_text(g_draw_dpy, g_draw_xd, &col, fh, spec.text, y, (char *)a,
                      spec.pad ? spec.pad : x, box);
  } else {
    XftFont *xf = FH_TO_XFT(fh);
    int len = (int)strlen(spec.text);
    if (len == 1 && xf &&
        RawLine_Push(x, y, spec.text[0], color, fh, needDestroy,
                     spec.fontsize)) {
      needDestroy = false;
    } else {
      RawLine_Flush();
      if (xf)
        x11_draw_utf8_fallback(g_draw_dpy, g_draw_xd, &col, x, y + (xf->ascent),
                               xf, spec.text, len,
                               spec.fontsize > 0 ? spec.fontsize : FontSize);
    }
  }
  XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &col);
  if (needDestroy)
    Platform_DestroyFont(fh);
  return true;
}

void Platform_DrawTextCentered(PlDrawTextCenteredSpec spec) {
  if (!spec.text || !g_draw_dpy || !g_draw_xd)
    return;
  XftFont *xf = FH_TO_XFT(spec.font);
  int ascent = xf ? xf->ascent : (spec.fontsize * 3 / 4);
  int descent = xf ? xf->descent : (spec.fontsize / 4);
  int lineHeight = ascent + descent;
  int tw = Platform_MeasureTextWidth(
      (PlMeasureTextWidthSpec){.text = spec.text,
                               .len = (int)strlen(spec.text),
                               .fontsize = spec.fontsize});
  int tx = spec.x + (spec.w - tw) / 2;
  int ty = spec.y + (spec.h - lineHeight) / 2;
  Platform_DrawTextRaw((PlDrawTextRawSpec){.x = tx,
                                           .y = ty,
                                           .text = spec.text,
                                           .color = spec.color,
                                           .font = spec.font,
                                           .align = "LEFT",
                                           .pad = 0,
                                           .fontsize = spec.fontsize});
}

static Display *g_line_dpy = NULL;
static int g_line_size = 0;
static XftFont *g_line_font = NULL;

static XftFont *LineFont(Display *dpy, int size) {
  if (size <= 0)
    size = FontSize;
  if (!g_line_font || g_line_dpy != dpy || g_line_size != size) {
    if (g_line_font && g_line_dpy)
      XftFontClose(g_line_dpy, g_line_font);
    g_line_font = NULL;
    g_line_dpy = NULL;
    g_line_size = 0;
    if (!dpy)
      return NULL;
    int screen = DefaultScreen(dpy);
    XftFont *xf = XftFontOpen(dpy, screen, FC_FAMILY, FcTypeString, UsedFont,
                              FC_PIXEL_SIZE, FcTypeDouble, (double)size, NULL);
    if (!xf)
      xf = XftFontOpen(dpy, screen, FC_FAMILY, FcTypeString, "sans",
                       FC_PIXEL_SIZE, FcTypeDouble, (double)size, NULL);
    if (!xf)
      xf = XftFontOpenName(dpy, screen, "fixed");
    if (!xf)
      return NULL;
    g_line_font = xf;
    g_line_dpy = dpy;
    g_line_size = size;
  }
  return g_line_font;
}

int x11_editbox_measure(const char *text, int len, void *user) {
  EditboxX11Metrics *m = (EditboxX11Metrics *)user;
  if (!m || !m->dpy || !m->xf || !text || len <= 0)
    return 0;
  XGlyphInfo ext;
  XftTextExtentsUtf8(m->dpy, m->xf, (FcChar8 *)text, len, &ext);
  return ext.xOff;
}

int Platform_MeasureTextWidth(PlMeasureTextWidthSpec spec) {
  if (!spec.text || spec.len <= 0)
    return 0;
  if (!g_draw_dpy)
    return spec.len * (spec.fontsize > 0 ? spec.fontsize : FontSize) / 2;
  XftFont *xf = LineFont(g_draw_dpy, spec.fontsize);
  if (!xf)
    return spec.len * (spec.fontsize > 0 ? spec.fontsize : FontSize) / 2;
  return x11_measure_utf8_fallback(g_draw_dpy, xf, spec.text, spec.len,
                                   spec.fontsize > 0 ? spec.fontsize
                                                     : FontSize);
}

bool Platform_DrawTextLine(PlDrawTextLineSpec spec) {
  if (!spec.text || spec.len <= 0 || !g_draw_dpy || !g_draw_xd)
    return false;
  int x = spec.x + PlDrawState_Get()->off_x;
  int y = spec.y + PlDrawState_Get()->off_y;
  XftFont *xf = LineFont(g_draw_dpy, spec.fontsize);
  if (!xf)
    return false;
  unsigned long color = spec.color == 0 ? rgb_to_hex(COLOR_TEXT) : spec.color;
  XftColor col;
  x11_xft_alloc_color(g_draw_dpy, g_draw_vis, g_draw_cmap, color, 0xFFFF, &col);
  x11_draw_utf8_fallback(g_draw_dpy, g_draw_xd, &col, x, y + xf->ascent, xf,
                         spec.text, spec.len,
                         spec.fontsize > 0 ? spec.fontsize : FontSize);
  XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &col);
  return true;
}

void Platform_DrawListBackground(PromptState *state, Display *dpy, XftDraw *xd,
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

void Platform_GetItemColors(PlGetItemColorsSpec spec) {
  unsigned long col_text = rgb_to_hex(COLOR_TEXT);
  float t = (spec.count > 1) ? (float)spec.idx / (float)(spec.count - 1)
                             : (float)spec.idx / 10.0f;
  unsigned long col_rainbow = rgb_to_hex(GetRainbowColor(t, 0.55f, 1.0f));
  unsigned long n = col_text, b = col_text, p = col_rainbow;
  if (RainbowText) {
    n = b = p = col_rainbow;
  }
  if (spec.num_color)
    *spec.num_color = n;
  if (spec.body_color)
    *spec.body_color = b;
  if (spec.plain_color)
    *spec.plain_color = p;
}

void Platform_DrawListItem(PromptState *state, Display *dpy, XftDraw *xd,
                           Visual *vis, Colormap cmap, FontHandle font,
                           ListBoxInfo *lb, int idx, int y, int text_x,
                           const char *icon_path) {
  unsigned long ncol, bcol, pcol;
  Platform_GetItemColors((PlGetItemColorsSpec){.idx = idx,
                                               .count = lb->item_count,
                                               .num_color = &ncol,
                                               .body_color = &bcol,
                                               .plain_color = &pcol});
  XftColor ncolor, bcolor, pcolor;
  x11_xft_alloc_color(dpy, vis, cmap, ncol, 0xFFFF, &ncolor);
  x11_xft_alloc_color(dpy, vis, cmap, bcol, 0xFFFF, &bcolor);
  x11_xft_alloc_color(dpy, vis, cmap, pcol, 0xFFFF, &pcolor);
  DrawListBoxItemText(dpy, xd, vis, cmap, font, &ncolor, &bcolor, &pcolor, lb,
                      (char *)icon_path, idx, y, text_x);
  XftColorFree(dpy, vis, cmap, &ncolor);
  XftColorFree(dpy, vis, cmap, &bcolor);
  XftColorFree(dpy, vis, cmap, &pcolor);
}

void Platform_DrawEditBackground(PromptState *state, Display *dpy, XftDraw *xd,
                                 Visual *vis, Colormap cmap, FontHandle font) {
  Platform_SetDrawContext(dpy, xd, vis, cmap);
  if (WindowTitleText)
    Platform_DrawTextRaw(
        (PlDrawTextRawSpec){.x = PROMPT_FOOTER_X,
                            .y = state->height - PROMPT_FOOTER_Y_FROM_BOTTOM,
                            .text = state->window_title,
                            .color = rgb_to_hex(COLOR_TEXT),
                            .font = font,
                            .align = "LEFT",
                            .pad = PROMPT_FOOTER_PAD,
                            .fontsize = FontSize});
}
static void x11_prompt_editbox_spec(PromptState *state, FontHandle font,
                                    EditboxSpec *eb,
                                    EditboxX11Metrics *metrics) {
  XftFont *xf = FH_TO_XFT(font);
  int text_len = (int)strlen(state->eb->filter);
  int cursor_x = 0;
  int text_w = 0;
  if (xf) {
    XGlyphInfo cext;
    XftTextExtentsUtf8(metrics->dpy, xf, (FcChar8 *)state->eb->filter,
                       state->eb->cursor, &cext);
    cursor_x = cext.xOff;
    XGlyphInfo textExt;
    XftTextExtentsUtf8(metrics->dpy, xf, (FcChar8 *)state->eb->filter, text_len,
                       &textExt);
    text_w = textExt.xOff;
  }
  int sel_start = 0, sel_end = 0;
  bool has_sel = false;
  Editbox_ResolveSelection(state, text_len, false, &sel_start, &sel_end,
                           &has_sel);
  memset(eb, 0, sizeof(*eb));
  eb->text = state->eb->filter;
  eb->text_len = text_len;
  eb->cursor = state->eb->cursor;
  eb->sel_start = sel_start;
  eb->sel_end = sel_end;
  eb->has_selection = has_sel;
  int asc = xf ? xf->ascent : 0;
  int desc = xf ? xf->descent : 0;
  int edit_y = PROMPT_EDIT_INSET;
  if (!state->lb && state->height > 0) {
    int line_h = asc + desc;
    if (line_h <= 0)
      line_h = FontSize > 0 ? FontSize : 16;
    edit_y = (state->height - line_h) / 2;
    if (edit_y < 2)
      edit_y = 2;
  }
  eb->sel_mode = EDITBOX_SEL_INCLUSIVE_CARET;
  eb->x = 0;
  eb->y = edit_y;
  eb->w = state->width;
  eb->inset = PROMPT_EDIT_INSET;
  eb->visible_w = state->width - PROMPT_EDIT_SCROLL_PAD;
  eb->scroll = state->eb->scroll_offset;
  eb->cursor_x = cursor_x;
  eb->text_w = text_w;
  eb->text_y = edit_y;
  eb->caret_y = Editbox_CaretY(edit_y, asc, desc, FontSize);
  eb->caret_h = PromptCaretHeight();
  eb->placeholder_dy = 2;
  eb->rainbow_limit_x = -1;
  eb->placeholder = state->eb->placeholder;
  eb->text_color = rgb_to_hex(COLOR_TEXT);
  eb->font = font;
  eb->measure = x11_editbox_measure;
  eb->measure_user = metrics;
}

void Platform_DrawEditText(PromptState *state, Display *dpy, XftDraw *xd,
                           Visual *vis, Colormap cmap, FontHandle font) {
  if (!state)
    return;
  Platform_SetDrawContext(dpy, xd, vis, cmap);
  EditboxX11Metrics metrics = {dpy, FH_TO_XFT(font)};
  EditboxSpec eb;
  EditboxLayout lay;
  x11_prompt_editbox_spec(state, font, &eb, &metrics);
  Editbox_Layout(&eb, &lay);
  state->eb->scroll_offset = eb.scroll;
  Editbox_Draw(&eb, &lay, EDITBOX_PART_TEXT);
}

void Platform_DrawEditTint(PromptState *state, Display *dpy, XftDraw *xd,
                           Visual *vis, Colormap cmap, FontHandle font) {
  Platform_SetDrawContext(dpy, xd, vis, cmap);
  EditboxX11Metrics metrics = {dpy, FH_TO_XFT(font)};
  EditboxSpec eb;
  EditboxLayout lay;
  x11_prompt_editbox_spec(state, font, &eb, &metrics);
  Editbox_Layout(&eb, &lay);
  Editbox_Draw(&eb, &lay, EDITBOX_PART_TINT);
}
void Platform_DrawEditCursor(PromptState *state, Display *dpy, XftDraw *xd,
                             Visual *vis, Colormap cmap, FontHandle font) {
  Platform_SetDrawContext(dpy, xd, vis, cmap);
  EditboxX11Metrics metrics = {dpy, FH_TO_XFT(font)};
  EditboxSpec eb;
  EditboxLayout lay;
  x11_prompt_editbox_spec(state, font, &eb, &metrics);
  Editbox_Layout(&eb, &lay);
  Editbox_Draw(&eb, &lay, EDITBOX_PART_CARET);
}

static GC raw_gc_for(Display *dpy, Drawable d);

static int raw_thickness_px(float thickness) {
  if (thickness <= 0.0f)
    return 0;
  int px = (int)(thickness + 0.5f);
  if (px < 1)
    px = 1;
  if (px > 255)
    px = 255;
  return px;
}

static bool raw_can_draw(void) {
  return g_draw_dpy != NULL && g_draw_xd != NULL;
}

static GC raw_gc_for(Display *dpy, Drawable d) {
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

static GC raw_prepare(unsigned long color, int thickness) {
  if (!raw_can_draw())
    return NULL;
  Drawable d = XftDrawDrawable(g_draw_xd);
  GC gc = raw_gc_for(g_draw_dpy, d);
  if (!gc)
    return NULL;
  XftColor col;
  x11_xft_alloc_color(g_draw_dpy, g_draw_vis, g_draw_cmap, color,
                      OpacityAlpha16(), &col);
  XSetForeground(g_draw_dpy, gc, col.pixel);
  XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &col);
  XSetLineAttributes(g_draw_dpy, gc, thickness > 0 ? thickness : 1, LineSolid,
                     CapButt, JoinMiter);
  return gc;
}

bool Platform_DrawRectRaw(PlDrawRectRawSpec spec) {
  int x = spec.x;
  int y = spec.y;
  int w = spec.w;
  int h = spec.h;
  unsigned long color = spec.color;
  int radius = spec.radius;
  bool rounded = spec.rounded;
  bool opaque = spec.opaque;
  float thickness = spec.thickness;
  bool filled = spec.filled;

  if (!raw_can_draw()) {
    return false;
  }

  if (w <= 0 || h <= 0) {
    return false;
  }

  x += PlDrawState_Get()->off_x;
  y += PlDrawState_Get()->off_y;

  int t = raw_thickness_px(thickness);
  if (t <= 0)
    t = 1;

  bool want_round = rounded && radius > 0;
  int r = 0;
  if (want_round) {
    r = radius;
    if (r * 2 > w)
      r = w / 2;
    if (r * 2 > h)
      r = h / 2;
    if (r < 0)
      r = 0;
  }

  if (filled) {
    measure_track(x, y, w, h);
  } else {
    measure_track(x - t, y - t, w + 2 * t, h + 2 * t);
  }
  unsigned short target_alpha = opaque ? 0xFFFFu : OpacityAlpha16();

  if (filled) {
    if (want_round && r > 0) {
      x11_fill_rounded_rect(g_draw_dpy, g_draw_xd, g_draw_vis, g_draw_cmap,
                            color, target_alpha, x, y, w, h, r, w, h);
    } else {
      XftColor col;
      x11_xft_alloc_color(g_draw_dpy, g_draw_vis, g_draw_cmap, color,
                          target_alpha, &col);
      XftDrawRect(g_draw_xd, &col, x, y, w, h);
      XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &col);
    }
    return true;
  }

  Drawable d = XftDrawDrawable(g_draw_xd);
  GC gc = raw_gc_for(g_draw_dpy, d);
  if (!gc) {
    return false;
  }
  XftColor col;
  x11_xft_alloc_color(g_draw_dpy, g_draw_vis, g_draw_cmap, color, target_alpha,
                      &col);
  XSetForeground(g_draw_dpy, gc, col.pixel);
  XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &col);
  XSetLineAttributes(g_draw_dpy, gc, t, LineSolid, CapButt, JoinMiter);
  if (want_round) {
    if (r > 0) {
      XArc arcs[4] = {
          {(short)x, (short)y, (unsigned short)(r * 2), (unsigned short)(r * 2),
           90 * 64, 90 * 64},
          {(short)(x + w - r * 2), (short)y, (unsigned short)(r * 2),
           (unsigned short)(r * 2), 0 * 64, 90 * 64},
          {(short)(x + w - r * 2), (short)(y + h - r * 2),
           (unsigned short)(r * 2), (unsigned short)(r * 2), 270 * 64, 90 * 64},
          (short)x,
          (short)(y + h - r * 2),
          (unsigned short)(r * 2),
          (unsigned short)(r * 2),
          180 * 64,
          90 * 64};
      XDrawArcs(g_draw_dpy, d, gc, arcs, 4);

      XSegment segments[4] = {{x + r, y, x + w - r, y},
                              {x + w, y + r, x + w, y + h - r},
                              {x + r, y + h, x + w - r, y + h},
                              x,
                              y + r,
                              x,
                              y + h - r};
      XDrawSegments(g_draw_dpy, d, gc, segments, 4);
    } else {
      XDrawRectangle(g_draw_dpy, d, gc, x, y, w, h);
    }
  } else {
    XftColor border_col;
    x11_xft_alloc_color(g_draw_dpy, g_draw_vis, g_draw_cmap, color,
                        target_alpha, &border_col);

    XftDrawRect(g_draw_xd, &border_col, x, y, w, t);
    XftDrawRect(g_draw_xd, &border_col, x, y + h - t, w, t);
    XftDrawRect(g_draw_xd, &border_col, x, y, t, h);
    XftDrawRect(g_draw_xd, &border_col, x + w - t, y, t, h);

    XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &border_col);
  }
  return true;
}

bool Platform_DrawLineRaw(PlDrawLineRawSpec spec) {
  if (!raw_can_draw())
    return false;
  int t = raw_thickness_px(spec.thickness);
  if (t < 1)
    t = 1;
  int x1 = spec.x1 + PlDrawState_Get()->off_x;
  int y1 = spec.y1 + PlDrawState_Get()->off_y;
  int x2 = spec.x2 + PlDrawState_Get()->off_x;
  int y2 = spec.y2 + PlDrawState_Get()->off_y;
  int bx = x1 < x2 ? x1 : x2;
  int by = y1 < y2 ? y1 : y2;
  int bw = (x1 > x2 ? x1 - x2 : x2 - x1) + t;
  int bh = (y1 > y2 ? y1 - y2 : y2 - y1) + t;
  measure_track(bx - t / 2, by - t / 2, bw, bh);
  GC gc = raw_prepare(spec.color, t);
  if (!gc)
    return false;
  Drawable d = XftDrawDrawable(g_draw_xd);
  XDrawLine(g_draw_dpy, d, gc, x1, y1, x2, y2);
  return true;
}

bool Platform_DrawCircleRaw(PlDrawCircleRawSpec spec) {
  if (!raw_can_draw() || spec.radius <= 0)
    return false;
  int t = raw_thickness_px(spec.thickness);
  int cx = spec.cx + PlDrawState_Get()->off_x;
  int cy = spec.cy + PlDrawState_Get()->off_y;
  measure_track(cx - spec.radius, cy - spec.radius, spec.radius * 2,
                spec.radius * 2);
  GC gc = raw_prepare(spec.color, t);
  if (!gc)
    return false;
  Drawable d = XftDrawDrawable(g_draw_xd);
  unsigned dd = (unsigned)(spec.radius * 2);
  if (t > 0)
    XDrawArc(g_draw_dpy, d, gc, cx - spec.radius, cy - spec.radius, dd, dd, 0,
             360 * 64);
  else
    XFillArc(g_draw_dpy, d, gc, cx - spec.radius, cy - spec.radius, dd, dd, 0,
             360 * 64);
  return true;
}

void Platform_DrawPromptSeparator(PlDrawPromptSeparatorSpec spec) {
  if (!EnableBorders || spec.width <= 0)
    return;
  Platform_DrawRectRaw((PlDrawRectRawSpec){.x = 0,
                                           .y = spec.y,
                                           .w = spec.width,
                                           .h = PROMPT_SEP_W,
                                           .color = rgb_to_hex(COLOR_BORDER),
                                           .radius = 0,
                                           .rounded = false,
                                           .opaque = true,
                                           .thickness = 1,
                                           .filled = true});
}

void Platform_DrawEditBox(PromptState *state, Display *dpy, XftDraw *xd,
                          Visual *vis, Colormap cmap) {
  if (!state)
    return;
  FlintContext_Get()->window.editbox_placed = true;
  FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
  Platform_DrawEditBackground(state, dpy, xd, vis, cmap, font);
  Platform_DrawEditTint(state, dpy, xd, vis, cmap, font);
  Platform_DrawEditText(state, dpy, xd, vis, cmap, font);
  Platform_DrawEditCursor(state, dpy, xd, vis, cmap, font);
  Platform_DestroyFont(font);
}

void Platform_GetTextLayoutMetrics(int *line_h, int *row_h, int *list_top) {
  Display *dpy = get_display();
  FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
  int lh = FontSize;
  int rh = x11_font_row_height_of(font);
  int lt = x11_font_list_top_of(font);
  Platform_DestroyFont(font);
  if (line_h)
    *line_h = lh;
  if (row_h)
    *row_h = rh;
  if (list_top)
    *list_top = lt;
}

void Platform_DrawListBox(PromptState *state, Display *dpy, XftDraw *xd,
                          Visual *vis, Colormap cmap) {

  if (!state || !state->lb)
    return;
  FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
  ListBoxInfo *lb = state->lb;
  if (!lb->item_count) {
    Platform_DestroyFont(font);
    return;
  }

  int itemH = x11_font_row_height_of(font);
  int y0 = x11_font_list_top_of(font);
  Platform_DrawPromptSeparator(
      (PlDrawPromptSeparatorSpec){.y = y0, .width = state->width});
  int yOffset = PROMPT_LIST_BOTTOM_TITLE;
  int visible = (state->height - y0 - yOffset) / itemH;
  if (visible < 1)
    visible = 1;
  if (visible > lb->item_count)
    visible = lb->item_count;
  UI_LB_SetVisibleCount(visible);

  ListBox_ApplyPendingScroll(lb, visible);

  ListBox_SetGeometry(y0, itemH, visible);
  ListBox_HoverFocus(lb, y0, itemH, lb->view_offset, visible);

  for (int i = 0; i < visible; i++) {
    int idx = lb->view_offset + i;
    int y = y0 + i * itemH;
    bool sel = (idx == lb->selected_index);
    if (RoundedCorners && CornerRadius > 0) {
      int R = PROMPT_SEL_RADIUS;
      int t = PROMPT_SEL_INSET;
      int bx = PROMPT_SEL_X, bw = state->width - PROMPT_SEL_W_SHRINK;
      if (sel) {
        x11_fill_rounded_rect(
            dpy, xd, vis, cmap, rgb_to_hex(COLOR_SELECTED_TEXT), 0xFFFF, bx + t,
            y + PROMPT_SEL_TOP_INSET + t, bw - 2 * t, itemH - 2 - 2 * t, R - 1,
            state->width, state->height);
      }
    } else {
      Platform_DrawListBackground(state, dpy, xd, vis, cmap, y, itemH, sel);
    }
    Platform_DrawListItem(state, dpy, xd, vis, cmap, font, lb, idx, y,
                          PROMPT_ITEM_TEXT_X, NULL);
  }

  if (PromptListBottomMargin() > 0) {
    Platform_DrawPromptSeparator((PlDrawPromptSeparatorSpec){
        .y = state->height - PROMPT_LIST_BOTTOM_TITLE, .width = state->width});
  }

  Platform_DestroyFont(font);
}
