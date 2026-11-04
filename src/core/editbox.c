#include "core/edit_state.h"
#include "core/editbox.h"

#include <string.h>

#include "core/modal.h"
#include "core/prompt_geometry.h"
#include "helpers/helpers_ui.h"
#include "platform/draw.h"
#include "platform/platform.h"
#include "config_values.h"

static int editbox_measure(const EditboxSpec *spec, const char *text, int len) {
  if (!text || len <= 0)
    return 0;
  if (spec->measure)
    return spec->measure(text, len, spec->measure_user);
  return Platform_MeasureTextWidth(
      (PlMeasureTextWidthSpec){.text = text, .len = len, .fontsize = FontSize});
}

void Editbox_BuildSpec(EditboxSpec *out, const EditboxInit *in) {
  if (!out || !in)
    return;
  memset(out, 0, sizeof(*out));
  out->text = in->text;
  out->text_len = in->text_len;
  out->cursor = in->cursor;
  out->sel_mode = EDITBOX_SEL_INCLUSIVE_CARET;
  out->x = in->x;
  out->y = in->y;
  out->w = in->w;
  out->inset = PROMPT_EDIT_INSET;
  out->visible_w = in->w - PROMPT_EDIT_SCROLL_PAD;
  out->scroll = in->state ? in->state->editbox->scroll_offset : 0;
  out->cursor_x = in->cursor_x;
  out->text_w = in->text_w;
  out->text_y = in->y;
  out->caret_y = Editbox_CaretY(in->y, in->ascent, in->descent);
  out->caret_h = PromptCaretHeight();
  out->placeholder = in->state ? in->state->editbox->placeholder : NULL;
  out->placeholder_dy = 2;
  out->rainbow_limit_x = -1;
  out->text_color = in->text_color;
  out->font = in->font;
  Editbox_ResolveSelection(in->state, in->text_len, true, &out->sel_start,
                           &out->sel_end, &out->has_selection);
}

int Editbox_CaretY(int box_top, int ascent, int descent) {
  int line_h = ascent + descent;
  if (line_h <= 0 || FontSize <= 0)
    return box_top;
  return box_top + (line_h - FontSize) / 2 + (PROMPT_DEADCURSOR_Y / 4);
}

void Editbox_ResolveSelection(PromptState *state, int text_len,
                              bool prefer_state, int *out_start, int *out_end,
                              bool *out_has) {
  int sel_start = 0;
  int sel_end = 0;
  bool has = false;
  if (prefer_state && state && state->editbox->sel_start >= 0 &&
      state->editbox->sel_end > state->editbox->sel_start) {
    sel_start = state->editbox->sel_start;
    sel_end = state->editbox->sel_end;
    has = true;
  } else {
    has = EditState_GetSelection(&sel_start, &sel_end) && text_len > 0;
  }
  if (out_start)
    *out_start = sel_start;
  if (out_end)
    *out_end = sel_end;
  if (out_has)
    *out_has = has;
}

int Editbox_ScrollOffset(const EditboxSpec *spec) {
  if (!spec)
    return 0;
  int visible_w = spec->visible_w;
  int scroll = spec->scroll;
  int cursor_x = spec->cursor_x;
  int text_w = spec->text_w;
  if (cursor_x - scroll > visible_w)
    scroll = cursor_x - visible_w;
  if (cursor_x - scroll < 0)
    scroll = cursor_x;
  if (scroll < 0)
    scroll = 0;
  if (text_w - scroll < visible_w && scroll > 0) {
    if (text_w < visible_w)
      scroll = 0;
    else
      scroll = text_w - visible_w;
  }
  if (cursor_x - scroll < 0)
    scroll = cursor_x;
  if (cursor_x - scroll > visible_w)
    scroll = cursor_x - visible_w;
  if (scroll < 0)
    scroll = 0;
  return scroll;
}

static void editbox_draw_rainbow(const EditboxSpec *spec,
                                 const EditboxLayout *layout) {
  int text_len = spec->text_len;
  int nchars = 0;
  for (int p = 0; p < text_len;) {
    unsigned char c = (unsigned char)spec->text[p];
    int cl = c < 0x80             ? 1
             : (c & 0xE0) == 0xC0 ? 2
             : (c & 0xF0) == 0xE0 ? 3
             : (c & 0xF8) == 0xF0 ? 4
                                  : 1;
    if (cl < 1)
      cl = 1;
    p += cl;
    nchars++;
  }
  int ci = 0;
  size_t pos = 0;
  while (pos < (size_t)text_len) {
    unsigned char c = (unsigned char)spec->text[pos];
    int cl = c < 0x80             ? 1
             : (c & 0xE0) == 0xC0 ? 2
             : (c & 0xF0) == 0xE0 ? 3
             : (c & 0xF8) == 0xF0 ? 4
                                  : 1;
    if (cl < 1)
      cl = 1;
    char buf[8];
    memcpy(buf, spec->text + pos, cl);
    buf[cl] = '\0';
    int tx = layout->draw_x +
             Platform_MeasureTextWidth((PlMeasureTextWidthSpec){
                 .text = spec->text, .len = (int)pos, .fontsize = FontSize});
    if (spec->rainbow_limit_x >= 0 && tx >= spec->rainbow_limit_x)
      break;
    unsigned long col =
        GetRainbowColor(nchars > 1 ? (float)ci / (float)(nchars - 1) : 0.5f,
                        0.85f, GradientBrightness);
    Platform_DrawTextRaw((PlDrawTextRawSpec){.x = tx,
                                             .y = spec->text_y,
                                             .text = buf,
                                             .color = col,
                                             .font = spec->font,
                                             .align = "",
                                             .pad = 0,
                                             .fontsize = 0});
    pos += cl;
    ci++;
  }
}

void Editbox_Layout(EditboxSpec *spec, EditboxLayout *out) {
  if (!spec) {
    return;
  }

  EditboxLayout layout;
  memset(&layout, 0, sizeof(layout));

  int scroll = Editbox_ScrollOffset(spec);
  spec->scroll = scroll;
  layout.scroll = scroll;

  int draw_x = spec->x + spec->inset - scroll;
  int cursor_draw_x = spec->x + spec->inset + spec->cursor_x - scroll;
  if (spec->align && spec->text_w > 0 && spec->text_w <= spec->w - 20) {
    if (Platform_StringContainsCaseInsensitive(spec->align, "CENTER")) {
      draw_x = (spec->w - spec->text_w) / 2;
      cursor_draw_x = draw_x + spec->cursor_x;
    } else if (Platform_StringContainsCaseInsensitive(spec->align, "RIGHT")) {
      draw_x = spec->w - spec->inset - spec->text_w;
      cursor_draw_x = draw_x + spec->cursor_x;
    }
  }
  layout.draw_x = draw_x;
  layout.cursor_draw_x = cursor_draw_x;

  int lo = spec->x + PROMPT_CARET_LO_CLAMP;
  int hi = spec->x + spec->w - PROMPT_CARET_HI_CLAMP;
  int caret_w = 1;
  PlMetrics *metrics = Platform_GetMetrics();
  if (metrics && metrics->edit_cursor_width > 0)
    caret_w = metrics->edit_cursor_width;

  if (spec->has_selection) {
    int sel_start = spec->sel_start < 0 ? 0 : spec->sel_start;
    int sel_end = spec->sel_end;
    if (spec->sel_mode == EDITBOX_SEL_EXCLUSIVE) {
      if (sel_end > spec->text_len)
        sel_end = spec->text_len;
    } else {
      if (sel_end >= spec->text_len)
        sel_end = spec->text_len - 1;
    }
    if (sel_start <= sel_end && sel_start < spec->text_len) {
      int sx0 = draw_x;
      int sx1 = draw_x;
      if (sel_start > 0)
        sx0 = draw_x + editbox_measure(spec, spec->text, sel_start);
      if (spec->sel_mode == EDITBOX_SEL_INCLUSIVE) {
        sx1 = draw_x + editbox_measure(spec, spec->text, sel_end + 1);
      } else if (spec->sel_mode == EDITBOX_SEL_INCLUSIVE_CARET) {
        sx1 = draw_x + editbox_measure(spec, spec->text, sel_end) + caret_w;
      } else {
        sx1 = draw_x + editbox_measure(spec, spec->text, sel_end);
      }
      if (sx0 < lo)
        sx0 = lo;
      if (sx1 > hi)
        sx1 = hi;
      layout.draw_selection = true;
      layout.sel_x0 = sx0;
      layout.sel_x1 = sx1;
    }
  }

  int caret_x = cursor_draw_x;
  if (caret_x < lo)
    caret_x = lo;
  if (caret_x > hi)
    caret_x = hi;
  layout.caret_x = caret_x;
  layout.caret_w = caret_w;

  if (out)
    *out = layout;
}

void Editbox_Draw(const EditboxSpec *spec, const EditboxLayout *layout,
                  int parts) {

  if (!spec || !layout) {
    return;
  }

  if (parts & EDITBOX_PART_TINT) {
    unsigned long tint = Color_Lerp(rgb_to_hex(COLOR_BG),
                                    rgb_to_hex(COLOR_CURSOR), 0.72f);
    if (layout->draw_selection) {
      Platform_DrawRectRaw(
          (PlDrawRectRawSpec){.x = layout->sel_x0,
                              .y = spec->caret_y,
                              .w = layout->sel_x1 - layout->sel_x0,
                              .h = spec->caret_h,
                              .color = tint,
                              .radius = 0,
                              .rounded = false,
                              .opaque = true,
                              .thickness = 1,
                              .filled = true}

      );
    }
  }

  if (parts & EDITBOX_PART_TEXT) {
    if (spec->text_len > 0) {
      if (RainbowText) {
        editbox_draw_rainbow(spec, layout);
      } else {
        Platform_DrawTextRaw((PlDrawTextRawSpec){.x = layout->draw_x,
                                                 .y = spec->text_y,
                                                 .text = spec->text,
                                                 .color = spec->text_color,
                                                 .font = spec->font,
                                                 .align = "",
                                                 .pad = 0,
                                                 .fontsize = 0});
      }
    } else if (spec->placeholder && spec->placeholder[0]) {
      unsigned long ph =
          Color_Lerp(rgb_to_hex(COLOR_BG), rgb_to_hex(COLOR_TEXT), 0.45f);
      Platform_DrawTextRaw(
          (PlDrawTextRawSpec){.x = layout->draw_x,
                              .y = spec->text_y + spec->placeholder_dy,
                              .text = spec->placeholder,
                              .color = ph,
                              .font = spec->font,
                              .align = "",
                              .pad = 0,
                              .fontsize = 0});
    }
  }

  if (parts & EDITBOX_PART_CARET) {
    Platform_DrawRectRaw(
        (PlDrawRectRawSpec){.x = layout->caret_x,
                            .y = spec->caret_y,
                             .w = layout->caret_w,
                            .h = spec->caret_h,
                            .color = rgb_to_hex(COLOR_CURSOR),
                            .radius = 0,
                            .rounded = false,
                            .opaque = true,
                            .thickness = 1,
                            .filled = true});
  }
}

void Editbox_Render(EditboxSpec *spec, EditboxLayout *out) {
  if (!spec) {
    return;
  }
  EditboxLayout layout;
  Editbox_Layout(spec, &layout);
  Editbox_Draw(spec, &layout, EDITBOX_PART_ALL);
  if (out)
    *out = layout;
}
