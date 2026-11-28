#include "helpers/x11/bg_image_x11.h"
#include "helpers/x11/raw_draw_x11.h"
#include "helpers/x11/window_factory_x11.h"
#include "helpers/x11/drawing_x11.h"
#include "helpers/helpers_ui.h"
#include <math.h>
#include <pthread.h>
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
  x11_apply_shape(&(X11ApplyShape){.f_dpy = dpy, .f_win = w, .f_w = attrs.width, .f_h = attrs.height, .f_rounded = rounded, .f_radius = radius});
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

bool Platform_DrawImageAt(PlatformDrawImageAt *argspec) {
  if (!argspec->f_path || !argspec->f_path[0] || !g_draw_dpy || !g_draw_xd)
    return false;
  int _offx = draw_state_get()->off_x;
  int _offy = draw_state_get()->off_y;
  argspec->f_x += _offx;
  argspec->f_y += _offy;
  if (argspec->f_w <= 0)
    argspec->f_w = IconSize;
  if (argspec->f_h <= 0)
    argspec->f_h = IconSize;
  draw_measure_track(argspec->f_x, argspec->f_y, argspec->f_w, argspec->f_h);
  return x11_draw_image_scaled(&(X11DrawImageScaled){.f_dpy = g_draw_dpy, .f_xd = g_draw_xd, .f_vis = g_draw_vis, .f_path = argspec->f_path, .f_x = argspec->f_x, .f_y = argspec->f_y, .f_tw = argspec->f_w, .f_th = argspec->f_h});
}

bool Platform_GetImageSize(const char *path, int *w, int *h) {
  return x11_get_image_size(path, w, h);
}

static bool x11_draw_text_raw_inner(DrawTextRaw spec) {
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
      tw = x11_measure_utf8_fallback(&(X11MeasureUtf8Fallback){.f_dpy = g_draw_dpy, .f_xf = mf, .f_s = spec.text, .f_n = (int)strlen(spec.text), .f_px = spec.font_size > 0 ? spec.font_size : FontSize});
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
  x11_xft_color_cached(&(X11XftColorCached){.f_dpy = g_draw_dpy, .f_vis = g_draw_vis, .f_cmap = g_draw_cmap, .f_hex = color, .f_alpha = 0xFFFF, .f_out = &col});
  if (string_contains_ci(a, "CENTER") || string_contains_ci(a, "LEFT") ||
      string_contains_ci(a, "RIGHT")) {
    int box = 0;
    if (spec.pad == 0 && !string_contains_ci(a, "LEFT"))
      box = x;
    x11_draw_wrapped_text(&(X11DrawWrappedText){.f_dpy = g_draw_dpy, .f_xd = g_draw_xd, .f_color = &col, .f_font = fh, .f_text = spec.text, .f_start_y = y, .f_centered_to = (char *)a, .f_x_offset = spec.pad ? spec.pad : x, .f_box_width = box});
  } else {
    XftFont *xf = FH_TO_XFT(fh);
    int len = (int)strlen(spec.text);
    if (len == 1 && xf &&
        x11_raw_line_push(&(X11RawLinePush){.f_x = x, .f_y = y, .f_ch = spec.text[0], .f_color = color, .f_fh = fh, .f_owned = needDestroy, .f_font_size = spec.font_size})) {
      needDestroy = false;
    } else {
      x11_raw_line_flush();
      if (xf)
        x11_draw_utf8_fallback(&(X11DrawUtf8Fallback){.f_dpy = g_draw_dpy, .f_xd = g_draw_xd, .f_color = &col, .f_x = x, .f_y = y + (xf->ascent), .f_xf = xf, .f_s = spec.text, .f_n = len, .f_px = spec.font_size > 0 ? spec.font_size : FontSize});
    }
  }
  if (needDestroy)
    Platform_DestroyFont(fh);
  return true;
}

bool Platform_DrawTextRaw(DrawTextRaw spec) {
  const DrawCanvas *ds = draw_state_get();
  if (ds->clip_y1 <= ds->clip_y0 || !g_draw_xd)
    return x11_draw_text_raw_inner(spec);
  int h = ds->clip_y1 - ds->clip_y0;
  XRectangle clip = {0, (short)ds->clip_y0, 65535,
                     (unsigned short)(h > 65535 ? 65535 : h)};
  XftDrawSetClipRectangles(g_draw_xd, 0, 0, &clip, 1);
  bool ok = x11_draw_text_raw_inner(spec);
  XftDrawSetClip(g_draw_xd, None);
  return ok;
}

void Platform_DrawTextCentered(DrawTextCentered spec) {
  if (!spec.text || !g_draw_dpy || !g_draw_xd)
    return;
  FontHandle fh = spec.font;
  bool need_destroy = false;
  if (!FH_TO_XFT(fh)) {
    int px = spec.font_size > 0 ? spec.font_size : FontSize;
    fh = x11_create_font_on_display(g_draw_dpy, px, UsedFont);
    if (!FH_TO_XFT(fh))
      return;
    need_destroy = true;
    spec.font = fh;
  }
  XftFont *xf = FH_TO_XFT(fh);
  int ascent = xf ? xf->ascent : (spec.font_size * 3 / 4);
  int descent = xf ? xf->descent : (spec.font_size / 4);
  int line_height = ascent + descent;
  draw_text_centered_impl(&spec, line_height);
  if (need_destroy)
    Platform_DestroyFont(fh);
}

int Platform_MeasureTextWidth(MeasureTextWidth spec) {
  if (!spec.text || spec.len <= 0)
    return 0;
  if (!g_draw_dpy)
    return spec.len * (spec.font_size > 0 ? spec.font_size : FontSize) / 2;
  XftFont *xf = x11_line_font(g_draw_dpy, spec.font_size);
  if (!xf)
    return spec.len * (spec.font_size > 0 ? spec.font_size : FontSize) / 2;
  return x11_measure_utf8_fallback(&(X11MeasureUtf8Fallback){.f_dpy = g_draw_dpy, .f_xf = xf, .f_s = spec.text, .f_n = spec.len, .f_px = spec.font_size > 0 ? spec.font_size
                                                      : FontSize});
}

bool Platform_DrawTextLine(DrawTextLine spec) {
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
  x11_xft_color_cached(&(X11XftColorCached){.f_dpy = g_draw_dpy, .f_vis = g_draw_vis, .f_cmap = g_draw_cmap, .f_hex = color, .f_alpha = 0xFFFF, .f_out = &col});
  x11_draw_utf8_fallback(&(X11DrawUtf8Fallback){.f_dpy = g_draw_dpy, .f_xd = g_draw_xd, .f_color = &col, .f_x = x, .f_y = y + xf->ascent, .f_xf = xf, .f_s = spec.text, .f_n = spec.len, .f_px = spec.font_size > 0 ? spec.font_size : FontSize});
  return true;
}

void x11_draw_list_item(X11DrawListItem *argspec) {
  XftColor pcolor;
  x11_xft_color_cached(&(X11XftColorCached){.f_dpy = argspec->f_dpy, .f_vis = argspec->f_vis, .f_cmap = argspec->f_cmap, .f_hex = rgb_to_hex(COLOR_TEXT), .f_alpha = 0xFFFF, .f_out = &pcolor});
  x11_draw_listbox_item_text(&(X11DrawListboxItemText){.f_dpy = argspec->f_dpy, .f_xd = argspec->f_xd, .f_vis = argspec->f_vis, .f_cmap = argspec->f_cmap, .f_font = argspec->f_font, .f_plain_color = &pcolor, .f_lb = argspec->f_lb, .f_icon_path = (char *)argspec->f_icon_path, .f_idx = argspec->f_idx, .f_y = argspec->f_y});
}

void x11_draw_edit_background(X11DrawEditBackground *argspec) {
  x11_set_draw_context(argspec->f_dpy, argspec->f_xd, argspec->f_vis, argspec->f_cmap);
  if (WindowTitleText)
    Platform_DrawTextRaw(
        (DrawTextRaw){.x = PROMPT_FOOTER_X,
                          .y = argspec->f_state->height - PROMPT_FOOTER_Y_FROM_BOTTOM,
                          .text = argspec->f_state->window_title,
                          .color = rgb_to_hex(COLOR_TEXT),
                          .font = argspec->f_font,
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

void x11_draw_edit_text(X11DrawEditText *argspec) {
  if (!argspec->f_state)
    return;
  draw_editbox_part(argspec->f_state, argspec->f_dpy, argspec->f_xd, argspec->f_vis, argspec->f_cmap, argspec->f_font, EDITBOX_PART_TEXT);
}

void x11_draw_edit_tint(X11DrawEditTint *argspec) {
  draw_editbox_part(argspec->f_state, argspec->f_dpy, argspec->f_xd, argspec->f_vis, argspec->f_cmap, argspec->f_font, EDITBOX_PART_TINT);
}

void x11_draw_edit_cursor(X11DrawEditCursor *argspec) {
  draw_editbox_part(argspec->f_state, argspec->f_dpy, argspec->f_xd, argspec->f_vis, argspec->f_cmap, argspec->f_font, EDITBOX_PART_CARET);
}

static bool blit_argb_buf(unsigned char *buf, int w, int h, int bx, int by,
                          bool owned) {
  if (!buf || w <= 0 || h <= 0)
    return false;
  Pixmap pm = 0;
  Picture pic = 0;
  bool ok = x11_upload_argb(&(X11UploadArgb){.f_dpy = g_draw_dpy, .f_ref = 0, .f_argb = buf, .f_w = w, .f_h = h, .f_out_pm = &pm, .f_out_pic = &pic});
  if (owned)
    free(buf);
  if (!ok)
    return false;
  XRenderPictFormat *dfmt = XRenderFindVisualFormat(g_draw_dpy, g_draw_vis);
  Picture dst =
      dfmt ? XRenderCreatePicture(g_draw_dpy, XftDrawDrawable(g_draw_xd), dfmt,
                                  0, NULL)
           : 0;
  bool result = false;
  if (dst) {
    const DrawCanvas *ds = draw_state_get();
    if (ds->clip_y1 > ds->clip_y0) {
      XRectangle clip = {0, (short)ds->clip_y0, 65535,
                         (unsigned short)(ds->clip_y1 - ds->clip_y0)};
      XRenderSetPictureClipRectangles(g_draw_dpy, dst, 0, 0, &clip, 1);
    }
    XRenderComposite(g_draw_dpy, PictOpOver, pic, None, dst, 0, 0, 0, 0, bx, by,
                     (unsigned int)w, (unsigned int)h);
    result = true;
    XRenderFreePicture(g_draw_dpy, dst);
  }
  XRenderFreePicture(g_draw_dpy, pic);
  XFreePixmap(g_draw_dpy, pm);
  return result;
}

static bool blit_argb_buf_scaled(const unsigned char *buf, int w, int h, int bx,
                                 int by, int dw, int dh) {
  if (!buf || w <= 0 || h <= 0 || dw <= 0 || dh <= 0 || !g_draw_dpy ||
      !g_draw_xd)
    return false;
  Pixmap pm = 0;
  Picture pic = 0;
  if (!x11_upload_argb(&(X11UploadArgb){.f_dpy = g_draw_dpy, .f_ref = 0, .f_argb = (unsigned char *)buf, .f_w = w, .f_h = h, .f_out_pm = &pm, .f_out_pic = &pic}))
    return false;
  XRenderPictFormat *dfmt = XRenderFindVisualFormat(g_draw_dpy, g_draw_vis);
  Picture dst =
      dfmt ? XRenderCreatePicture(g_draw_dpy, XftDrawDrawable(g_draw_xd), dfmt,
                                  0, NULL)
           : 0;
  bool result = false;
  if (dst) {
    const DrawCanvas *ds = draw_state_get();
    if (ds->clip_y1 > ds->clip_y0) {
      XRectangle clip = {0, (short)ds->clip_y0, 65535,
                         (unsigned short)(ds->clip_y1 - ds->clip_y0)};
      XRenderSetPictureClipRectangles(g_draw_dpy, dst, 0, 0, &clip, 1);
    }
    if (w != dw || h != dh) {
      XTransform transform = {{
          {XDoubleToFixed((double)w / (double)dw), 0, 0},
          {0, XDoubleToFixed((double)h / (double)dh), 0},
          {0, 0, XDoubleToFixed(1)},
      }};
      XRenderSetPictureTransform(g_draw_dpy, pic, &transform);
      XRenderSetPictureFilter(g_draw_dpy, pic, FilterGood, NULL, 0);
    }
    XRenderComposite(g_draw_dpy, PictOpOver, pic, None, dst, 0, 0, 0, 0, bx, by,
                     (unsigned int)dw, (unsigned int)dh);
    result = true;
    XRenderFreePicture(g_draw_dpy, dst);
  }
  XRenderFreePicture(g_draw_dpy, pic);
  XFreePixmap(g_draw_dpy, pm);
  return result;
}

bool Platform_DrawImageBuffer(PlatformDrawImageBuffer *argspec) {
  if (!argspec->f_image || !argspec->f_image->pixels || argspec->f_image->w <= 0 || argspec->f_image->h <= 0)
    return false;
  if (!g_draw_dpy || !g_draw_xd)
    return false;
  argspec->f_x += draw_state_get()->off_x;
  argspec->f_y += draw_state_get()->off_y;
  if (argspec->f_w <= 0)
    argspec->f_w = argspec->f_image->w;
  if (argspec->f_h <= 0)
    argspec->f_h = argspec->f_image->h;
  draw_measure_track(argspec->f_x, argspec->f_y, argspec->f_w, argspec->f_h);
  return blit_argb_buf_scaled(argspec->f_image->pixels, argspec->f_image->w, argspec->f_image->h, argspec->f_x, argspec->f_y, argspec->f_w, argspec->f_h);
}

static unsigned char *make_rect_border_argb(int rect_w, int rect_h,
                                            float radius, float thickness,
                                            unsigned long color, float alpha,
                                            int *out_w, int *out_h) {
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

#define RECT_BORDER_CACHE_CAP 128

typedef struct {
  int w, h;
  int bw, bh;
  float radius, thickness, alpha;
  unsigned long color;
  unsigned char *buf;
  unsigned long used;
} RectBorderCacheEntry;

static RectBorderCacheEntry g_rect_border_cache[RECT_BORDER_CACHE_CAP];
static unsigned long g_rect_border_cache_tick = 0;
static pthread_mutex_t g_rect_border_cache_lock = PTHREAD_MUTEX_INITIALIZER;

static bool rect_border_cache_key_eq(const RectBorderCacheEntry *e, int w,
                                     int h, float radius, float thickness,
                                     unsigned long color, float alpha) {
  return e->w == w && e->h == h && e->radius == radius &&
         e->thickness == thickness && e->color == color && e->alpha == alpha;
}

static unsigned char *rect_border_argb(int rect_w, int rect_h, float radius,
                                       float thickness, unsigned long color,
                                       float alpha, int *out_w, int *out_h,
                                       bool *out_owned) {
  if (radius > (float)rect_w * 0.5f)
    radius = (float)rect_w * 0.5f;
  if (radius > (float)rect_h * 0.5f)
    radius = (float)rect_h * 0.5f;

  int pad = (int)thickness / 2 + 2;
  int w = rect_w + 2 * pad, h = rect_h + 2 * pad;
  bool cacheable = w > 0 && h > 0 && w <= 1024 && h <= 1024;
  if (!cacheable) {
    *out_owned = true;
    return make_rect_border_argb(rect_w, rect_h, radius, thickness, color,
                                 alpha, out_w, out_h);
  }

  pthread_mutex_lock(&g_rect_border_cache_lock);
  unsigned long stamp = ++g_rect_border_cache_tick;
  for (int i = 0; i < RECT_BORDER_CACHE_CAP; i++) {
    RectBorderCacheEntry *e = &g_rect_border_cache[i];
    if (e->buf &&
        rect_border_cache_key_eq(e, w, h, radius, thickness, color, alpha)) {
      e->used = stamp;
      if (out_w)
        *out_w = e->bw;
      if (out_h)
        *out_h = e->bh;
      *out_owned = false;
      pthread_mutex_unlock(&g_rect_border_cache_lock);
      return e->buf;
    }
  }

  int victim = 0;
  for (int i = 1; i < RECT_BORDER_CACHE_CAP; i++) {
    RectBorderCacheEntry *a = &g_rect_border_cache[i];
    RectBorderCacheEntry *b = &g_rect_border_cache[victim];
    if (!a->buf || (!b->buf || a->used < b->used))
      victim = i;
    if (!a->buf)
      break;
  }

  unsigned char *buf = make_rect_border_argb(rect_w, rect_h, radius, thickness,
                                             color, alpha, out_w, out_h);
  if (!buf) {
    pthread_mutex_unlock(&g_rect_border_cache_lock);
    return NULL;
  }

  RectBorderCacheEntry *dst = &g_rect_border_cache[victim];
  free(dst->buf);
  dst->w = w;
  dst->h = h;
  dst->bw = out_w ? *out_w : w;
  dst->bh = out_h ? *out_h : h;
  dst->radius = radius;
  dst->thickness = thickness;
  dst->color = color;
  dst->alpha = alpha;
  dst->buf = buf;
  dst->used = stamp;
  *out_owned = false;
  pthread_mutex_unlock(&g_rect_border_cache_lock);
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
      x11_fill_rounded_rect(&(X11FillRoundedRect){.f_dpy = g_draw_dpy, .f_xd = g_draw_xd, .f_vis = g_draw_vis, .f_cmap = g_draw_cmap, .f_hex = color, .f_alpha16 = target_alpha, .f_x = x, .f_y = y, .f_w = w, .f_h = h, .f_radius = r});
    } else {
      XftColor col;
      x11_xft_color_cached(&(X11XftColorCached){.f_dpy = g_draw_dpy, .f_vis = g_draw_vis, .f_cmap = g_draw_cmap, .f_hex = color, .f_alpha = target_alpha, .f_out = &col});
      const DrawCanvas *ds = draw_state_get();
      bool clipped = ds->clip_y1 > ds->clip_y0;
      if (clipped) {
        int clip_h = ds->clip_y1 - ds->clip_y0;
        XRectangle clip = {0, (short)ds->clip_y0, 65535,
                           (unsigned short)(clip_h > 65535 ? 65535 : clip_h)};
        XftDrawSetClipRectangles(g_draw_xd, 0, 0, &clip, 1);
      }
      XftDrawRect(g_draw_xd, &col, x, y, w, h);
      if (clipped)
        XftDrawSetClip(g_draw_xd, None);
    }
    return true;
  }

  {
    float r2 = (float)r;
    float alpha = (float)target_alpha / 65535.0f;
    int bw = 0, bh = 0;
    bool owned = false;
    unsigned char *buf =
        rect_border_argb(w, h, r2, (float)t, color, alpha, &bw, &bh, &owned);
    if (!buf)
      return false;
    return blit_argb_buf(buf, bw, bh, (int)x - (bw - (int)w) / 2,
                         (int)y - (bh - (int)h) / 2, owned);
  }
}

static unsigned char *line_argb_buffer(const int x1, const int y1, const int x2,
                                       const int y2, const int w, const int h,
                                       const unsigned long color,
                                       const float thickness,
                                       const float alpha) {
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

bool Platform_DrawLineRaw(DrawLineRaw spec) {
  if (!x11_raw_can_draw())
    return false;
  int x1, y1, x2, y2;
  int t = draw_line_prep(&(DrawLinePrep){.f_spec = &spec, .f_off_x = draw_state_get()->off_x, .f_off_y = draw_state_get()->off_y, .f_x1 = &x1, .f_y1 = &y1, .f_x2 = &x2, .f_y2 = &y2});
  int thickness = t > 0 ? t : 1;
  int pad = (int)((float)thickness * 0.5f) + 2;
  int bx = (x1 < x2 ? x1 : x2) - pad;
  int by = (y1 < y2 ? y1 : y2) - pad;
  int w = abs(x2 - x1) + thickness + 2 * pad;
  int h = abs(y2 - y1) + thickness + 2 * pad;
  float alpha = 1.0f;
  unsigned char *buf = line_argb_buffer(x1 - bx, y1 - by, x2 - bx, y2 - by, w,
                                        h, spec.color, (float)thickness, alpha);
  return blit_argb_buf(buf, w, h, bx, by, true);
}

bool Platform_DrawCircleRaw(DrawCircleRaw spec) {
  if (!x11_raw_can_draw() || spec.radius <= 0)
    return false;
  int cx, cy;
  int t = draw_circle_prep(&(DrawCirclePrep){.f_spec = &spec, .f_off_x = draw_state_get()->off_x, .f_off_y = draw_state_get()->off_y, .f_cx = &cx, .f_cy = &cy});
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


