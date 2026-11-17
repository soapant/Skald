#include "helpers/win32/raw_draw_win32.h"
#include "helpers/win32/common_ui_win32.h"

bool Platform_SetWindowRounded(WindowHandle win, bool rounded, int radius) {
  HWND hwnd = WH_TO_HWND(win);
  if (!hwnd || !IsWindow(hwnd))
    return false;
  RECT rc;
  GetWindowRect(hwnd, &rc);
  int w = rc.right - rc.left;
  int h = rc.bottom - rc.top;
  if (w <= 0 || h <= 0)
    return false;
  win32_apply_rounded(hwnd, w, h, rounded, radius);
  return true;
}

bool Platform_PrewarmImage(char *path) {
  if (!path || !path[0])
    return false;
  HDC screen = win32_screen_dc();
  if (!screen)
    return false;
  void *bmp = win32_img_cache_get(path, screen, FontSize, FontSize);
  if (bmp == WIN32_IMG_FAILED)
    return false;
  if (!bmp) {
    bmp = win32_load_scaled_bitmap(path, FontSize, FontSize);
    win32_img_cache_put(path, screen, FontSize, FontSize, bmp);
  }
  return bmp != NULL;
}

static void win32_draw_text_prep(int sx, int sy, unsigned long c, int *x_out,
                                 int *y_out, unsigned long *color_out) {
  *x_out = sx;
  *y_out = sy;
  pl_draw_state_offset(x_out, y_out);
  *color_out = c != COLOR_AUTO ? rgb_to_hex(c) : COLOR_TEXT;
}

bool Platform_WindowToRoot(WindowHandle h, int *out_x, int *out_y) {
  HWND w = WH_TO_HWND(h);
  if (!w || !IsWindow(w))
    return false;
  POINT pt;
  pt.x = 0;
  pt.y = 0;
  if (!ClientToScreen(w, &pt))
    return false;
  if (out_x)
    *out_x = (int)pt.x;
  if (out_y)
    *out_y = (int)pt.y;
  return true;
}

bool Platform_DrawTextRaw(PlDrawTextRawSpec spec) {
  if (!spec.text || !g_draw_hdc)
    return false;
  int x = 0, y = 0;
  unsigned long color = 0;
  win32_draw_text_prep(spec.x, spec.y, spec.color, &x, &y, &color);
  const char *a = spec.align ? spec.align : "CENTER";
  HFONT old = NULL;
  int tlen = (int)strlen(spec.text);
  int wlen = 0;
  wchar_t *wide = win32_widen(spec.text, tlen, &wlen);
  if (!wide)
    return false;
  int dy = 0;
  HFONT hfont = spec.font.hfont
                    ? spec.font.hfont
                    : win32_font_for_text(wide, wlen, spec.fontsize, &old, &dy);
  if (!hfont)
    hfont = win32_sized_font(spec.fontsize);
  if (!old && hfont)
    old = (HFONT)SelectObject(g_draw_hdc, hfont);

  int y_draw = y + dy;
  if (pl_draw_state_get()->win32_widget_surface) {
    TEXTMETRICW tm = {0};
    if (GetTextMetricsW(g_draw_hdc, &tm))
      y_draw -= (int)tm.tmInternalLeading - 1;
  }
  SetBkMode(g_draw_hdc, TRANSPARENT);
  SetTextColor(g_draw_hdc, color);
  bool centered = string_contains_ci(a, "CENTER");
  bool right = !centered && string_contains_ci(a, "RIGHT");
  bool measuring = pl_draw_state_get()->measuring;
  SIZE msz;
  msz.cx = 0;
  msz.cy = 0;
  if (measuring || centered || right) {
    if (dy != 0) {
      HFONT nf = win32_sized_font(spec.fontsize);
      if (!nf)
        nf = win32_sized_font(spec.fontsize);
      if (nf) {
        HFONT t2 = (HFONT)SelectObject(g_draw_hdc, nf);
        GetTextExtentPoint32W(g_draw_hdc, wide, wlen, &msz);
        SelectObject(g_draw_hdc, t2);
      } else {
        GetTextExtentPoint32W(g_draw_hdc, wide, wlen, &msz);
      }
    } else {
      GetTextExtentPoint32W(g_draw_hdc, wide, wlen, &msz);
    }
  }
  if (measuring) {
    int bx = x;
    if (centered)
      bx = x - msz.cx / 2;
    else if (right)
      bx = x - msz.cx;
    draw_measure_track(bx, y, msz.cx, msz.cy);
  }
  if (!centered && !right) {
    int x0 = spec.pad ? spec.pad : x;
    if (measuring)
      draw_measure_track(x0, y, msz.cx, msz.cy);
    TextOutW(g_draw_hdc, x0, y_draw, wide, wlen);
    free(wide);
    if (old)
      SelectObject(g_draw_hdc, old);
    return true;
  }
  int box = (spec.pad == 0) ? x : 0;
  int x0;
  if (centered) {
    x0 = box > 0 ? (box - msz.cx) / 2
                 : spec.pad + (PROMPT_WRAP_BOX_CENTER - msz.cx) / 2;
  } else {
    x0 = box > 0 ? box - msz.cx : PROMPT_WRAP_BOX_RIGHT - msz.cx - spec.pad;
  }
  TextOutW(g_draw_hdc, x0, y_draw, wide, wlen);
  free(wide);
  if (old)
    SelectObject(g_draw_hdc, old);
  return true;
}

void Platform_DrawTextCentered(PlDrawTextCenteredSpec spec) {
  if (!spec.text || !g_draw_hdc) {
    return;
  }
  HFONT hfont =
      spec.font.hfont ? spec.font.hfont : win32_sized_font(spec.fontsize);
  HFONT old = hfont ? (HFONT)SelectObject(g_draw_hdc, hfont) : NULL;
  TEXTMETRICW tm = {0};
  GetTextMetricsW(g_draw_hdc, &tm);
  int ascent = tm.tmAscent;
  int descent = tm.tmDescent;
  int line_height = ascent + descent;
  draw_text_centered_impl(&spec, line_height);
  if (old)
    SelectObject(g_draw_hdc, old);
}

int Platform_MeasureTextWidth(PlMeasureTextWidthSpec spec) {
  if (!spec.text || spec.len <= 0)
    return 0;
  if (!g_draw_hdc)
    return spec.len * (spec.fontsize > 0 ? spec.fontsize : 16) / 2;
  int wlen = 0;
  wchar_t *wide = win32_widen(spec.text, spec.len, &wlen);
  if (!wide)
    return 0;
  HFONT old = NULL;
  HFONT hfont = win32_font_for_text(wide, wlen, spec.fontsize, &old, NULL);
  if (!hfont)
    hfont = win32_sized_font(spec.fontsize);
  SIZE sz;
  sz.cx = 0;
  GetTextExtentPoint32W(g_draw_hdc, wide, wlen, &sz);
  if (old)
    SelectObject(g_draw_hdc, old);
  free(wide);
  return sz.cx;
}

bool Platform_DrawTextLine(PlDrawTextLineSpec spec) {
  if (!spec.text || spec.len <= 0 || !g_draw_hdc)
    return false;
  int x = 0, y = 0;
  unsigned long color = 0;
  win32_draw_text_prep(spec.x, spec.y, spec.color, &x, &y, &color);
  int wlen = 0;
  wchar_t *wide = win32_widen(spec.text, spec.len, &wlen);
  if (!wide)
    return false;
  HFONT hfont = win32_sized_font(spec.fontsize);
  HFONT old = hfont ? (HFONT)SelectObject(g_draw_hdc, hfont) : NULL;
  SetBkMode(g_draw_hdc, TRANSPARENT);
  SetTextColor(g_draw_hdc, color);
  TextOutW(g_draw_hdc, x, y, wide, wlen);
  if (old)
    SelectObject(g_draw_hdc, old);
  free(wide);
  return true;
}

bool Platform_DrawRectRaw(PlDrawRectRawSpec spec) {
  RECT_RAW_UNPACK(spec);

  if (!g_draw_hdc || w <= 0 || h <= 0) {
    return false;
  }

  x += pl_draw_state_get()->off_x;
  y += pl_draw_state_get()->off_y;

  int corner_w = 0;
  int corner_h = 0;
  if (rounded && radius > 0) {
    int r = radius;
    if (r * 2 > w)
      r = w / 2;
    if (r * 2 > h)
      r = h / 2;

    corner_w = r * 2;
    corner_h = r * 2;
  }

  if (filled || thickness <= 0.0f) {
    int fill_t = filled ? 0 : 1;
    if (filled)
      draw_measure_track(x, y, w, h);
    else
      draw_measure_track(x - fill_t, y - fill_t, w + 2 * fill_t,
                         h + 2 * fill_t);
    HBRUSH brush = CreateSolidBrush(win32_resolve_color(color, opaque));
    HGDIOBJ old_brush = SelectObject(g_draw_hdc, brush);
    HGDIOBJ old_pen = SelectObject(g_draw_hdc, GetStockObject(NULL_PEN));

    if (corner_w > 0 && corner_h > 0) {
      RoundRect(g_draw_hdc, x, y, x + w, y + h, corner_w, corner_h);
    } else {
      RECT rc;
      rc.left = x;
      rc.top = y;
      rc.right = x + w;
      rc.bottom = y + h;
      FillRect(g_draw_hdc, &rc, brush);
    }

    SelectObject(g_draw_hdc, old_pen);
    SelectObject(g_draw_hdc, old_brush);
    DeleteObject(brush);
    return true;
  }

  int t = draw_thickness_px(thickness);

  draw_measure_track(x - t, y - t, w + 2 * t, h + 2 * t);

  HPEN pen = CreatePen(PS_SOLID, t, win32_resolve_color(color, opaque));
  HGDIOBJ old_pen = SelectObject(g_draw_hdc, pen);
  HGDIOBJ old_br = SelectObject(g_draw_hdc, GetStockObject(NULL_BRUSH));

  RoundRect(g_draw_hdc, x, y, x + w, y + h, corner_w, corner_h);

  SelectObject(g_draw_hdc, old_br);
  SelectObject(g_draw_hdc, old_pen);
  DeleteObject(pen);
  return true;
}

int Platform_GetTextLineHeight(int fontsize) {
  if (fontsize <= 0)
    fontsize = FontSize;
  if (!g_draw_hdc)
    return fontsize;
  FontHandle font = Platform_CreateFont(fontsize, UsedFont);
  HFONT old = font.hfont ? (HFONT)SelectObject(g_draw_hdc, font.hfont) : NULL;
  TEXTMETRICW tm = {0};
  GetTextMetricsW(g_draw_hdc, &tm);
  if (old)
    SelectObject(g_draw_hdc, old);
  Platform_DestroyFont(font);
  return tm.tmHeight > 0 ? (int)tm.tmHeight : fontsize;
}

bool Platform_DrawLineRaw(PlDrawLineRawSpec spec) {
  int x1, y1, x2, y2;
  int t = draw_line_prep(&spec, pl_draw_state_get()->off_x,
                         pl_draw_state_get()->off_y, &x1, &y1, &x2, &y2);
  HPEN pen = CreatePen(PS_SOLID, t, win32_colorref(spec.color));
  HGDIOBJ old = SelectObject(g_draw_hdc, pen);
  MoveToEx(g_draw_hdc, x1, y1, NULL);
  LineTo(g_draw_hdc, x2, y2);
  SelectObject(g_draw_hdc, old);
  DeleteObject(pen);
  return true;
}

bool Platform_DrawCircleRaw(PlDrawCircleRawSpec spec) {
  if (spec.radius <= 0)
    return false;
  int cx, cy;
  int t = draw_circle_prep(&spec, pl_draw_state_get()->off_x,
                           pl_draw_state_get()->off_y, &cx, &cy);
  int left = cx - spec.radius, top = cy - spec.radius;
  int right = cx + spec.radius, bottom = cy + spec.radius;
  if (t > 0) {
    HPEN pen = CreatePen(PS_SOLID, t, win32_colorref(spec.color));
    HGDIOBJ old = SelectObject(g_draw_hdc, pen);
    HGDIOBJ old_br = SelectObject(g_draw_hdc, GetStockObject(NULL_BRUSH));
    Ellipse(g_draw_hdc, left, top, right, bottom);
    SelectObject(g_draw_hdc, old_br);
    SelectObject(g_draw_hdc, old);
    DeleteObject(pen);
  } else {
    HBRUSH br = CreateSolidBrush(win32_colorref(spec.color));
    HGDIOBJ old = SelectObject(g_draw_hdc, br);
    HGDIOBJ old_pen = SelectObject(g_draw_hdc, GetStockObject(NULL_PEN));
    Ellipse(g_draw_hdc, left, top, right, bottom);
    SelectObject(g_draw_hdc, old_pen);
    SelectObject(g_draw_hdc, old);
    DeleteObject(br);
  }
  return true;
}

bool Platform_DrawImageAt(const char *path, int x, int y, int w, int h) {
  if (!path || !path[0])
    return false;
  int _offx = pl_draw_state_get()->off_x;
  int _offy = pl_draw_state_get()->off_y;
  x += _offx;
  y += _offy;
  if (pl_draw_state_get()->listbox_rows &&
      pl_draw_state_get()->win32_listbox_surface)
    y -= 2;
  if (w <= 0)
    w = (IconSize > 0) ? IconSize : 16;
  if (h <= 0)
    h = (IconSize > 0) ? IconSize : 16;
  draw_measure_track(x, y, w, h);

  if (!g_draw_hdc) {
    log_error("img: no draw context (g_draw_hdc NULL)");
    return false;
  }

  HDC screen = win32_screen_dc();
  if (!screen) {
    log_error("img: GetDC(NULL) failed");
    return false;
  }

  void *bmp = win32_img_cache_get(path, screen, w, h);
  if (bmp == WIN32_IMG_FAILED) {
    return false;
  }
  if (!bmp) {
    bmp = win32_load_scaled_bitmap(path, w, h);
    win32_img_cache_put(path, screen, w, h, bmp);
  }
  if (!bmp) {
    return false;
  }

  win32_gdip_ensure();
  if (g_gdip_token == 0) {
    log_error("img: Gdiplus unavailable");
    return false;
  }

  if (s_cached_hdc != g_draw_hdc || !s_cached_g) {
    if (s_cached_g) {
      GdipDeleteGraphics(s_cached_g);
      s_cached_g = NULL;
    }
    s_cached_hdc = g_draw_hdc;
    if (GdipCreateFromHDC(g_draw_hdc, &s_cached_g) != Ok || !s_cached_g) {
      log_error("img: GdipCreateFromHDC failed for hdc=%p", (void *)g_draw_hdc);
      s_cached_hdc = NULL;
      s_cached_g = NULL;
      return false;
    }
    GdipSetInterpolationMode(s_cached_g, InterpolationModeNearestNeighbor);
    GdipSetPixelOffsetMode(s_cached_g, PixelOffsetModeNone);
    GdipSetCompositingQuality(s_cached_g, CompositingQualityHighSpeed);
    GdipSetSmoothingMode(s_cached_g, SmoothingModeHighSpeed);
  }

  GpStatus status = GdipDrawImageRectI(s_cached_g, (GpBitmap *)bmp, x, y, w, h);
  if (status != Ok) {
    log_error("img: GdipDrawImageRectI failed st=%d at %d,%d %dx%d",
              (int)status, x, y, w, h);
  }
  return (status == Ok);
}

static void win32_draw_prompt_editbox(PromptState *state, int part_mask,
                                      bool persist_scroll) {
  if (!state || !g_draw_hdc) {
    return;
  }
  bool prev_lb = pl_draw_state_get()->win32_listbox_surface;
  pl_draw_state_get()->win32_listbox_surface = false;
  FontHandle font = Platform_CreateFont(FontSize, UsedFont);
  EditboxSpec eb;
  EditboxLayout lay;
  win32_prompt_editbox_spec(state, font, &eb);
  editbox_layout(&eb, &lay);
  if (persist_scroll)
    state->eb->scroll_offset = eb.scroll;
  editbox_draw(&eb, &lay, part_mask);
  Platform_DestroyFont(font);
  pl_draw_state_get()->win32_listbox_surface = prev_lb;
}

void Platform_DrawPromptEditText(PromptState *state) {
  win32_draw_prompt_editbox(state, EDITBOX_PART_TINT | EDITBOX_PART_TEXT, true);
}

void Platform_DrawPromptEditbox(PromptState *state) {
  win32_draw_prompt_editbox(state, EDITBOX_PART_ALL, true);
}

void Platform_DrawPromptEditCursor(PromptState *state) {
  win32_draw_prompt_editbox(state, EDITBOX_PART_CARET, false);
}
