#include "helpers/x11/raw_draw_x11.h"
#define RAW_LINE_CAP 4096

bool Platform_WindowToRoot(WindowHandle h, int *out_x, int *out_y) {
  Display *dpy = x11_get_display();
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

#define RAW_LINE_CAP 4096

void Platform_FlushDraw(void) { x11_raw_line_flush(); }

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
  DrawMeasureTrack(x, y, w, h);
  return x11_draw_image_scaled(g_draw_dpy, g_draw_xd, g_draw_vis, path, x, y, w,
                               h);
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
      fh = x11_create_font_on_display(g_draw_dpy, spec.fontsize, UsedFont);
    } else {
      fh = x11_create_font_on_display(g_draw_dpy, FontSize, UsedFont);
    }
    needDestroy = true;
  }
  unsigned long color =
      spec.color == PL_COLOR_AUTO ? RgbToHex(COLOR_TEXT) : spec.color;
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
    if (String_ContainsCI(a, "CENTER"))
      bx = x - tw / 2;
    else if (String_ContainsCI(a, "RIGHT"))
      bx = x - tw;
    DrawMeasureTrack(bx, y, tw, th);
  }
  XftColor col;
  x11_xft_alloc_color(g_draw_dpy, g_draw_vis, g_draw_cmap, color, 0xFFFF, &col);
  if (String_ContainsCI(a, "CENTER") || String_ContainsCI(a, "LEFT") ||
      String_ContainsCI(a, "RIGHT")) {
    int box = 0;
    if (spec.pad == 0 && !String_ContainsCI(a, "LEFT"))
      box = x;
    x11_draw_wrapped_text(g_draw_dpy, g_draw_xd, &col, fh, spec.text, y,
                          (char *)a, spec.pad ? spec.pad : x, box);
  } else {
    XftFont *xf = FH_TO_XFT(fh);
    int len = (int)strlen(spec.text);
    if (len == 1 && xf &&
        x11_raw_line_push(x, y, spec.text[0], color, fh, needDestroy,
                          spec.fontsize)) {
      needDestroy = false;
    } else {
      x11_raw_line_flush();
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
  DrawTextCenteredImpl(&spec, lineHeight);
}

int Platform_MeasureTextWidth(PlMeasureTextWidthSpec spec) {
  if (!spec.text || spec.len <= 0)
    return 0;
  if (!g_draw_dpy)
    return spec.len * (spec.fontsize > 0 ? spec.fontsize : FontSize) / 2;
  XftFont *xf = x11_line_font(g_draw_dpy, spec.fontsize);
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
  XftFont *xf = x11_line_font(g_draw_dpy, spec.fontsize);
  if (!xf)
    return false;
  unsigned long color =
      spec.color == PL_COLOR_AUTO ? RgbToHex(COLOR_TEXT) : spec.color;
  XftColor col;
  x11_xft_alloc_color(g_draw_dpy, g_draw_vis, g_draw_cmap, color, 0xFFFF, &col);
  x11_draw_utf8_fallback(g_draw_dpy, g_draw_xd, &col, x, y + xf->ascent, xf,
                         spec.text, spec.len,
                         spec.fontsize > 0 ? spec.fontsize : FontSize);
  XftColorFree(g_draw_dpy, g_draw_vis, g_draw_cmap, &col);
  return true;
}

void x11_draw_list_item(Display *dpy, XftDraw *xd,
                        Visual *vis, Colormap cmap, FontHandle font,
                        ListBoxInfo *lb, int idx, int y, int text_x,
                        const char *icon_path) {
  unsigned long ncol, bcol, pcol;
  Item_GetColors((PlGetItemColorsSpec){.idx = idx,
                                       .count = lb->item_count,
                                       .num_color = &ncol,
                                       .body_color = &bcol,
                                       .plain_color = &pcol});
  XftColor ncolor, bcolor, pcolor;
  x11_xft_alloc_color(dpy, vis, cmap, ncol, 0xFFFF, &ncolor);
  x11_xft_alloc_color(dpy, vis, cmap, bcol, 0xFFFF, &bcolor);
  x11_xft_alloc_color(dpy, vis, cmap, pcol, 0xFFFF, &pcolor);
  x11_draw_listbox_item_text(dpy, xd, vis, cmap, font, &ncolor, &bcolor,
                             &pcolor, lb, (char *)icon_path, idx, y, text_x);
  XftColorFree(dpy, vis, cmap, &ncolor);
  XftColorFree(dpy, vis, cmap, &bcolor);
  XftColorFree(dpy, vis, cmap, &pcolor);
}

void x11_draw_edit_background(PromptState *state, Display *dpy, XftDraw *xd,
                              Visual *vis, Colormap cmap, FontHandle font) {
  x11_set_draw_context(dpy, xd, vis, cmap);
  if (WindowTitleText)
    Platform_DrawTextRaw(
        (PlDrawTextRawSpec){.x = PROMPT_FOOTER_X,
                            .y = state->height - PROMPT_FOOTER_Y_FROM_BOTTOM,
                            .text = state->window_title,
                            .color = RgbToHex(COLOR_TEXT),
                            .font = font,
                            .align = "LEFT",
                            .pad = PROMPT_FOOTER_PAD,
                            .fontsize = FontSize});
}

static void draw_editbox_part(PromptState *state, Display *dpy, XftDraw *xd,
                              Visual *vis, Colormap cmap, FontHandle font,
                              int part) {
  x11_set_draw_context(dpy, xd, vis, cmap);
  EditboxX11Metrics metrics = {dpy, FH_TO_XFT(font)};
  EditboxSpec eb;
  EditboxLayout lay;
  x11_prompt_editbox_spec(state, font, &eb, &metrics);
  Editbox_Layout(&eb, &lay);
  if (part == EDITBOX_PART_TEXT)
    state->eb->scroll_offset = eb.scroll;
  Editbox_Draw(&eb, &lay, part);
}

void x11_draw_edit_text(PromptState *state, Display *dpy, XftDraw *xd,
                        Visual *vis, Colormap cmap, FontHandle font) {
  if (!state)
    return;
  draw_editbox_part(state, dpy, xd, vis, cmap, font, EDITBOX_PART_TEXT);
}

void x11_draw_edit_tint(PromptState *state, Display *dpy, XftDraw *xd,
                        Visual *vis, Colormap cmap, FontHandle font) {
  draw_editbox_part(state, dpy, xd, vis, cmap, font, EDITBOX_PART_TINT);
}

void x11_draw_edit_cursor(PromptState *state, Display *dpy, XftDraw *xd,
                          Visual *vis, Colormap cmap, FontHandle font) {
  draw_editbox_part(state, dpy, xd, vis, cmap, font, EDITBOX_PART_CARET);
}

bool Platform_DrawRectRaw(PlDrawRectRawSpec spec) {
  RECT_RAW_UNPACK(spec);

  if (!x11_raw_can_draw()) {
    return false;
  }

  if (w <= 0 || h <= 0) {
    return false;
  }

  x += PlDrawState_Get()->off_x;
  y += PlDrawState_Get()->off_y;

  int t = DrawThicknessPx(thickness);
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
    DrawMeasureTrack(x, y, w, h);
  } else {
    DrawMeasureTrack(x - t, y - t, w + 2 * t, h + 2 * t);
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
  GC gc = x11_raw_gc_for(g_draw_dpy, d);
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
          {(short)x, (short)(y + h - r * 2),
           (unsigned short)(r * 2), (unsigned short)(r * 2),
           180 * 64, 90 * 64}};
      XDrawArcs(g_draw_dpy, d, gc, arcs, 4);

      XSegment segments[4] = {{x + r, y, x + w - r, y},
                              {x + w, y + r, x + w, y + h - r},
                              {x + r, y + h, x + w - r, y + h},
                              {x, y + r, x, y + h - r}};
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
  if (!x11_raw_can_draw())
    return false;
  int x1, y1, x2, y2;
  int t = DrawLinePrep(&spec, PlDrawState_Get()->off_x,
                       PlDrawState_Get()->off_y, &x1, &y1, &x2, &y2);
  GC gc = x11_raw_prepare(spec.color, t);
  if (!gc)
    return false;
  Drawable d = XftDrawDrawable(g_draw_xd);
  XDrawLine(g_draw_dpy, d, gc, x1, y1, x2, y2);
  return true;
}

bool Platform_DrawCircleRaw(PlDrawCircleRawSpec spec) {
  if (!x11_raw_can_draw() || spec.radius <= 0)
    return false;
  int cx, cy;
  int t = DrawCirclePrep(&spec, PlDrawState_Get()->off_x,
                         PlDrawState_Get()->off_y, &cx, &cy);
  GC gc = x11_raw_prepare(spec.color, t);
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

void Platform_GetTextLayoutMetrics(int *line_h, int *row_h, int *list_top) {
  Display *dpy = x11_get_display();
  FontHandle font = x11_create_font_on_display(dpy, FontSize, UsedFont);
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
