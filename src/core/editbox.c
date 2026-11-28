#include "helpers/helpers_text.h"
#include "core/edit_state.h"
#include "core/editbox.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/context.h"
#include "core/focus.h"
#include "core/window_manager.h"
#include "helpers/helpers_input_state.h"
#include "core/prompt_geometry.h"
#include "core/listbox_hover.h"
#include "core/types/window.h"
#include "helpers/helpers_ui.h"
#include "platform/draw.h"
#include "platform/platform.h"
#include "deps/log.h"
#include "config_values.h"

int editbox_auto_width(const char *text, int font_size) {
  int w = 0;
  if (text && text[0]) {
    w = Platform_MeasureTextWidth((MeasureTextWidthSpec){
        .text = text, .len = (int)strlen(text), .font_size = font_size});
  }
  w += EDITBOX_TIGHT_INSET + EDITBOX_TIGHT_RIGHT_PAD;
  if (w < 24)
    w = 24;
  return w;
}

static EditboxWrapCache *editbox_prompt_wrap_cache(PromptState *state);

static const Metrics *editbox_state_metrics(PromptState *state) {
  if (state && state->active_editbox >= 0 &&
      state->active_editbox < state->editbox_count)
    return &state->editboxes[state->active_editbox].metrics;
  return metrics_get();
}

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
  if (w <= 0) {
    w = editbox_auto_width(buf, fs);
    int avail = state ? state->width : PROMPT_CUSTOM_FALLBACK_W;
    if (w > avail)
      w = avail;
  }
  if (h <= 0)
    h = prompt_custom_edit_height();
  out->x = x;
  out->w = w;
  out->h = h;
  out->font_size = fs;
  out->wrapped = spec->wrapped;
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

void editbox_spec_from_place(FnEditboxSpecFromPlaceSpec *argspec) {
  if (!argspec->f_out || !argspec->f_place || !argspec->f_ui || !argspec->f_metrics)
    return;
  int text_w = argspec->f_metrics->text_w, cursor_x = argspec->f_metrics->cursor_x;
  int ascent = argspec->f_metrics->ascent, descent = argspec->f_metrics->descent;
  FontHandle font = argspec->f_metrics->font;
  EditboxInitFromSpec init_args = {0};
  init_args.state = argspec->f_state;
  init_args.buf = argspec->f_place->buf;
  init_args.text_len = argspec->f_place->text_len;
  init_args.cursor = argspec->f_place->cursor;
  init_args.x = argspec->f_place->x;
  init_args.y = argspec->f_place->y_raw;
  init_args.w = argspec->f_place->w;
  init_args.h = argspec->f_place->h;
  init_args.cursor_x = cursor_x;
  init_args.text_w = text_w;
  init_args.ascent = ascent;
  init_args.descent = descent;
  init_args.wrapped = argspec->f_place->wrapped;
  init_args.font_size = argspec->f_place->font_size;
  init_args.font = font;
  init_args.text_color = argspec->f_ui->color_text;
  init_args.bg_color = argspec->f_ui->color_bg;
  init_args.placeholder = argspec->f_ui->placeholder;
  init_args.caret_h = argspec->f_ui->caret_h;
  init_args.caret_w = argspec->f_ui->caret_w;
  init_args.caret_x = argspec->f_ui->caret_x;
  init_args.caret_y = argspec->f_ui->caret_y;
  init_args.caret_pos = argspec->f_ui->caret_pos;
  EditboxInit ebi;
  editbox_init_from_spec(&ebi, &init_args);
  editbox_build_spec(argspec->f_out, &ebi);
  argspec->f_out->draw_bg = argspec->f_ui->color_bg != 0;
  argspec->f_out->wrap_cache = editbox_prompt_wrap_cache(argspec->f_state);
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
  out->wrapped = args->wrapped;
  out->ascent = args->ascent;
  out->descent = args->descent;
  out->text_color =
      args->text_color ? rgb_to_hex(args->text_color) : rgb_to_hex(COLOR_TEXT);
  out->bg_color = args->bg_color ? rgb_to_hex(args->bg_color) : 0UL;
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
  out->metrics = editbox_state_metrics(in->state);
  out->text = in->text;
  out->text_len = in->text_len;
  out->cursor = in->cursor;
  out->wrapped = in->wrapped;
  out->sel_mode = EDITBOX_SEL_INCLUSIVE_CARET;
  out->x = in->x;
  out->y = in->y;
  out->w = in->w;
  out->h = in->h;
  if (!in->wrapped && in->text_w > 0 &&
      in->w < in->text_w + PROMPT_EDIT_SCROLL_PAD + 16) {
    out->inset = EDITBOX_TIGHT_INSET;
    out->visible_w = in->w - EDITBOX_TIGHT_INSET - 2;
  } else {
    out->inset = PROMPT_EDIT_INSET;
    out->visible_w = in->w - PROMPT_EDIT_SCROLL_PAD;
  }
  out->scroll = in->state ? in->state->eb->scroll_offset : 0;
  out->cursor_x = in->cursor_x;
  out->text_w = in->text_w;
  out->text_y = in->y + Platform_EditboxPlaceholderOffset();
  int caret_auto_y =
      editbox_caret_y(in->y, in->ascent, in->descent,
                      in->font_size > 0 ? in->font_size : FontSize);
  out->caret_y = in->caret_y > 0 ? in->caret_y : caret_auto_y;
  out->font_size = in->font_size > 0 ? in->font_size : FontSize;
  out->caret_h = in->caret_h > 0 ? in->caret_h : prompt_caret_height();
  out->caret_w = in->caret_w;
  out->caret_x = in->caret_x;
  out->caret_pos = in->caret_pos;
  out->caret_h = out->caret_h;
  out->placeholder = in->placeholder
                         ? in->placeholder
                         : (in->state ? in->state->eb->placeholder : NULL);
  out->placeholder_dy = 0;
  out->text_color = in->text_color;
  out->bg_color = in->bg_color;
  out->font = in->font;
  editbox_resolve_selection(&(FnEditboxResolveSelectionSpec){.f_state = in->state, .f_text_len = in->text_len, .f_prefer_state = true, .f_out_start = &out->sel_start, .f_out_end = &out->sel_end, .f_out_has = &out->has_selection});
}

int editbox_caret_y(int box_top, int ascent, int descent, int font_size) {
  int line_h = ascent + descent;
  int fs = font_size > 0 ? font_size : FontSize;
  if (line_h <= 0 || fs <= 0)
    return box_top;
  return box_top + (line_h - fs) / 2 + (PROMPT_DEADCURSOR_Y / 4);
}

static bool editbox_prompt_wrapped(const PromptState *state) {
  if (state && state->active_editbox >= 0 &&
      state->active_editbox < state->editbox_count)
    return state->editboxes[state->active_editbox].wrapped;
  return false;
}

static EditboxWrapCache *editbox_prompt_wrap_cache(PromptState *state) {
  if (state && state->active_editbox >= 0 &&
      state->active_editbox < state->editbox_count)
    return &state->editboxes[state->active_editbox].wrap_cache;
  return NULL;
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
  editbox_resolve_selection(&(FnEditboxResolveSelectionSpec){.f_state = state, .f_text_len = text_len, .f_prefer_state = true, .f_out_start = &sel_start, .f_out_end = &sel_end, .f_out_has = &has_sel});
  memset(spec, 0, sizeof(*spec));
  spec->wrap_cache = editbox_prompt_wrap_cache(state);
  spec->metrics = editbox_state_metrics(state);
  spec->text = state->eb->filter;
  spec->text_len = text_len;
  spec->cursor = state->eb->cursor;
  spec->wrapped = editbox_prompt_wrapped(state);
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
  spec->h = state->height;
  spec->inset = PROMPT_EDIT_INSET;
  spec->visible_w = state->width - PROMPT_EDIT_SCROLL_PAD;
  spec->scroll = state->eb->scroll_offset;
  spec->cursor_x = cursor_x;
  spec->text_w = text_w;
  spec->text_y = edit_y + Platform_EditboxPlaceholderOffset();
  spec->caret_y = editbox_caret_y(edit_y, ascent, descent, FontSize);
  spec->caret_h = prompt_caret_height();
  spec->placeholder_dy = 0;
  spec->placeholder = state->eb->placeholder;
  spec->text_color = rgb_to_hex(COLOR_TEXT);
  spec->font = font;
  int edit_h = ascent + descent;
  if (edit_h <= 0)
    edit_h = FontSize > 0 ? FontSize : 16;
  editbox_legacy_tick(&(FnEditboxLegacyTickSpec){.f_state = state, .f_x = 0, .f_y = edit_y, .f_w = state->width, .f_h = edit_h, .f_font_size = FontSize});
}

void editbox_resolve_selection(FnEditboxResolveSelectionSpec *argspec) {
  int sel_start = 0;
  int sel_end = 0;
  bool has = false;
  if (argspec->f_prefer_state && argspec->f_state && argspec->f_state->eb->sel_start >= 0 &&
      argspec->f_state->eb->sel_end > argspec->f_state->eb->sel_start) {
    sel_start = argspec->f_state->eb->sel_start;
    sel_end = argspec->f_state->eb->sel_end;
    has = true;
  } else {
    has = edit_state_get_selection(&sel_start, &sel_end) && argspec->f_text_len > 0;
  }
  if (argspec->f_out_start)
    *argspec->f_out_start = sel_start;
  if (argspec->f_out_end)
    *argspec->f_out_end = sel_end;
  if (argspec->f_out_has)
    *argspec->f_out_has = has;
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

typedef struct {
  const EditboxSpec *spec;
  const char *text;
} EditboxWrapCtx;

static int editbox_wrap_measure(void *ctx, const char *text, int len) {
  EditboxWrapCtx *wrap = (EditboxWrapCtx *)ctx;
  return wrap ? editbox_measure(wrap->spec, text, len) : 0;
}

static int editbox_wrap_line_height(const EditboxSpec *spec) {
  int line_h = spec->font_size + 4;
  return line_h > 4 ? line_h : FontSize + 4;
}

static uint64_t editbox_wrap_key(const char *text, int len, int line_h,
                                 int max_width) {
  uint64_t h = 1469598103934665603ULL;
  for (int i = 0; i < len; i++) {
    h ^= (unsigned char)text[i];
    h *= 1099511628211ULL;
  }
  int meta[2] = {line_h, max_width};
  for (int i = 0; i < 2; i++) {
    h ^= (uint32_t)meta[i];
    h *= 1099511628211ULL;
  }
  return h;
}

static __thread Pos *s_wrap_fallback = NULL;
static __thread int s_wrap_fallback_cap = 0;

void editbox_wrap_cache_free(EditboxWrapCache *cache) {
  if (!cache)
    return;
  free(cache->positions);
  cache->positions = NULL;
  cache->positions_cap = 0;
  cache->key = 0;
  cache->len = 0;
  cache->count = 0;
}

static bool editbox_wrap_ensure(Pos **buf, int *cap, int need) {
  if (need <= *cap)
    return true;
  int ncap = *cap > 0 ? *cap : 128;
  while (ncap < need)
    ncap *= 2;
  Pos *grown = (Pos *)realloc(*buf, (size_t)ncap * sizeof(*grown));
  if (!grown)
    return false;
  *buf = grown;
  *cap = ncap;
  return true;
}

static const Pos *editbox_wrap_positions(const EditboxSpec *spec,
                                         const char *text, int line_h,
                                         int *count) {
  const char *src = text ? text : "";
  int len = (int)strlen(src);
  int max_width = spec->visible_w;
  uint64_t key = editbox_wrap_key(src, len, line_h, max_width);
  EditboxWrapCache *cache = spec->wrap_cache;
  if (cache && cache->positions && cache->key == key && cache->len == len &&
      cache->line_h == line_h && cache->max_width == max_width) {
    if (count)
      *count = cache->count;
    return cache->positions;
  }
  int need = len + 1;
  Pos **buf = cache ? &cache->positions : &s_wrap_fallback;
  int *cap = cache ? &cache->positions_cap : &s_wrap_fallback_cap;
  if (!editbox_wrap_ensure(buf, cap, need)) {
    if (count)
      *count = 0;
    return NULL;
  }
  Pos *out = *buf;
  EditboxWrapCtx ctx = {spec, src};
  int n = text_wrap_layout(&(FnTextWrapLayoutSpec){.f_text = src, .f_x0 = 0, .f_y0 = 0, .f_max_width = max_width, .f_line_height = line_h, .f_measure = editbox_wrap_measure, .f_ctx = &ctx, .f_out = out, .f_maxout = need});
  if (cache) {
    cache->key = key;
    cache->len = len;
    cache->line_h = line_h;
    cache->max_width = max_width;
    cache->count = n;
  }
  if (count)
    *count = n;
  return out;
}

static void editbox_wrap_caret(const EditboxSpec *spec, const Pos *pos,
                               int count, int line_h, int x0, int y0,
                               int *out_x, int *out_y, int *out_line) {
  int cursor = spec->cursor;
  if (cursor < 0)
    cursor = 0;
  if (cursor > spec->text_len)
    cursor = spec->text_len;
  int x = x0;
  int y = y0;
  if (count > 0) {
    if (cursor < count) {
      x = x0 + pos[cursor].x;
      y = y0 + pos[cursor].y;
    } else {
      x = x0 + pos[count - 1].x;
      y = y0 + pos[count - 1].y;
      if (spec->text_len > 0) {
        int start = spec->text_len - 1;
        while (start > 0 && (((unsigned char)spec->text[start] & 0xC0) == 0x80))
          start--;
        x += editbox_measure(spec, spec->text + start, spec->text_len - start);
      }
    }
  }
  int line = (y - y0) / line_h;
  if (line < 0)
    line = 0;
  if (out_x)
    *out_x = x;
  if (out_y)
    *out_y = y;
  if (out_line)
    *out_line = line;
}

static void editbox_wrap_draw_text(const EditboxSpec *spec, int draw_y,
                                   const char *text, unsigned long color) {
  if (!text || !text[0])
    return;
  int len = (int)strlen(text);
  if (len <= 0)
    return;
  int line_h = editbox_wrap_line_height(spec);
  int x0 = spec->x + spec->inset;
  int y0 = spec->text_y;
  int count = 0;
  const Pos *positions = editbox_wrap_positions(spec, text, line_h, &count);
  if (count <= 0)
    return;
  char line[1024];
  int start = 0;
  for (int i = 1; i <= count; i++) {
    if (i < count && positions[i].y == positions[start].y)
      continue;
    int end = i;
    while (end > start && text[end - 1] == '\n')
      end--;
    if (end > start) {
      int n = end - start;
      if (n >= (int)sizeof(line))
        n = (int)sizeof(line) - 1;
      memcpy(line, text + start, (size_t)n);
      line[n] = '\0';
      Platform_DrawTextRaw(
          (DrawTextRawSpec){.x = x0 + positions[start].x,
                            .y = y0 + positions[start].y - draw_y,
                            .text = line,
                            .color = color,
                            .font = spec->font,
                            .align = "",
                            .pad = 0,
                            .font_size = spec->font_size});
    }
    start = i;
  }
}

static int editbox_caret_width(const EditboxSpec *spec) {
  int caret_w = spec->caret_w > 0 ? spec->caret_w : 1;
  const Metrics *metrics = spec->metrics ? spec->metrics : metrics_get();
  if (spec->caret_w <= 0 && metrics && metrics->edit_cursor_width > 0)
    caret_w = metrics->edit_cursor_width;
  return caret_w;
}

void editbox_layout(EditboxSpec *spec, EditboxLayout *out) {
  if (!spec) {
    return;
  }

  EditboxLayout layout;
  memset(&layout, 0, sizeof(layout));

  if (spec->wrapped) {
    int line_h = editbox_wrap_line_height(spec);
    int x0 = spec->x + spec->inset;
    int y0 = spec->text_y;
    int count = 0;
    const Pos *positions =
        editbox_wrap_positions(spec, spec->text, line_h, &count);
    int caret_x = 0, caret_y = 0, caret_line = 0;
    editbox_wrap_caret(spec, positions, count, line_h, x0, y0, &caret_x,
                       &caret_y, &caret_line);
    int max_y = 0;
    for (int i = 0; i < count; i++)
      if (positions[i].y > max_y)
        max_y = positions[i].y;
    int line_count = max_y / line_h + 1;
    int visible = spec->h > 0 ? spec->h / line_h : 1;
    if (visible < 1)
      visible = 1;
    int scroll_y = 0;
    if (caret_line >= visible)
      scroll_y = (caret_line - visible + 1) * line_h;
    layout.draw_x = x0;
    layout.cursor_draw_x = caret_x;
    layout.caret_x = caret_x;
    layout.caret_w = editbox_caret_width(spec);
    int caret_line_y = caret_y - y0;
    layout.caret_y = spec->caret_y + caret_line_y - scroll_y;
    layout.caret_text_y = caret_y - scroll_y;
    layout.scroll_y = scroll_y;
    layout.line_count = line_count;
    layout.caret_line = caret_line;
    spec->scroll = 0;
    if (out)
      *out = layout;
    return;
  }

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
  int caret_w = editbox_caret_width(spec);

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
  if (spec->draw_bg && (parts & EDITBOX_PART_TINT))
    Platform_DrawRectRaw((DrawRectRawSpec){.x = spec->x,
                                           .y = spec->y,
                                           .w = spec->w,
                                           .h = spec->h,
                                           .color = spec->bg_color,
                                           .opaque = true,
                                           .radius = CornerRadius,
                                           .rounded = RoundedCorners,
                                           .thickness = 1,
                                           .filled = true});
  unsigned long bg = spec->bg_color ? spec->bg_color : rgb_to_hex(COLOR_BG);

  if (spec->wrapped) {
    if ((parts & EDITBOX_PART_TEXT) && !spec->password_mode) {
      if (spec->text_len > 0) {
        editbox_wrap_draw_text(spec, layout->scroll_y, spec->text,
                               spec->text_color);
      } else if (spec->placeholder && spec->placeholder[0]) {
        unsigned long ph =
            color_lerp(rgb_to_hex(COLOR_TEXT), rgb_to_hex(RGB(0, 0, 0)), 0.45f);
        editbox_wrap_draw_text(spec, layout->scroll_y, spec->placeholder, ph);
      }
    }
    if ((parts & EDITBOX_PART_CARET) && !spec->password_mode) {
      Platform_DrawRectRaw((DrawRectRawSpec){.x = layout->caret_x,
                                             .y = layout->caret_y,
                                             .w = layout->caret_w,
                                             .h = spec->caret_h,
                                             .color = rgb_to_hex(COLOR_CURSOR),
                                             .opaque = true,
                                             .thickness = 1,
                                             .filled = true});
      if (layout->caret_w > 1) {
        const char *src = spec->text;
        int srclen = spec->text_len;
        bool is_placeholder = false;
        if ((!src || srclen <= 0) && spec->placeholder &&
            spec->placeholder[0]) {
          src = spec->placeholder;
          srclen = (int)strlen(src);
          is_placeholder = true;
        }
        (void)is_placeholder;
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
          Platform_DrawTextRaw((DrawTextRawSpec){.x = layout->caret_x,
                                                 .y = layout->caret_text_y,
                                                 .text = ch,
                                                 .color = bg,
                                                 .font = spec->font,
                                                 .align = "",
                                                 .pad = 0,
                                                 .font_size = spec->font_size});
        }
      }
    }
    return;
  }

  if (parts & EDITBOX_PART_TINT) {
    unsigned long tint = color_lerp(bg, rgb_to_hex(COLOR_CURSOR), 0.72f);
    if (layout->draw_selection) {
      Platform_DrawRectRaw(
          (DrawRectRawSpec){.x = layout->sel_x0,
                            .y = spec->caret_y,
                            .w = layout->sel_x1 - layout->sel_x0,
                            .h = spec->caret_h,
                            .color = tint,
                            .opaque = true,
                            .thickness = 1,
                            .filled = true}

      );
    }
  }

  if (parts & EDITBOX_PART_TEXT) {
    if (spec->text_len > 0 && !spec->password_mode) {
      Platform_DrawTextRaw((DrawTextRawSpec){.x = layout->draw_x,
                                             .y = spec->text_y,
                                             .text = spec->text,
                                             .color = spec->text_color,
                                             .font = spec->font,
                                             .align = "",
                                             .pad = 0,
                                             .font_size = spec->font_size});
    } else if (spec->placeholder && spec->placeholder[0]) {
      unsigned long ph =
          color_lerp(rgb_to_hex(COLOR_TEXT), rgb_to_hex(RGB(0, 0, 0)), 0.45f);
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

static void editbox_registry_mouse(int *out_x, int *out_y, bool *out_down) {
  *out_x = -9999;
  *out_y = -9999;
  *out_down = false;
  int mx = 0, my = 0;
  if (!listbox_client_mouse_pos(&mx, &my))
    return;
  *out_x = mx;
  *out_y = my;
  *out_down = input_mouse_state_down(MOUSE_BTN_LEFT);
}

void editbox_registry_begin_frame(PromptState *state) {
  if (!state)
    return;
  for (int i = 0; i < state->editbox_count; i++)
    state->editboxes[i].placed = false;
}

void editbox_registry_begin(PromptState *state) {
  if (!state)
    return;
  if (state->active_editbox >= 0 &&
      state->active_editbox < state->editbox_count)
    state->eb = &state->editboxes[state->active_editbox].eb;
  else
    state->eb = state->default_eb;
}

PromptEditbox *editbox_registry_find(PromptState *state, const char *name) {
  if (!state || !name || !name[0])
    return NULL;
  for (int i = 0; i < state->editbox_count; i++)
    if (strcmp(state->editboxes[i].name, name) == 0)
      return &state->editboxes[i];
  return NULL;
}

EditBox *editbox_registry_lookup(PromptState *state, const char *name) {
  if (!state)
    return NULL;
  if (name && name[0]) {
    PromptEditbox *entry = editbox_registry_find(state, name);
    return entry ? &entry->eb : NULL;
  }
  return state->eb;
}

int editbox_registry_place(FnEditboxRegistryPlaceSpec *argspec) {
  if (!argspec->f_state || !argspec->f_name || !argspec->f_name[0])
    return -1;
  PromptEditbox *entry = editbox_registry_find(argspec->f_state, argspec->f_name);
  if (!entry) {
    if (argspec->f_state->editbox_count >= PROMPT_EDITBOX_MAX)
      return -1;
    entry = &argspec->f_state->editboxes[argspec->f_state->editbox_count++];
    memset(entry, 0, sizeof(*entry));
    entry->metrics = *metrics_get();
    snprintf(entry->name, sizeof(entry->name), "%s", argspec->f_name);
    entry->eb.sel_start = -1;
    entry->eb.sel_end = -1;
  }
  if (entry->buffer != argspec->f_buffer || entry->buffer_size != argspec->f_buffer_size) {
    entry->buffer = argspec->f_buffer;
    entry->buffer_size = argspec->f_buffer_size;
    size_t cap = argspec->f_buffer_size > 0 ? argspec->f_buffer_size : sizeof(entry->eb.filter);
    size_t n = argspec->f_buffer ? strnlen(argspec->f_buffer, cap - 1) : 0;
    if (n >= sizeof(entry->eb.filter))
      n = sizeof(entry->eb.filter) - 1;
    if (argspec->f_buffer)
      memcpy(entry->eb.filter, argspec->f_buffer, n);
    entry->eb.filter[n] = '\0';
    entry->eb.cursor = n;
    entry->eb.sel_start = -1;
    entry->eb.sel_end = -1;
    entry->eb.scroll_offset = 0;
    free(entry->eb.placeholder);
    entry->eb.placeholder = NULL;
  }
  if (argspec->f_w <= 0)
    argspec->f_w = argspec->f_state->width;
  if (argspec->f_h <= 0)
    argspec->f_h = prompt_custom_edit_height();
  entry->x = argspec->f_x;
  entry->y = argspec->f_y;
  entry->w = argspec->f_w;
  entry->h = argspec->f_h;
  entry->placed = true;
  int idx = (int)(entry - argspec->f_state->editboxes);
  return idx;
}

#define EDITBOX_FOCUS_PREFIX "editbox:"

static void editbox_focus_name(char *out, size_t out_size,
                               const PromptEditbox *entry) {
  snprintf(out, out_size, "%s%s", EDITBOX_FOCUS_PREFIX, entry->name);
}

static void editbox_focus_set(PromptState *state, int idx) {
  char name[160];
  editbox_focus_name(name, sizeof(name), &state->editboxes[idx]);
  state->active_editbox = idx;
  edit_state_end_selection();
  focus_set_pseudo_named(FOCUS_KIND_EDITBOX, name);
  editbox_registry_begin(state);
}

static void editbox_focus_activate(void *owner) {
  PromptState *state = (PromptState *)owner;
  const char *name = focus_name();
  if (!state || !name)
    return;
  const char *prefix = EDITBOX_FOCUS_PREFIX;
  if (strncmp(name, prefix, strlen(prefix)) != 0)
    return;
  PromptEditbox *entry = editbox_registry_find(state, name + strlen(prefix));
  if (!entry)
    return;
  editbox_focus_set(state, (int)(entry - state->editboxes));
}

void editbox_registry_register_focus(PromptState *state) {
  if (!state)
    return;
  for (int i = 0; i < state->editbox_count; i++) {
    PromptEditbox *entry = &state->editboxes[i];
    if (!entry->placed)
      continue;
    char name[160];
    editbox_focus_name(name, sizeof(name), entry);
    focus_register(&(FnFocusRegisterSpec){.f_name = name, .f_kind = FOCUS_KIND_EDITBOX, .f_x = entry->x, .f_y = entry->y, .f_w = entry->w, .f_h = entry->h, .f_owner = state, .f_activate = editbox_focus_activate, .f_adjust = NULL});
  }
}

void editbox_registry_set_active(PromptState *state, const char *name) {
  if (!state)
    return;
  if (!name || !name[0]) {
    state->active_editbox = -1;
  } else {
    PromptEditbox *entry = editbox_registry_find(state, name);
    if (!entry) {
      log_warn("editbox set active: unknown name '%s'", name);
      return;
    }
    editbox_focus_set(state, (int)(entry - state->editboxes));
    log_info("editbox set active: name='%s' idx=%d eb=%p",
             name && name[0] ? name : "(legacy)", state->active_editbox,
             (void *)state->eb);
    return;
  }
  edit_state_end_selection();
  focus_set_pseudo(FOCUS_KIND_EDITBOX);
  editbox_registry_begin(state);
  log_info("editbox set active: name='%s' idx=%d eb=%p",
           name && name[0] ? name : "(legacy)", state->active_editbox,
           (void *)state->eb);
}

void editbox_registry_cycle(PromptState *state, int dir) {
  log_info("editbox cycle: enter dir=%d count=%d active=%d", dir,
           state ? state->editbox_count : -1,
           state ? state->active_editbox : -1);
  if (!state || state->editbox_count <= 0)
    return;
  int cur = state->active_editbox;
  int step = dir >= 0 ? 1 : -1;
  for (int i = 0; i < state->editbox_count; i++) {
    cur = (cur + step + state->editbox_count) % state->editbox_count;
    if (!state->editboxes[cur].placed || state->editboxes[cur].no_tab)
      continue;
    editbox_focus_set(state, cur);
    log_info("editbox cycle: dir=%d active='%s' idx=%d", dir,
             state->editboxes[cur].name, cur);
    return;
  }
}

static void editbox_single_click_to_caret(PromptState *state,
                                          PromptEditbox *entry, int mx) {
  const char *text = entry->eb.filter;
  int len = (int)strlen(text);
  int font_size = entry->font_size > 0 ? entry->font_size : FontSize;
  int text_w = Platform_MeasureTextWidth(
      (MeasureTextWidthSpec){.text = text, .len = len, .font_size = font_size});
  int inset = text_w > 0 && entry->w < text_w + PROMPT_EDIT_SCROLL_PAD + 16
                  ? EDITBOX_TIGHT_INSET
                  : PROMPT_EDIT_INSET;
  int target_x = mx - entry->x - inset + entry->eb.scroll_offset;
  int best = 0;
  int best_dist = INT_MAX;
  for (int i = 0; i < len;) {
    if (text[i] == '\n') {
      i++;
      continue;
    }
    int glen = utf8_lead_len((unsigned char)text[i]);
    if (i + glen > len)
      glen = len - i;
    int before = Platform_MeasureTextWidth(
        (MeasureTextWidthSpec){.text = text, .len = i, .font_size = font_size});
    int after = Platform_MeasureTextWidth((MeasureTextWidthSpec){
        .text = text, .len = i + glen, .font_size = font_size});
    int before_dist = before - target_x;
    if (before_dist < 0)
      before_dist = -before_dist;
    int after_dist = after - target_x;
    if (after_dist < 0)
      after_dist = -after_dist;
    if (before_dist <= after_dist) {
      if (before_dist < best_dist) {
        best_dist = before_dist;
        best = i;
      }
    } else if (after_dist < best_dist) {
      best_dist = after_dist;
      best = i + glen;
    }
    i += glen;
  }
  edit_state_end_selection();
  state->eb->cursor = best;
}

static bool editbox_wrap_caret_from_point(PromptState *state,
                                          PromptEditbox *entry, int mx,
                                          int my) {
  if (!state || !entry || !entry->wrapped)
    return false;
  EditboxWrapCache *cache = &entry->wrap_cache;
  if (cache->count <= 0 || cache->line_h <= 0)
    return false;
  const char *text = entry->eb.filter;
  int len = (int)strlen(text);
  int max_width = entry->w > PROMPT_EDIT_SCROLL_PAD
                      ? entry->w - PROMPT_EDIT_SCROLL_PAD
                      : 40;
  uint64_t key = editbox_wrap_key(text, len, cache->line_h, max_width);
  if (cache->key != key || cache->len != len || cache->max_width != max_width)
    return false;
  int line_h = cache->line_h;
  int x0 = entry->x + PROMPT_EDIT_INSET;
  int y0 = (entry->y > 0 ? entry->y : PROMPT_EDIT_INSET) +
           Platform_EditboxPlaceholderOffset();
  int cursor = state->eb->cursor;
  if (cursor < 0)
    cursor = 0;
  if (cursor > len)
    cursor = len;
  int caret_idx = cursor < cache->count ? cursor : cache->count - 1;
  int caret_line = cache->positions[caret_idx].y / line_h;
  int line_count = 1;
  for (int i = 0; i < cache->count; i++) {
    int line = cache->positions[i].y / line_h;
    if (line >= line_count)
      line_count = line + 1;
  }
  int visible = entry->h > 0 ? entry->h / line_h : 1;
  if (visible < 1)
    visible = 1;
  int scroll_y =
      caret_line >= visible ? (caret_line - visible + 1) * line_h : 0;
  int target_y = my - y0 + scroll_y;
  int target_line = target_y / line_h;
  if (target_line < 0)
    target_line = 0;
  if (target_line >= line_count)
    target_line = line_count - 1;
  int target_x = mx - x0;
  int font_size = line_h > 4 ? line_h - 4 : FontSize;
  int best = 0;
  int best_dist = INT_MAX;
  for (int i = 0; i < cache->count && i < len; i++) {
    if (text[i] == '\n' || ((unsigned char)text[i] & 0xC0) == 0x80)
      continue;
    if (cache->positions[i].y / line_h != target_line)
      continue;
    int glen = utf8_lead_len((unsigned char)text[i]);
    if (i + glen > len)
      glen = len - i;
    int before = cache->positions[i].x;
    int after;
    int next = i + glen;
    if (next < cache->count &&
        cache->positions[next].y == cache->positions[i].y) {
      after = cache->positions[next].x;
    } else {
      after =
          before + Platform_MeasureTextWidth((MeasureTextWidthSpec){
                       .text = text + i, .len = glen, .font_size = font_size});
    }
    int dx = before - target_x;
    int dy = cache->positions[i].y - target_y;
    int dist = dx * dx + dy * dy;
    if (dist < best_dist) {
      best_dist = dist;
      best = i;
    }
    dx = after - target_x;
    dist = dx * dx + dy * dy;
    if (dist < best_dist) {
      best_dist = dist;
      best = next;
    }
  }
  edit_state_end_selection();
  state->eb->cursor = best;
  return true;
}

void editbox_registry_tick(PromptState *state) {
  if (!state)
    return;
  int mx = 0, my = 0;
  bool down = false;
  editbox_registry_mouse(&mx, &my, &down);
  if (state->active_editbox < 0 && state->editbox_count > 0) {
    editbox_focus_set(state, 0);
  }
  for (int i = 0; i < state->editbox_count; i++) {
    PromptEditbox *entry = &state->editboxes[i];
    if (!entry->placed)
      continue;
    bool over = mx >= entry->x && mx < entry->x + entry->w && my >= entry->y &&
                my < entry->y + entry->h;
    if (down && !entry->was_down && over) {
      editbox_focus_set(state, i);
      if (entry->wrapped)
        editbox_wrap_caret_from_point(state, entry, mx, my);
      else
        editbox_single_click_to_caret(state, entry, mx);
      log_info("editbox tick: clicked '%s' idx=%d rect=%d,%d %dx%d",
               entry->name, i, entry->x, entry->y, entry->w, entry->h);
    }
    entry->was_down = down;
  }
  editbox_registry_register_focus(state);
  editbox_registry_begin(state);
}

void editbox_legacy_tick(FnEditboxLegacyTickSpec *argspec) {
  if (!argspec->f_state || !argspec->f_state->eb)
    return;
  int idx = editbox_registry_place(&(FnEditboxRegistryPlaceSpec){.f_state = argspec->f_state, .f_name = PROMPT_PRIMARY_EDITBOX_NAME, .f_buffer = argspec->f_state->eb->filter, .f_buffer_size = sizeof(argspec->f_state->eb->filter), .f_x = argspec->f_x, .f_y = argspec->f_y, .f_w = argspec->f_w, .f_h = argspec->f_h});
  if (idx < 0)
    return;
  PromptEditbox *entry = &argspec->f_state->editboxes[idx];
  entry->font_size = argspec->f_font_size > 0 ? argspec->f_font_size : FontSize;
  entry->primary = true;
  entry->eb = *argspec->f_state->eb;
  EditBox *saved_eb = argspec->f_state->eb;
  int saved_active = argspec->f_state->active_editbox;
  argspec->f_state->eb = &entry->eb;
  argspec->f_state->active_editbox = idx;
  editbox_registry_tick(argspec->f_state);
  *saved_eb = *argspec->f_state->eb;
  argspec->f_state->eb = saved_eb;
  argspec->f_state->active_editbox = saved_active;
}

void editbox_registry_sync(PromptState *state) {
  if (!state)
    return;
  for (int i = 0; i < state->editbox_count; i++) {
    PromptEditbox *entry = &state->editboxes[i];
    if (!entry->buffer)
      continue;
    size_t cap =
        entry->buffer_size > 0 ? entry->buffer_size : sizeof(entry->eb.filter);
    size_t n = strnlen(entry->eb.filter, cap - 1);
    memcpy(entry->buffer, entry->eb.filter, n);
    entry->buffer[n] = '\0';
  }
}

bool editbox_wrap_move_vertical(PromptState *state, int dir) {
  if (!state || !dir)
    return false;
  if (state->active_editbox < 0 ||
      state->active_editbox >= state->editbox_count)
    return false;
  PromptEditbox *entry = &state->editboxes[state->active_editbox];
  if (!entry->wrapped)
    return false;
  EditboxWrapCache *cache = &entry->wrap_cache;
  if (cache->count <= 0 || cache->line_h <= 0)
    return false;
  const char *text = entry->eb.filter;
  int len = (int)strlen(text);
  int max_width = entry->w > PROMPT_EDIT_SCROLL_PAD
                      ? entry->w - PROMPT_EDIT_SCROLL_PAD
                      : 40;
  uint64_t key = editbox_wrap_key(text, len, cache->line_h, max_width);
  if (cache->key != key || cache->len != len || cache->max_width != max_width)
    return false;
  int cursor = state->eb->cursor;
  if (cursor < 0)
    cursor = 0;
  if (cursor > len)
    cursor = len;
  int idx = cursor < cache->count ? cursor : cache->count - 1;
  int desired_x = cache->positions[idx].x;
  int current_line = cache->positions[idx].y / cache->line_h;
  int line_count = 1;
  for (int i = 0; i < cache->count; i++) {
    int line = cache->positions[i].y / cache->line_h;
    if (line >= line_count)
      line_count = line + 1;
  }
  int target_line = current_line + dir;
  if (target_line < 0 || target_line >= line_count)
    return false;
  int best = -1;
  int best_dist = INT_MAX;
  int font_size = cache->line_h > 4 ? cache->line_h - 4 : FontSize;
  for (int i = 0; i < cache->count && i < len; i++) {
    if (text[i] == '\n' || ((unsigned char)text[i] & 0xC0) == 0x80)
      continue;
    if (cache->positions[i].y / cache->line_h != target_line)
      continue;
    int glen = utf8_lead_len((unsigned char)text[i]);
    if (i + glen > len)
      glen = len - i;
    int before = cache->positions[i].x;
    int after;
    int next = i + glen;
    if (next < cache->count &&
        cache->positions[next].y == cache->positions[i].y) {
      after = cache->positions[next].x;
    } else {
      after =
          before + Platform_MeasureTextWidth((MeasureTextWidthSpec){
                       .text = text + i, .len = glen, .font_size = font_size});
    }
    int before_dist = before - desired_x;
    if (before_dist < 0)
      before_dist = -before_dist;
    int after_dist = after - desired_x;
    if (after_dist < 0)
      after_dist = -after_dist;
    int cand = before_dist <= after_dist ? i : next;
    int dist = before_dist <= after_dist ? before_dist : after_dist;
    if (dist < best_dist) {
      best_dist = dist;
      best = cand;
    }
  }
  if (best < 0)
    return false;
  edit_state_end_selection();
  state->eb->cursor = best;
  return true;
}

const char *editbox_registry_active_name(const PromptState *state) {
  if (!state || state->active_editbox < 0 ||
      state->active_editbox >= state->editbox_count)
    return NULL;
  return state->editboxes[state->active_editbox].name;
}
