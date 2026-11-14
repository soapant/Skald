#include "helpers/helpers_text.h"
#include "core/edit_state.h"
#include "core/editbox.h"

#include <string.h>

#include "core/prompt_geometry.h"
#include "core/types/window.h"
#include "helpers/helpers_ui.h"
#include "platform/draw.h"
#include "platform/platform.h"
#include "config_values.h"

static int editbox_measure(const EditboxSpec *spec, const char *text, int len) {
  if (!text || len <= 0)
    return 0;
  if (spec->measure)
    return spec->measure(text, len, spec->measure_user);
  return Platform_MeasureTextWidth((PlMeasureTextWidthSpec){
      .text = text,
      .len = len,
      .fontsize = spec->fontsize > 0 ? spec->fontsize : FontSize});
}

void Editbox_ResolvePlace(const char *buffer, int x, int y, int w, int h,
                          int fontsize, PromptState *state, EditboxPlace *out) {
  if (!out)
    return;
  memset(out, 0, sizeof(*out));
  out->state = state;
  int fs = fontsize > 0 ? fontsize : FontSize;
  const char *buf = buffer;
  if (!buf)
    buf = state ? state->eb->filter : "";
  if (w <= 0)
    w = state ? state->width : PROMPT_CUSTOM_FALLBACK_W;
  if (h <= 0)
    h = PromptCustomEditHeight();
  out->x = x;
  out->w = w;
  out->h = h;
  out->fontsize = fs;
  out->buf = buf;
  out->text_len = (int)strlen(buf);
  out->cursor = state && state->eb ? (int)state->eb->cursor : out->text_len;
  if (out->cursor < 0)
    out->cursor = 0;
  if (out->cursor > out->text_len)
    out->cursor = out->text_len;
  out->y = y;
  out->y_raw = y == 0 ? PROMPT_EDIT_INSET : y;
}

void Editbox_InitFromSpec(EditboxInit *out, const EditboxInitFromSpec *args) {
  if (!out || !args)
    return;
  memset(out, 0, sizeof(*out));
  out->state = args->state;
  out->text = args->buf;
  out->text_len = args->text_len;
  out->cursor = args->cursor;
  out->x = args->x;
  out->y = args->y;
  out->w = args->w;
  out->h = args->h;
  out->cursor_x = args->cursor_x;
  out->text_w = args->text_w;
  out->ascent = args->ascent;
  out->descent = args->descent;
  out->text_color =
      args->text_color ? RgbToHex(args->text_color) : RgbToHex(COLOR_TEXT);
  out->bg_color =
      args->bg_color ? RgbToHex(args->bg_color) : RgbToHex(COLOR_BG);
  out->placeholder = args->placeholder;
  out->caret_height = args->caret_height;
  out->caret_width = args->caret_width;
  out->caret_x = args->caret_x;
  out->caret_y = args->caret_y;
  out->caret_pos = args->caret_pos;
  out->fontsize = args->fontsize;
  out->font = args->font;
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
  out->scroll = in->state ? in->state->eb->scroll_offset : 0;
  out->cursor_x = in->cursor_x;
  out->text_w = in->text_w;
  out->text_y = in->y;
  int caret_auto_y = Editbox_CaretY(in->y, in->ascent, in->descent,
                                    in->fontsize > 0 ? in->fontsize : FontSize);
  out->caret_y = in->caret_y > 0 ? in->caret_y : caret_auto_y;
  out->fontsize = in->fontsize > 0 ? in->fontsize : FontSize;
  out->caret_height =
      in->caret_height > 0 ? in->caret_height : PromptCaretHeight();
  out->caret_width = in->caret_width;
  out->caret_x = in->caret_x;
  out->caret_pos = in->caret_pos;
  out->caret_h = out->caret_height;
  out->placeholder = in->placeholder
                         ? in->placeholder
                         : (in->state ? in->state->eb->placeholder : NULL);
  out->placeholder_dy = 2;
  out->rainbow_limit_x = -1;
  out->text_color = in->text_color;
  out->bg_color = in->bg_color;
  out->font = in->font;
  Editbox_ResolveSelection(in->state, in->text_len, true, &out->sel_start,
                           &out->sel_end, &out->has_selection);
}

int Editbox_CaretY(int box_top, int ascent, int descent, int fontsize) {
  int line_h = ascent + descent;
  int fs = fontsize > 0 ? fontsize : FontSize;
  if (line_h <= 0 || fs <= 0)
    return box_top;
  return box_top + (line_h - fs) / 2 + (PROMPT_DEADCURSOR_Y / 4);
}

void EditboxSpec_FillFromPrompt(EditboxSpec *spec, PromptState *state,
                                int text_len, int cursor_x, int text_w,
                                int ascent, int descent, FontHandle font) {
  if (!spec || !state)
    return;
  int sel_start = 0, sel_end = 0;
  bool has_sel = false;
  Editbox_ResolveSelection(state, text_len, true, &sel_start, &sel_end,
                           &has_sel);
  memset(spec, 0, sizeof(*spec));
  spec->text = state->eb->filter;
  spec->text_len = text_len;
  spec->cursor = state->eb->cursor;
  spec->password_mode = state->password_mode;
  spec->sel_start = sel_start;
  spec->sel_end = sel_end;
  spec->has_selection = has_sel;
  int edit_y = PROMPT_EDIT_INSET;
  if (!state->lb && state->height > 0) {
    int line_h = ascent + descent;
    if (line_h <= 0)
      line_h = FontSize > 0 ? FontSize : 16;
    edit_y = (state->height - line_h) / 2;
    if (edit_y < 2)
      edit_y = 2;
  }
  spec->sel_mode = EDITBOX_SEL_INCLUSIVE_CARET;
  spec->x = 0;
  spec->y = edit_y;
  spec->w = state->width;
  spec->inset = PROMPT_EDIT_INSET;
  spec->visible_w = state->width - PROMPT_EDIT_SCROLL_PAD;
  spec->scroll = state->eb->scroll_offset;
  spec->cursor_x = cursor_x;
  spec->text_w = text_w;
  spec->text_y = edit_y;
  spec->caret_y = Editbox_CaretY(edit_y, ascent, descent, FontSize);
  spec->caret_h = PromptCaretHeight();
  spec->placeholder_dy = 2;
  spec->rainbow_limit_x = -1;
  spec->placeholder = state->eb->placeholder;
  spec->text_color = RgbToHex(COLOR_TEXT);
  spec->font = font;
}

void Editbox_ResolveSelection(PromptState *state, int text_len,
                              bool prefer_state, int *out_start, int *out_end,
                              bool *out_has) {
  int sel_start = 0;
  int sel_end = 0;
  bool has = false;
  if (prefer_state && state && state->eb->sel_start >= 0 &&
      state->eb->sel_end > state->eb->sel_start) {
    sel_start = state->eb->sel_start;
    sel_end = state->eb->sel_end;
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
    int cl = Utf8_LeadLen((unsigned char)spec->text[p]);
    p += cl;
    nchars++;
  }
  int ci = 0;
  size_t pos = 0;
  while (pos < (size_t)text_len) {
    int cl = Utf8_LeadLen((unsigned char)spec->text[pos]);
    if (pos + (size_t)cl > (size_t)text_len)
      cl = (int)((size_t)text_len - pos);
    char buf[8];
    memcpy(buf, spec->text + pos, cl);
    buf[cl] = '\0';
    int tx = layout->draw_x +
             Platform_MeasureTextWidth((PlMeasureTextWidthSpec){
                 .text = spec->text,
                 .len = (int)pos,
                 .fontsize = spec->fontsize > 0 ? spec->fontsize : FontSize});
    if (spec->rainbow_limit_x >= 0 && tx >= spec->rainbow_limit_x)
      break;
    unsigned long col = GetRainbowColor(
        nchars > 1 ? (float)ci / (float)(nchars - 1) : 0.5f, 0.85f, 1.0f);
    Platform_DrawTextRaw((PlDrawTextRawSpec){.x = tx,
                                             .y = spec->text_y,
                                             .text = buf,
                                             .color = col,
                                             .font = spec->font,
                                             .align = "",
                                             .pad = 0,
                                             .fontsize = spec->fontsize});
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
    if (String_ContainsCI(spec->align, "CENTER")) {
      draw_x = (spec->w - spec->text_w) / 2;
      cursor_draw_x = draw_x + spec->cursor_x;
    } else if (String_ContainsCI(spec->align, "RIGHT")) {
      draw_x = spec->w - spec->inset - spec->text_w;
      cursor_draw_x = draw_x + spec->cursor_x;
    }
  }
  layout.draw_x = draw_x;
  layout.cursor_draw_x = cursor_draw_x;

  int lo = spec->x + PROMPT_CARET_LO_CLAMP;
  int hi = spec->x + spec->w - PROMPT_CARET_HI_CLAMP;
  int caret_w = spec->caret_width > 0 ? spec->caret_width : 1;
  PlMetrics *metrics = Metrics_Get();
  if (spec->caret_width <= 0 && metrics && metrics->edit_cursor_width > 0)
    caret_w = metrics->edit_cursor_width;

  if (spec->has_selection) {
    int sel_start = spec->sel_start < 0 ? 0 : spec->sel_start;
    int sel_end = spec->sel_end;
    if (spec->sel_mode == EDITBOX_SEL_EXCLUSIVE) {
      if (sel_end > spec->text_len)
        sel_end = spec->text_len;
    } else if (spec->sel_mode == EDITBOX_SEL_INCLUSIVE) {
      if (sel_end >= spec->text_len)
        sel_end = spec->text_len - 1;
    } else if (sel_end > spec->text_len) {
      sel_end = spec->text_len;
    }
    if (sel_start <= sel_end && sel_start < spec->text_len) {
      int sx0 = draw_x;
      int sx1 = draw_x;
      if (sel_start > 0)
        sx0 = draw_x + editbox_measure(spec, spec->text, sel_start);
      if (spec->sel_mode == EDITBOX_SEL_INCLUSIVE) {
        sx1 = draw_x + editbox_measure(spec, spec->text, sel_end + 1);
      } else if (spec->sel_mode == EDITBOX_SEL_INCLUSIVE_CARET) {
        sx1 = draw_x + editbox_measure(spec, spec->text, sel_end);
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

  int caret_x = spec->caret_x > 0     ? spec->caret_x
                : spec->caret_pos > 0 ? draw_x + spec->caret_pos
                                      : cursor_draw_x;
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
  unsigned long bg = spec->bg_color ? spec->bg_color : RgbToHex(COLOR_BG);

  if (parts & EDITBOX_PART_TINT) {
    unsigned long tint = Color_Lerp(bg, RgbToHex(COLOR_CURSOR), 0.72f);
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
    if (spec->text_len > 0 && !spec->password_mode) {
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
                                                 .fontsize = spec->fontsize});
      }
    } else if (spec->placeholder && spec->placeholder[0]) {
      unsigned long ph =
          Color_Lerp(RgbToHex(COLOR_BG), RgbToHex(COLOR_TEXT), 0.45f);
      Platform_DrawTextRaw(
          (PlDrawTextRawSpec){.x = layout->draw_x,
                              .y = spec->text_y + spec->placeholder_dy,
                              .text = spec->placeholder,
                              .color = ph,
                              .font = spec->font,
                              .align = "",
                              .pad = 0,
                              .fontsize = spec->fontsize});
    }
  }

  if (parts & EDITBOX_PART_CARET) {
    if (spec->password_mode) {
      return;
    }
    Platform_DrawRectRaw((PlDrawRectRawSpec){.x = layout->caret_x,
                                             .y = spec->caret_y,
                                             .w = layout->caret_w,
                                             .h = spec->caret_h,
                                             .color = RgbToHex(COLOR_CURSOR),
                                             .radius = 0,
                                             .rounded = false,
                                             .opaque = true,
                                             .thickness = 1,
                                             .filled = true});
    if (layout->caret_w > 1) {
      const char *src = spec->password_mode ? NULL : spec->text;
      int srclen = spec->text_len;
      bool is_placeholder = false;
      if ((!src || srclen <= 0) && spec->placeholder && spec->placeholder[0]) {
        src = spec->placeholder;
        srclen = (int)strlen(src);
        is_placeholder = true;
      }
      if (src && spec->cursor >= 0 && spec->cursor < srclen) {
        int cl = 1;
        while (spec->cursor + cl < srclen &&
               ((unsigned char)src[spec->cursor + cl] & 0xC0) == 0x80)
          cl++;
        if (cl > 7)
          cl = 7;
        char ch[8];
        memcpy(ch, src + spec->cursor, (size_t)cl);
        ch[cl] = '\0';
        int ty =
            is_placeholder ? spec->text_y + spec->placeholder_dy : spec->text_y;
        Platform_DrawTextRaw((PlDrawTextRawSpec){.x = layout->caret_x,
                                                 .y = ty,
                                                 .text = ch,
                                                 .color = bg,
                                                 .font = spec->font,
                                                 .align = "",
                                                 .pad = 0,
                                                 .fontsize = spec->fontsize});
      }
    }
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
