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
  return Platform_MeasureTextWidth((MeasureTextWidthSpec){
      .text = text,
      .len = len,
      .font_size = spec->font_size > 0 ? spec->font_size : FontSize});
}

void editbox_resolve_place(const EditboxPlaceSpec *spec, PromptState *state,
                           EditboxPlace *out) {
  if (!out || !spec)
    return;
  memset(out, 0, sizeof(*out));
  out->state = state;
  int x = spec->x, y = spec->y, w = spec->w, h = spec->h;
  int font_size = spec->font_size;
  const char *buffer = spec->buffer;
  int fs = font_size > 0 ? font_size : FontSize;
  const char *buf = buffer;
  if (!buf)
    buf = state ? state->eb->filter : "";
  if (w <= 0)
    w = state ? state->width : PROMPT_CUSTOM_FALLBACK_W;
  if (h <= 0)
    h = prompt_custom_edit_height();
  out->x = x;
  out->w = w;
  out->h = h;
  out->font_size = fs;
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

void editbox_spec_from_place(EditboxSpec *out, const EditboxPlace *place,
                             PromptState *state, const EditboxUiSpec *ui,
                             const EditboxMetrics *metrics) {
  if (!out || !place || !ui || !metrics)
    return;
  int text_w = metrics->text_w, cursor_x = metrics->cursor_x;
  int ascent = metrics->ascent, descent = metrics->descent;
  FontHandle font = metrics->font;
  EditboxInitFromSpec init_args = {0};
  init_args.state = state;
  init_args.buf = place->buf;
  init_args.text_len = place->text_len;
  init_args.cursor = place->cursor;
  init_args.x = place->x;
  init_args.y = place->y_raw;
  init_args.w = place->w;
  init_args.h = place->h;
  init_args.cursor_x = cursor_x;
  init_args.text_w = text_w;
  init_args.ascent = ascent;
  init_args.descent = descent;
  init_args.font_size = place->font_size;
  init_args.font = font;
  init_args.text_color = ui->color_text;
  init_args.bg_color = ui->color_bg;
  init_args.placeholder = ui->placeholder;
  init_args.caret_h = ui->caret_h;
  init_args.caret_w = ui->caret_w;
  init_args.caret_x = ui->caret_x;
  init_args.caret_y = ui->caret_y;
  init_args.caret_pos = ui->caret_pos;
  EditboxInit ebi;
  editbox_init_from_spec(&ebi, &init_args);
  editbox_build_spec(out, &ebi);
}

void editbox_init_from_spec(EditboxInit *out, const EditboxInitFromSpec *args) {
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
      args->text_color ? rgb_to_hex(args->text_color) : rgb_to_hex(COLOR_TEXT);
  out->bg_color =
      args->bg_color ? rgb_to_hex(args->bg_color) : rgb_to_hex(COLOR_BG);
  out->placeholder = args->placeholder;
  out->caret_h = args->caret_h;
  out->caret_w = args->caret_w;
  out->caret_x = args->caret_x;
  out->caret_y = args->caret_y;
  out->caret_pos = args->caret_pos;
  out->font_size = args->font_size;
  out->font = args->font;
}

void editbox_build_spec(EditboxSpec *out, const EditboxInit *in) {
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
  int caret_auto_y =
      editbox_caret_y(in->y, in->ascent, in->descent,
                      in->font_size > 0 ? in->font_size : FontSize);
  out->caret_y = in->caret_y > 0 ? in->caret_y : caret_auto_y;
  out->font_size = in->font_size > 0 ? in->font_size : FontSize;
  out->caret_h =
      in->caret_h > 0 ? in->caret_h : prompt_caret_height();
  out->caret_w = in->caret_w;
  out->caret_x = in->caret_x;
  out->caret_pos = in->caret_pos;
  out->caret_h = out->caret_h;
  out->placeholder = in->placeholder
                         ? in->placeholder
                         : (in->state ? in->state->eb->placeholder : NULL);
  out->placeholder_dy = 2;
  out->rainbow_limit_x = -1;
  out->text_color = in->text_color;
  out->bg_color = in->bg_color;
  out->font = in->font;
  editbox_resolve_selection(in->state, in->text_len, true, &out->sel_start,
                            &out->sel_end, &out->has_selection);
}

int editbox_caret_y(int box_top, int ascent, int descent, int font_size) {
  int line_h = ascent + descent;
  int fs = font_size > 0 ? font_size : FontSize;
  if (line_h <= 0 || fs <= 0)
    return box_top;
  return box_top + (line_h - fs) / 2 + (PROMPT_DEADCURSOR_Y / 4);
}

void editbox_spec_fill_from_prompt(EditboxSpec *spec, PromptState *state,
                                   int text_len,
                                   const EditboxMetrics *metrics) {
  if (!spec || !state || !metrics)
    return;
  int cursor_x = metrics->cursor_x, text_w = metrics->text_w;
  int ascent = metrics->ascent, descent = metrics->descent;
  FontHandle font = metrics->font;
  int sel_start = 0, sel_end = 0;
  bool has_sel = false;
  editbox_resolve_selection(state, text_len, true, &sel_start, &sel_end,
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
  spec->caret_y = editbox_caret_y(edit_y, ascent, descent, FontSize);
  spec->caret_h = prompt_caret_height();
  spec->placeholder_dy = 2;
  spec->rainbow_limit_x = -1;
  spec->placeholder = state->eb->placeholder;
  spec->text_color = rgb_to_hex(COLOR_TEXT);
  spec->font = font;
}

void editbox_resolve_selection(PromptState *state, int text_len,
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
    has = edit_state_get_selection(&sel_start, &sel_end) && text_len > 0;
  }
  if (out_start)
    *out_start = sel_start;
  if (out_end)
    *out_end = sel_end;
  if (out_has)
    *out_has = has;
}

int editbox_scroll_offset(const EditboxSpec *spec) {
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
    int cl = utf8_lead_len((unsigned char)spec->text[p]);
    p += cl;
    nchars++;
  }
  int ci = 0;
  size_t pos = 0;
  while (pos < (size_t)text_len) {
    int cl = utf8_lead_len((unsigned char)spec->text[pos]);
    if (pos + (size_t)cl > (size_t)text_len)
      cl = (int)((size_t)text_len - pos);
    char buf[8];
    memcpy(buf, spec->text + pos, cl);
    buf[cl] = '\0';
    int tx = layout->draw_x +
             Platform_MeasureTextWidth((MeasureTextWidthSpec){
                 .text = spec->text,
                 .len = (int)pos,
                 .font_size = spec->font_size > 0 ? spec->font_size : FontSize});
    if (spec->rainbow_limit_x >= 0 && tx >= spec->rainbow_limit_x)
      break;
    unsigned long col = get_rainbow_color(
        nchars > 1 ? (float)ci / (float)(nchars - 1) : 0.5f, 0.85f, 1.0f);
    Platform_DrawTextRaw((DrawTextRawSpec){.x = tx,
                                           .y = spec->text_y,
                                           .text = buf,
                                           .color = col,
                                           .font = spec->font,
                                           .align = "",
                                           .pad = 0,
                                           .font_size = spec->font_size});
    pos += cl;
    ci++;
  }
}

void editbox_layout(EditboxSpec *spec, EditboxLayout *out) {
  if (!spec) {
    return;
  }

  EditboxLayout layout;
  memset(&layout, 0, sizeof(layout));

  int scroll = editbox_scroll_offset(spec);
  spec->scroll = scroll;
  layout.scroll = scroll;

  int draw_x = spec->x + spec->inset - scroll;
  int cursor_draw_x = spec->x + spec->inset + spec->cursor_x - scroll;
  if (spec->align && spec->text_w > 0 && spec->text_w <= spec->w - 20) {
    if (string_contains_ci(spec->align, "CENTER")) {
      draw_x = (spec->w - spec->text_w) / 2;
      cursor_draw_x = draw_x + spec->cursor_x;
    } else if (string_contains_ci(spec->align, "RIGHT")) {
      draw_x = spec->w - spec->inset - spec->text_w;
      cursor_draw_x = draw_x + spec->cursor_x;
    }
  }
  layout.draw_x = draw_x;
  layout.cursor_draw_x = cursor_draw_x;

  int lo = spec->x + PROMPT_CARET_LO_CLAMP;
  int hi = spec->x + spec->w - PROMPT_CARET_HI_CLAMP;
  int caret_w = spec->caret_w > 0 ? spec->caret_w : 1;
  const Metrics *metrics = metrics_get();
  if (spec->caret_w <= 0 && metrics && metrics->edit_cursor_width > 0)
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

void editbox_draw(const EditboxSpec *spec, const EditboxLayout *layout,
                  int parts) {

  if (!spec || !layout) {
    return;
  }
  unsigned long bg = spec->bg_color ? spec->bg_color : rgb_to_hex(COLOR_BG);

  if (parts & EDITBOX_PART_TINT) {
    unsigned long tint = color_lerp(bg, rgb_to_hex(COLOR_CURSOR), 0.72f);
    if (layout->draw_selection) {
      Platform_DrawRectRaw(
          (DrawRectRawSpec){.x = layout->sel_x0,
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
        Platform_DrawTextRaw((DrawTextRawSpec){.x = layout->draw_x,
                                               .y = spec->text_y,
                                               .text = spec->text,
                                               .color = spec->text_color,
                                               .font = spec->font,
                                               .align = "",
                                               .pad = 0,
                                               .font_size = spec->font_size});
      }
    } else if (spec->placeholder && spec->placeholder[0]) {
      unsigned long ph =
          color_lerp(rgb_to_hex(COLOR_BG), rgb_to_hex(COLOR_TEXT), 0.45f);
      Platform_DrawTextRaw(
          (DrawTextRawSpec){.x = layout->draw_x,
                            .y = spec->text_y + spec->placeholder_dy,
                            .text = spec->placeholder,
                            .color = ph,
                            .font = spec->font,
                            .align = "",
                            .pad = 0,
                            .font_size = spec->font_size});
    }
  }

  if (parts & EDITBOX_PART_CARET) {
    if (spec->password_mode) {
      return;
    }
    Platform_DrawRectRaw((DrawRectRawSpec){.x = layout->caret_x,
                                           .y = spec->caret_y,
                                           .w = layout->caret_w,
                                           .h = spec->caret_h,
                                           .color = rgb_to_hex(COLOR_CURSOR),
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
        Platform_DrawTextRaw((DrawTextRawSpec){.x = layout->caret_x,
                                               .y = ty,
                                               .text = ch,
                                               .color = bg,
                                               .font = spec->font,
                                               .align = "",
                                               .pad = 0,
                                               .font_size = spec->font_size});
      }
    }
  }
}

void editbox_render(EditboxSpec *spec, EditboxLayout *out) {
  if (!spec) {
    return;
  }
  EditboxLayout layout;
  editbox_layout(spec, &layout);
  editbox_draw(spec, &layout, EDITBOX_PART_ALL);
  if (out)
    *out = layout;
}
