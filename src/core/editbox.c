#include "helpers/helpers_text.h"
#include "core/edit_state.h"
#include "core/editbox.h"

#include <stdio.h>
#include <string.h>

#include "core/context.h"
#include "core/focus.h"
#include "core/window_manager.h"
#include "helpers/helpers_input_state.h"
#include "core/prompt_geometry.h"
#include "core/types/window.h"
#include "helpers/helpers_ui.h"
#include "platform/draw.h"
#include "platform/platform.h"
#include "deps/log.h"
#include "config_values.h"

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
  out->metrics = editbox_state_metrics(in->state);
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
  out->caret_h = in->caret_h > 0 ? in->caret_h : prompt_caret_height();
  out->caret_w = in->caret_w;
  out->caret_x = in->caret_x;
  out->caret_pos = in->caret_pos;
  out->caret_h = out->caret_h;
  out->placeholder = in->placeholder
                         ? in->placeholder
                         : (in->state ? in->state->eb->placeholder : NULL);
  out->placeholder_dy = Platform_EditboxPlaceholderOffset();
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
  spec->metrics = editbox_state_metrics(state);
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
  spec->placeholder_dy = Platform_EditboxPlaceholderOffset();
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
  const Metrics *metrics = spec->metrics ? spec->metrics : metrics_get();
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

static void editbox_registry_mouse(int *out_x, int *out_y, bool *out_down) {
  *out_x = -9999;
  *out_y = -9999;
  *out_down = false;
  int mx = 0, my = 0;
  if (!Platform_GetMousePos(&mx, &my))
    return;
  int wx = 0, wy = 0;
  WindowManager *wm = WindowManager_Get();
  Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
  bool have_origin = false;
  if (top)
    have_origin = Platform_WindowToRoot(top->handle, &wx, &wy);
  if (!have_origin)
    have_origin =
        Platform_WindowToRoot(SkaldContext_Get()->window.hPopupWnd, &wx, &wy);
  if (have_origin) {
    mx -= wx;
    my -= wy;
  }
  int dx = 0, dy = 0;
  get_draw_offset(&dx, &dy);
  mx -= dx;
  my -= dy;
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

int editbox_registry_place(PromptState *state, const char *name, char *buffer,
                           size_t buffer_size, int x, int y, int w, int h) {
  if (!state || !name || !name[0])
    return -1;
  PromptEditbox *entry = editbox_registry_find(state, name);
  if (!entry) {
    if (state->editbox_count >= PROMPT_EDITBOX_MAX)
      return -1;
    entry = &state->editboxes[state->editbox_count++];
    memset(entry, 0, sizeof(*entry));
    entry->metrics = *metrics_get();
    snprintf(entry->name, sizeof(entry->name), "%s", name);
    entry->eb.sel_start = -1;
    entry->eb.sel_end = -1;
  }
  if (entry->buffer != buffer || entry->buffer_size != buffer_size) {
    entry->buffer = buffer;
    entry->buffer_size = buffer_size;
    size_t cap = buffer_size > 0 ? buffer_size : sizeof(entry->eb.filter);
    size_t n = buffer ? strnlen(buffer, cap - 1) : 0;
    if (n >= sizeof(entry->eb.filter))
      n = sizeof(entry->eb.filter) - 1;
    if (buffer)
      memcpy(entry->eb.filter, buffer, n);
    entry->eb.filter[n] = '\0';
    entry->eb.cursor = n;
    entry->eb.sel_start = -1;
    entry->eb.sel_end = -1;
    entry->eb.scroll_offset = 0;
    entry->eb.placeholder = NULL;
  }
  if (w <= 0)
    w = state->width;
  if (h <= 0)
    h = prompt_custom_edit_height();
  entry->x = x;
  entry->y = y;
  entry->w = w;
  entry->h = h;
  entry->placed = true;
  int idx = (int)(entry - state->editboxes);
  return idx;
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
    state->active_editbox = (int)(entry - state->editboxes);
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
    if (!state->editboxes[cur].placed)
      continue;
    state->active_editbox = cur;
    edit_state_end_selection();
    focus_set_pseudo(FOCUS_KIND_EDITBOX);
    editbox_registry_begin(state);
    log_info("editbox cycle: dir=%d active='%s' idx=%d", dir,
             state->editboxes[cur].name, cur);
    return;
  }
}

void editbox_registry_tick(PromptState *state) {
  if (!state)
    return;
  int mx = 0, my = 0;
  bool down = false;
  editbox_registry_mouse(&mx, &my, &down);
  if (state->active_editbox < 0 && state->editbox_count > 0) {
    state->active_editbox = 0;
    focus_set_pseudo(FOCUS_KIND_EDITBOX);
  }
  for (int i = 0; i < state->editbox_count; i++) {
    PromptEditbox *entry = &state->editboxes[i];
    if (!entry->placed)
      continue;
    bool over = mx >= entry->x && mx < entry->x + entry->w && my >= entry->y &&
                my < entry->y + entry->h;
    if (down && !entry->was_down && over) {
      state->active_editbox = i;
      edit_state_end_selection();
      focus_set_pseudo(FOCUS_KIND_EDITBOX);
      log_info("editbox tick: clicked '%s' idx=%d rect=%d,%d %dx%d",
               entry->name, i, entry->x, entry->y, entry->w, entry->h);
    }
    entry->was_down = down;
  }
  editbox_registry_begin(state);
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

const char *editbox_registry_active_name(const PromptState *state) {
  if (!state || state->active_editbox < 0 ||
      state->active_editbox >= state->editbox_count)
    return NULL;
  return state->editboxes[state->active_editbox].name;
}
