#include "helpers/x11/raw_draw_x11.h"
#include "helpers/x11/window_factory_x11.h"
#include "helpers/x11/drawing_x11.h"
#include "helpers/helpers_ui.h"
#include <math.h>
#define RAW_LINE_CAP 4096

bool Platform_SetWindowRounded(WindowHandle win, bool rounded, int radius) {
  Display *dpy = x11_get_display();
  if (!dpy)
    return false;
  Window w = (Window)WH_TO_XID(win);
  if (w == None)
    return false;
  XWindowAttributes attrs;
  if (!XGetWindowAttributes(dpy, w, &attrs))
    return false;
  x11_apply_shape(dpy, w, attrs.width, attrs.height, rounded, radius);
  XFlush(dpy);
  return true;
}

bool Platform_WindowToRoot(WindowHandle h, int *out_x, int *out_y) {
  Display *dpy = x11_get_display();
  if (!dpy)
    return false;
  Window w = (Window)WH_TO_XID(h);
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
  int _offx = draw_state_get()->off_x;
  int _offy = draw_state_get()->off_y;
  x += _offx;
  y += _offy;
  if (w <= 0)
    w = IconSize;
  if (h <= 0)
    h = IconSize;
  draw_measure_track(x, y, w, h);
  return x11_draw_image_scaled(g_draw_dpy, g_draw_xd, g_draw_vis, path, x, y, w,
                               h);
}

bool Platform_DrawTextRaw(DrawTextRawSpec spec) {
  if (!spec.text || !g_draw_dpy || !g_draw_xd)
    return false;
  if (spec.angle != 0.0f)
    return x11_raw_draw_text_rotated(&spec);
  int _ox = draw_state_get()->off_x;
  int _oy = draw_state_get()->off_y;
  int x = spec.x + _ox;
  int y = spec.y + _oy;
  FontHandle fh = spec.font;
  bool needDestroy = false;
  if (!FH_TO_XFT(fh)) {
    if (spec.font_size) {
      fh = x11_create_font_on_display(g_draw_dpy, spec.font_size, UsedFont);
    } else {
      fh = x11_create_font_on_display(g_draw_dpy, FontSize, UsedFont);
    }
    needDestroy = true;
  }
  unsigned long color =
      spec.color == COLOR_AUTO ? rgb_to_hex(COLOR_TEXT) : spec.color;
  const char *a = spec.align ? spec.align : "CENTER";
  if (draw_state_get()->measuring) {
    XftFont *mf = FH_TO_XFT(fh);
    int tw = 0;
    if (mf) {
      tw = x11_measure_utf8_fallback(
          g_draw_dpy, mf, spec.text, (int)strlen(spec.text),
          spec.font_size > 0 ? spec.font_size : FontSize);
    }
    int th = mf ? (mf->ascent + mf->descent)
                : (spec.font_size > 0 ? spec.font_size : FontSize);
    int bx = x;
    if (string_contains_ci(a, "CENTER"))
      bx = x - tw / 2;
    else if (string_contains_ci(a, "RIGHT"))
      bx = x - tw;
    draw_measure_track(bx, y, tw, th);
  }
  XftColor col;
  x11_xft_color_cached(g_draw_dpy, g_draw_vis, g_draw_cmap, color, 0xFFFF,
                       &col);
  if (string_contains_ci(a, "CENTER") || string_contains_ci(a, "LEFT") ||
      string_contains_ci(a, "RIGHT")) {
    int box = 0;
    if (spec.pad == 0 && !string_contains_ci(a, "LEFT"))
      box = x;
    x11_draw_wrapped_text(g_draw_dpy, g_draw_xd, &col, fh, spec.text, y,
                          (char *)a, spec.pad ? spec.pad : x, box);
  } else {
    XftFont *xf = FH_TO_XFT(fh);
    int len = (int)strlen(spec.text);
    if (len == 1 && xf &&
        x11_raw_line_push(x, y, spec.text[0], color, fh, needDestroy,
                          spec.font_size)) {
      needDestroy = false;
    } else {
      x11_raw_line_flush();
      if (xf)
        x11_draw_utf8_fallback(g_draw_dpy, g_draw_xd, &col, x, y + (xf->ascent),
                               xf, spec.text, len,
                               spec.font_size > 0 ? spec.font_size : FontSize);
    }
  }
  if (needDestroy)
    Platform_DestroyFont(fh);
  return true;
}

void Platform_DrawTextCentered(DrawTextCenteredSpec spec) {
  if (!spec.text || !g_draw_dpy || !g_draw_xd)
    return;
  XftFont *xf = FH_TO_XFT(spec.font);
  int ascent = xf ? xf->ascent : (spec.font_size * 3 / 4);
  int descent = xf ? xf->descent : (spec.font_size / 4);
  int line_height = ascent + descent;
  draw_text_centered_impl(&spec, line_height);
}

int Platform_MeasureTextWidth(MeasureTextWidthSpec spec) {
  if (!spec.text || spec.len <= 0)
    return 0;
  if (!g_draw_dpy)
    return spec.len * (spec.font_size > 0 ? spec.font_size : FontSize) / 2;
  XftFont *xf = x11_line_font(g_draw_dpy, spec.font_size);
  if (!xf)
    return spec.len * (spec.font_size > 0 ? spec.font_size : FontSize) / 2;
  return x11_measure_utf8_fallback(g_draw_dpy, xf, spec.text, spec.len,
                                   spec.font_size > 0 ? spec.font_size
                                                     : FontSize);
}

bool Platform_DrawTextLine(DrawTextLineSpec spec) {
  if (!spec.text || spec.len <= 0 || !g_draw_dpy || !g_draw_xd)
    return false;
  int x = spec.x + draw_state_get()->off_x;
  int y = spec.y + draw_state_get()->off_y;
  XftFont *xf = x11_line_font(g_draw_dpy, spec.font_size);
  if (!xf)
    return false;
  unsigned long color =
      spec.color == COLOR_AUTO ? rgb_to_hex(COLOR_TEXT) : spec.color;
  XftColor col;
  x11_xft_color_cached(g_draw_dpy, g_draw_vis, g_draw_cmap, color, 0xFFFF,
                       &col);
  x11_draw_utf8_fallback(g_draw_dpy, g_draw_xd, &col, x, y + xf->ascent, xf,
                         spec.text, spec.len,
                         spec.font_size > 0 ? spec.font_size : FontSize);
  return true;
}

void x11_draw_list_item(Display *dpy, XftDraw *xd, Visual *vis, Colormap cmap,
                        FontHandle font, ListBoxInfo *lb, int idx, int y,
                        int text_x, const char *icon_path) {
  unsigned long ncol, bcol, pcol;
  item_get_colors((GetItemColorsSpec){.idx = idx,
                                      .count = lb->item_count,
                                      .num_color = &ncol,
                                      .body_color = &bcol,
                                      .plain_color = &pcol});
  XftColor ncolor, bcolor, pcolor;
  x11_xft_color_cached(dpy, vis, cmap, ncol, 0xFFFF, &ncolor);
  x11_xft_color_cached(dpy, vis, cmap, bcol, 0xFFFF, &bcolor);
  x11_xft_color_cached(dpy, vis, cmap, pcol, 0xFFFF, &pcolor);
  x11_draw_listbox_item_text(dpy, xd, vis, cmap, font, &pcolor, lb,
                             (char *)icon_path, idx, y);
}

void x11_draw_edit_background(const PromptState *state, Display *dpy,
                              XftDraw *xd, Visual *vis, Colormap cmap,
                              FontHandle font) {
  x11_set_draw_context(dpy, xd, vis, cmap);
  if (WindowTitleText)
    Platform_DrawTextRaw(
        (DrawTextRawSpec){.x = PROMPT_FOOTER_X,
                          .y = state->height - PROMPT_FOOTER_Y_FROM_BOTTOM,
                          .text = state->window_title,
                          .color = rgb_to_hex(COLOR_TEXT),
                          .font = font,
                          .align = "LEFT",
                          .pad = PROMPT_FOOTER_PAD,
                          .font_size = FontSize});
}

static void draw_editbox_part(PromptState *state, Display *dpy, XftDraw *xd,
                              Visual *vis, Colormap cmap, FontHandle font,
                              int part) {
  x11_set_draw_context(dpy, xd, vis, cmap);
  EditboxX11Metrics metrics = {dpy, FH_TO_XFT(font)};
  EditboxSpec eb;
  EditboxLayout lay;
  x11_prompt_editbox_spec(state, font, &eb, &metrics);
  editbox_layout(&eb, &lay);
  if (part == EDITBOX_PART_TEXT)
    state->eb->scroll_offset = eb.scroll;
  editbox_draw(&eb, &lay, part);
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

static bool blit_argb_buf(unsigned char *buf, int w, int h, int bx, int by) {
  if (!buf || w <= 0 || h <= 0)
    return false;
  Pixmap pm = 0;
  Picture pic = 0;
  bool ok = x11_upload_argb(g_draw_dpy, 0, buf, w, h, &pm, &pic);
  free(buf);
  if (!ok)
    return false;
  XRenderPictFormat *dfmt = XRenderFindVisualFormat(g_draw_dpy, g_draw_vis);
  Picture dst = dfmt ? XRenderCreatePicture(g_draw_dpy,
                                            XftDrawDrawable(g_draw_xd),
                                            dfmt, 0, NULL)
                     : 0;
  bool result = false;
  if (dst) {
    XRenderComposite(g_draw_dpy, PictOpOver, pic, None, dst, 0, 0, 0, 0, bx, by,
                     (unsigned int)w, (unsigned int)h);
    result = true;
    XRenderFreePicture(g_draw_dpy, dst);
  }
  XRenderFreePicture(g_draw_dpy, pic);
  XFreePixmap(g_draw_dpy, pm);
  return result;
}

static unsigned char *rect_border_argb(int rect_w, int rect_h, float radius,
                                       float thickness, unsigned long color,
                                       float alpha, int *out_w, int *out_h) {
  int pad = (int)thickness / 2 + 2;
  int w = rect_w + 2 * pad, h = rect_h + 2 * pad;
  if (radius > (float)rect_w * 0.5f)
    radius = (float)rect_w * 0.5f;
  if (radius > (float)rect_h * 0.5f)
    radius = (float)rect_h * 0.5f;
  unsigned char *buf = calloc((size_t)w * (size_t)h * 4, 1);
  if (!buf)
    return NULL;
  float cr = (float)((color >> 16) & 0xFF);
  float cg = (float)((color >> 8) & 0xFF);
  float cb = (float)(color & 0xFF);
  float rx = (float)rect_w * 0.5f - radius;
  float ry = (float)rect_h * 0.5f - radius;
  for (int py = 0; py < h; py++) {
    float pyc = (float)py + 0.5f - (float)h * 0.5f;
    for (int px = 0; px < w; px++) {
      float pxc = (float)px + 0.5f - (float)w * 0.5f;
      float ax = fabsf(pxc) - rx;
      float ay = fabsf(pyc) - ry;
      float qx = fmaxf(ax, 0.0f), qy = fmaxf(ay, 0.0f);
      float sd = sqrtf(qx * qx + qy * qy) + fminf(fmaxf(ax, ay), 0.0f) - radius;
      if (sd > 0.0f)
        continue;
      float cov = thickness + sd;
      if (cov <= 0.0f)
        continue;
      if (cov > 1.0f)
        cov = 1.0f;
      float a8 = alpha * cov;
      unsigned char A = (unsigned char)(a8 * 255.0f + 0.5f);
      if (A == 0)
        continue;
      size_t o = ((size_t)py * (size_t)w + (size_t)px) * 4;
      buf[o + 0] = (unsigned char)(cb * a8 + 0.5f);
      buf[o + 1] = (unsigned char)(cg * a8 + 0.5f);
      buf[o + 2] = (unsigned char)(cr * a8 + 0.5f);
      buf[o + 3] = A;
    }
  }
  if (out_w)
    *out_w = w;
  if (out_h)
    *out_h = h;
  return buf;
}

bool Platform_DrawRectRaw(DrawRectRawSpec spec) {
  RECT_RAW_UNPACK(spec);

  if (!x11_raw_can_draw()) {
    return false;
  }

  if (w <= 0 || h <= 0) {
    return false;
  }

  if (angle != 0.0f)
    return x11_raw_draw_rect_rotated(&spec);

  x += draw_state_get()->off_x;
  y += draw_state_get()->off_y;

  int t = draw_thickness_px(thickness);
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
    draw_measure_track(x, y, w, h);
  } else {
    draw_measure_track(x - t, y - t, w + 2 * t, h + 2 * t);
  }
  unsigned short target_alpha = opaque ? 0xFFFFu : opacity_alpha16();

  if (filled) {
    if (want_round && r > 0) {
      x11_fill_rounded_rect(g_draw_dpy, g_draw_xd, g_draw_vis, g_draw_cmap,
                            color, target_alpha, x, y, w, h, r);
    } else {
      XftColor col;
      x11_xft_color_cached(g_draw_dpy, g_draw_vis, g_draw_cmap, color,
                           target_alpha, &col);
      XftDrawRect(g_draw_xd, &col, x, y, w, h);
    }
    return true;
  }

  {
    float r2 = (float)r;
    float alpha = (float)target_alpha / 65535.0f;
    int bw = 0, bh = 0;
    unsigned char *buf = rect_border_argb(w, h, r2, (float)t, color, alpha,
                                          &bw, &bh);
    if (!buf)
      return false;
    return blit_argb_buf(buf, bw, bh, (int)x - (bw - (int)w) / 2,
                         (int)y - (bh - (int)h) / 2);
  }
  return true;
}

static unsigned char *line_argb_buffer(const int x1, const int y1,
                                       const int x2, const int y2,
                                       const int w, const int h,
                                       const unsigned long color,
                                       const float thickness, const float alpha) {
  if (x1 == x2 || y1 == y2) {
    unsigned char *buf = calloc((size_t)w * (size_t)h * 4, 1);
    if (!buf)
      return NULL;
    float cr = (float)((color >> 16) & 0xFF);
    float cg = (float)((color >> 8) & 0xFF);
    float cb = (float)(color & 0xFF);
    for (int py = 0; py < h; py++) {
      for (int px = 0; px < w; px++) {
        if (x1 == x2) {
          int d = px - x1;
          int ad = d < 0 ? -d : d;
          int half = (int)thickness / 2;
          if (ad > half)
            continue;
        } else {
          int d = py - y1;
          int ad = d < 0 ? -d : d;
          int half = (int)thickness / 2;
          if (ad > half)
            continue;
        }
        float a8 = alpha;
        size_t o = ((size_t)py * (size_t)w + (size_t)px) * 4;
        buf[o + 0] = (unsigned char)(cb * a8 + 0.5f);
        buf[o + 1] = (unsigned char)(cg * a8 + 0.5f);
        buf[o + 2] = (unsigned char)(cr * a8 + 0.5f);
        buf[o + 3] = (unsigned char)(a8 * 255.0f + 0.5f);
      }
    }
    return buf;
  }
  unsigned char *buf = calloc((size_t)w * (size_t)h * 4, 1);
  if (!buf)
    return NULL;
  float fx1 = (float)x1, fy1 = (float)y1;
  float fx2 = (float)x2, fy2 = (float)y2;
  float dx = fx2 - fx1, dy = fy2 - fy1;
  float len2 = dx * dx + dy * dy;
  float cr = (float)((color >> 16) & 0xFF);
  float cg = (float)((color >> 8) & 0xFF);
  float cb = (float)(color & 0xFF);
  float radius = thickness * 0.5f;
  for (int py = 0; py < h; py++) {
    for (int px = 0; px < w; px++) {
      float pxc = (float)px + 0.5f - fx1;
      float pyc = (float)py + 0.5f - fy1;
      float d;
      if (len2 < 1e-6f) {
        d = sqrtf(pxc * pxc + pyc * pyc);
      } else {
        float tt = (pxc * dx + pyc * dy) / len2;
        if (tt < 0.0f)
          tt = 0.0f;
        else if (tt > 1.0f)
          tt = 1.0f;
        float qx = pxc - tt * dx, qy = pyc - tt * dy;
        d = sqrtf(qx * qx + qy * qy);
      }
      float cov = radius - d + 0.5f;
      if (cov <= 0.0f)
        continue;
      if (cov > 1.0f)
        cov = 1.0f;
      float a8 = alpha * cov;
      unsigned char A = (unsigned char)(a8 * 255.0f + 0.5f);
      if (A == 0)
        continue;
      size_t o = ((size_t)py * (size_t)w + (size_t)px) * 4;
      buf[o + 0] = (unsigned char)(cb * a8 + 0.5f);
      buf[o + 1] = (unsigned char)(cg * a8 + 0.5f);
      buf[o + 2] = (unsigned char)(cr * a8 + 0.5f);
      buf[o + 3] = A;
    }
  }
  return buf;
}

bool Platform_DrawLineRaw(DrawLineRawSpec spec) {
  if (!x11_raw_can_draw())
    return false;
  int x1, y1, x2, y2;
  int t = draw_line_prep(&spec, draw_state_get()->off_x,
                         draw_state_get()->off_y, &x1, &y1, &x2, &y2);
  int thickness = t > 0 ? t : 1;
  int pad = (int)((float)thickness * 0.5f) + 2;
  int bx = (x1 < x2 ? x1 : x2) - pad;
  int by = (y1 < y2 ? y1 : y2) - pad;
  int w = abs(x2 - x1) + thickness + 2 * pad;
  int h = abs(y2 - y1) + thickness + 2 * pad;
  float alpha = 1.0f;
  unsigned char *buf = line_argb_buffer(x1 - bx, y1 - by, x2 - bx, y2 - by, w,
                                        h, spec.color, (float)thickness, alpha);
  return blit_argb_buf(buf, w, h, bx, by);
}

bool Platform_DrawCircleRaw(DrawCircleRawSpec spec) {
  if (!x11_raw_can_draw() || spec.radius <= 0)
    return false;
  int cx, cy;
  int t = draw_circle_prep(&spec, draw_state_get()->off_x,
                           draw_state_get()->off_y, &cx, &cy);
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
  int rh = prompt_row_height();
  int lt = prompt_list_top();
  Platform_DestroyFont(font);
  if (line_h)
    *line_h = lh;
  if (row_h)
    *row_h = rh;
  if (list_top)
    *list_top = lt;
}
