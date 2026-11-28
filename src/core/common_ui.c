#include "helpers/helpers_text.h"
#include "core/timer.h"
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "core/context.h"
#include "core/types/prompt.h"
#include "deps/log.h"
#include "platform/platform.h"
#include "helpers/helpers_ui.h"
#include "core/listbox_hover.h"
#include "config_values.h"
#include "core/types.h"
#include "core/types/color.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#include "deps/stb_ds.h"
#pragma GCC diagnostic pop
#include "core/window_manager.h"
#include "core/prompt_geometry.h"
#include "core/draw_state.h"
#include "platform/draw.h"
#include "helpers/helpers_input_state.h"
#include "platform/wm_platform.h"

bool cover_scale(FnCoverScaleSpec *argspec) {
  double scale = (double)argspec->f_tw / (double)argspec->f_sw;
  double scale_y = (double)argspec->f_th / (double)argspec->f_sh;
  if (scale_y > scale)
    scale = scale_y;
  if (scale <= 0.0)
    return 0;
  int w = (int)((double)argspec->f_sw * scale + 0.5);
  int hgt = (int)((double)argspec->f_sh * scale + 0.5);
  if (w < 1)
    w = 1;
  if (hgt < 1)
    hgt = 1;
  if (argspec->f_dw)
    *argspec->f_dw = w;
  if (argspec->f_dh)
    *argspec->f_dh = hgt;
  return 1;
}

Metrics *metrics_get(void) {
  static Metrics m = {0};
  static atomic_int cached_font_size = -1;
  int font_size = FontSize;
  if (atomic_load_explicit(&cached_font_size, memory_order_relaxed) !=
      font_size) {
    m.edit_cursor_width = 0;
    m.edit_cursor_height = prompt_caret_height();
    m.editbox_padding = PROMPT_EDIT_INSET;
    m.listbox_padding = prompt_list_bottom_margin();
    m.listbox_item_height = prompt_row_height();
    atomic_store_explicit(&cached_font_size, font_size, memory_order_relaxed);
  }
  return &m;
}

void draw_prompt_separator(DrawPromptSeparatorSpec spec) {
  if (!EnableBorders || spec.width <= 0)
    return;
  if (draw_state_get()->suppress_separators)
    return;
  Platform_DrawRectRaw((DrawRectRawSpec){.x = 0,
                                         .y = spec.y,
                                         .w = spec.width,
                                         .h = PROMPT_SEP_W,
                                         .color = rgb_to_hex(COLOR_BORDER),
                                         .radius = 0,
                                         .rounded = false,
                                         .opaque = true,
                                         .thickness = 0,
                                         .filled = true});
}

bool prompt_band_take_placed(void) {
  SkaldContext *ctx = SkaldContext_Get();
  ctx->window.editbox_placed = false;
  ctx->window.listbox_placed = false;
  return ctx->window.band_active;
}

void prompt_band_commit_placed(void) {
  SkaldContext *ctx = SkaldContext_Get();
  ctx->window.band_active = ctx->window.editbox_placed;
}

static bool fuzzy_match(const char *text, const char *pattern) {
  if (!pattern || !pattern[0])
    return true;
  if (!text || !text[0])
    return false;
  while (*pattern && *text) {
    if (tolower((unsigned char)*pattern) == tolower((unsigned char)*text))
      pattern++;
    text++;
  }
  return *pattern == '\0';
}

static int levenshtein(const char *a, const char *b) {
  int la = (int)strlen(a), lb = (int)strlen(b);
  if (abs(la - lb) > 2)
    return 100;
  if (la > 255 || lb > 255)
    return 100;
  int dp[3][256];
  for (int j = 0; j <= lb && j < 256; j++)
    dp[0][j] = j;
  for (int i = 1; i <= la && i < 256; i++) {
    int cur = i % 3, prev = (i + 2) % 3, prev2 = (i + 1) % 3;
    dp[cur][0] = i;
    for (int j = 1; j <= lb && j < 256; j++) {
      int cost =
          tolower((unsigned char)a[i - 1]) == tolower((unsigned char)b[j - 1])
              ? 0
              : 1;
      int v = dp[prev][j] + 1;
      if (dp[cur][j - 1] + 1 < v)
        v = dp[cur][j - 1] + 1;
      if (dp[prev][j - 1] + cost < v)
        v = dp[prev][j - 1] + cost;
      if (i > 1 && j > 1 &&
          tolower((unsigned char)a[i - 1]) ==
              tolower((unsigned char)b[j - 2]) &&
          tolower((unsigned char)a[i - 2]) ==
              tolower((unsigned char)b[j - 1])) {
        if (dp[prev2][j - 2] + 1 < v)
          v = dp[prev2][j - 2] + 1;
      }
      dp[cur][j] = v;
    }
  }
  return dp[la % 3][lb];
}

static bool fuzzy_match_with_typo(const char *text, const char *pattern) {
  if (fuzzy_match(text, pattern))
    return true;
  if (!text || !pattern)
    return false;
  if (strlen(pattern) < 3)
    return false;
  int dist = levenshtein(text, pattern);
  if (dist <= 1)
    return true;
  if (strlen(pattern) >= 4 && dist == 2)
    return true;
  return false;
}

static int fuzzy_score(const char *text, const char *pattern) {
  if (!pattern || !pattern[0])
    return 0;
  if (!text || !text[0])
    return 1000000;
  int score = 0;
  int last = -1;
  int consecutive = 0;
  int pos = 0;
  int tlen = (int)strlen(text);
  for (int i = 0; pattern[i]; i++) {
    char pc = tolower((unsigned char)pattern[i]);
    bool found = false;
    for (int j = pos; text[j]; j++) {
      if (tolower((unsigned char)text[j]) == pc) {
        if (last >= 0) {
          int gap = j - last - 1;
          score += gap * 5;
          if (gap == 0)
            consecutive++;
        } else {
          score += j * 2;
        }
        if (j == 0 || text[j - 1] == ' ' || text[j - 1] == '/' ||
            text[j - 1] == '_' || text[j - 1] == '-' || text[j - 1] == '.')
          score -= 3;
        last = j;
        pos = j + 1;
        found = true;
        break;
      }
    }
    if (!found)
      return 1000000;
  }
  score -= consecutive * 4;
  score += tlen / 8;
  return score;
}

static void populate_capture_selection(ListBox *lb, char *out_text,
                                       size_t out_size, int *out_sel) {
  int current_sel = lb->selected_index;
  if (out_sel)
    *out_sel = current_sel;
  if (out_text && out_size > 0) {
    out_text[0] = '\0';
    const char *cur = listbox_info_row(lb, current_sel);
    lb->selected_text = cur;
    if (cur) {
      safe_copy(out_text, out_size, cur);
    }
  }
}

static void populate_clean_filter(const char *filter, char *clean_filter,
                                  size_t size) {
  clean_filter[0] = '\0';
  if (!filter)
    return;
  while (*filter && isspace((unsigned char)*filter))
    filter++;
  size_t j = 0;
  bool last_was_space = false;
  for (size_t i = 0; filter[i] && j + 1 < size; i++) {
    char c = filter[i];
    if (isspace((unsigned char)c)) {
      if (!last_was_space) {
        clean_filter[j++] = ' ';
        last_was_space = true;
      }
    } else {
      clean_filter[j++] = c;
      last_was_space = false;
    }
  }
  while (j > 0 && clean_filter[j - 1] == ' ')
    j--;
  clean_filter[j] = '\0';
}

static void normalize_for_match(const char *src, char *dst, size_t dst_size) {
  if (!src || !dst || dst_size == 0) {
    if (dst && dst_size)
      dst[0] = '\0';
    return;
  }
  while (*src && isspace((unsigned char)*src))
    src++;
  size_t j = 0;
  bool last_was_space = false;
  for (size_t i = 0; src[i] && j + 1 < dst_size; i++) {
    char c = src[i];
    if (isspace((unsigned char)c)) {
      if (!last_was_space) {
        dst[j++] = ' ';
        last_was_space = true;
      }
    } else {
      dst[j++] = c;
      last_was_space = false;
    }
  }
  while (j > 0 && dst[j - 1] == ' ')
    j--;
  dst[j] = '\0';
}

static bool
populate_item_matches(const char *search_text, const char *clean_filter,
                      bool (*filter_match)(const char *, const char *)) {
  if (filter_match)
    return filter_match(search_text, clean_filter);
  char norm_search[1024];
  normalize_for_match(search_text, norm_search, sizeof(norm_search));
  if (string_starts_with_ci(SkaldContext_Get()->window.current_feature,
                            "center")) {
    return fuzzy_match_with_typo(norm_search, clean_filter);
  }
  return string_contains_ci(norm_search, clean_filter);
}

static int populate_filter(ListBox *lb, const char **items, int item_count,
                           const char *clean_filter,
                           bool (*filter_match)(const char *, const char *)) {
  int total_matches = 0;
  if (!listbox_info_ensure_matches(lb, item_count))
    return 0;
  bool empty_filter = (clean_filter[0] == '\0');
  for (int i = 0; i < item_count; i++) {
    if (!items[i])
      continue;
    if (empty_filter) {
      lb->matching_indices[total_matches++] = i;
      continue;
    }
    if (populate_item_matches(items[i], clean_filter, filter_match)) {
      lb->matching_indices[total_matches++] = i;
    }
  }
  return total_matches;
}

static void populate_rank_matches(ListBox *lb, const char **items,
                                  const char *clean_filter, int total_matches,
                                  int (*item_rank)(int item_index)) {
  if (clean_filter[0] == '\0' || total_matches <= 1)
    return;
  int *scores = (int *)malloc((size_t)total_matches * sizeof(*scores));
  if (!scores)
    return;
  for (int i = 0; i < total_matches; i++) {
    int idx = lb->matching_indices[i];
    const char *txt = items[idx] ? items[idx] : "";
    if (fuzzy_match(txt, clean_filter))
      scores[i] = fuzzy_score(txt, clean_filter);
    else
      scores[i] = 500 + levenshtein(txt, clean_filter) * 20;
  }
  for (int i = 0; i < total_matches - 1; i++)
    for (int j = i + 1; j < total_matches; j++) {
      int ri = item_rank ? item_rank(lb->matching_indices[i]) : 0;
      int rj = item_rank ? item_rank(lb->matching_indices[j]) : 0;
      if (rj < ri || (rj == ri && scores[j] < scores[i])) {
        int tm = lb->matching_indices[i];
        lb->matching_indices[i] = lb->matching_indices[j];
        lb->matching_indices[j] = tm;
        int ts = scores[i];
        scores[i] = scores[j];
        scores[j] = ts;
      }
    }
  free(scores);
}

static void populate_ensure_matches(ListBox *lb, int item_count,
                                    const char *clean_filter,
                                    int *total_matches) {
  if (*total_matches == 0 && item_count > 0 && strlen(clean_filter) == 0) {
    int force_count = (item_count < 30) ? item_count : 30;
    if (!listbox_info_ensure_matches(lb, force_count))
      return;
    for (int i = 0; i < force_count; i++) {
      lb->matching_indices[*total_matches] = i;
      (*total_matches)++;
    }
  }
}

static void populate_build_cache(ListBox *lb, const char **items,
                                 int total_matches) {
  lb->item_count = 0;
  if (total_matches <= 0)
    return;
  if (!listbox_info_ensure_rows(lb, total_matches)) {
    lb->item_count = 0;
    return;
  }
  for (int i = 0; i < total_matches; i++) {
    int raw_data_index = lb->matching_indices[i];
    if (!items[raw_data_index])
      continue;
    if (!listbox_info_set_row(lb, lb->item_count, items[raw_data_index]))
      continue;
    lb->item_count++;
  }
}

static void populate_update_ui(ListBox *lb, int total_matches,
                               const char *filter) {
  UI_LB_SetCount(lb, total_matches);
  bool has_filter = filter && filter[0] != '\0';
  size_t flen = filter ? strlen(filter) : 0;
  size_t cmp =
      flen + 1 < sizeof(lb->last_filter) ? flen + 1 : sizeof(lb->last_filter);
  bool same_filter = has_filter && strncmp(lb->last_filter, filter, cmp) == 0;
  if (total_matches != 0 && has_filter && !same_filter) {
    UI_SetSelection(lb, 0);
  } else if (lb->selected_index >= total_matches) {
    UI_SetSelection(lb, total_matches > 0 ? total_matches - 1 : -1);
  } else if (lb->selected_index >= 0) {
    UI_NavigateTo(lb, lb->selected_index);
  } else {
    UI_SetSelection(lb, -1);
  }
  lb->selected_text = listbox_info_row(lb, lb->selected_index);
  if (filter)
    safe_copy(lb->last_filter, sizeof(lb->last_filter), filter);
  else
    lb->last_filter[0] = '\0';
  listbox_scroll_to_selection(lb);
  if (total_matches == 0) {
    Platform_DismissDisplayMessage();
  }
}

void populate_listbox(ListBox *lb, const char *filter, const char **items,
                      int item_count,
                      bool (*filter_match)(const char *item,
                                           const char *filter)) {
  populate_listbox_ranked(&(FnPopulateListboxRankedSpec){.f_lb = lb, .f_filter = filter, .f_items = items, .f_item_count = item_count, .f_filter_match = filter_match, .f_item_rank = NULL});
}

void populate_listbox_ranked(FnPopulateListboxRankedSpec *argspec) {
  if (!argspec->f_lb)
    return;

  UI_LB_BeginUpdate();
  if (!argspec->f_filter)
    argspec->f_filter = "";
  int current_sel;
  char current_text[256];
  populate_capture_selection(argspec->f_lb, current_text, sizeof(current_text),
                             &current_sel);

  char clean_filter[256];
  populate_clean_filter(argspec->f_filter, clean_filter, sizeof(clean_filter));
  int total_matches = 0;

  total_matches =
      populate_filter(argspec->f_lb, argspec->f_items, argspec->f_item_count, clean_filter, argspec->f_filter_match);

  populate_rank_matches(argspec->f_lb, argspec->f_items, clean_filter, total_matches, argspec->f_item_rank);

  populate_ensure_matches(argspec->f_lb, argspec->f_item_count, clean_filter, &total_matches);

  populate_build_cache(argspec->f_lb, argspec->f_items, total_matches);

  populate_update_ui(argspec->f_lb, total_matches, argspec->f_filter);

  argspec->f_lb->total_matches = total_matches;

  log_info("populate_listbox completed for filter='%s'", argspec->f_filter ? argspec->f_filter : "");
  UI_LB_EndUpdate();
}

SlotRect message_slot_to_rect(MessageSlot slot, int prompt_gap) {
  SlotRect out;
  int h_slot = 1;
  int v_slot = 1;
  int gap = MessageScreenGap;
  int outward = 0;
  int x = 0, y = 0, w = 0, h = 0;
  int mox = 0, moy = 0, mow = 0, moh = 0;
  WindowManager_GetMainMonitorGeometry(&mox, &moy, &mow, &moh);
  if (mow <= 0)
    mow = 1920;
  if (moh <= 0)
    moh = 1080;
  if (slot >= TOP_LEFT_RELATIVE && slot <= BOTTOM_RIGHT_RELATIVE) {
    if (prompt_gap > 0)
      gap = prompt_gap;
    outward = 1;
    WindowManager_GetPromptBounds(&x, &y, &w, &h);
    if (w <= 0 || h <= 0) {
      x = mox;
      y = moy;
      w = mow;
      h = moh;
    }
    slot = (MessageSlot)(slot - TOP_LEFT_RELATIVE + TOP_LEFT);
  } else {
    x = mox;
    y = moy;
    w = mow;
    h = moh;
  }
  if (slot >= TOP_LEFT && slot <= BOTTOM_RIGHT) {
    int idx = slot - TOP_LEFT;
    h_slot = idx % 3;
    v_slot = idx / 3;
  }
  out.x = x;
  out.y = y;
  out.w = w;
  out.h = h;
  out.h_slot = h_slot;
  out.v_slot = v_slot;
  out.gap = gap;
  out.outward = outward;
  return out;
}

void display_message_slot(const MessageSlotSpec *spec) {
  if (!spec)
    return;
  const char *message = spec->message;
  void (*render)(void *ud) = spec->render;
  if (!message)
    message = "";
  if (!message[0] && !render)
    return;
  SlotRect r = message_slot_to_rect(spec->slot, spec->prompt_gap);
  Platform_DisplayMessageAnchored(
      (AnchoredMessageSpec){.message = message,
                            .x = r.x,
                            .y = r.y,
                            .w = r.w,
                            .h = r.h,
                            .h_slot = r.h_slot,
                            .v_slot = r.v_slot,
                            .gap = r.gap,
                            .outward = r.outward,
                            .max_width = spec->max_w,
                            .max_height = spec->max_h,
                            .timeout = spec->timeout,
                            .color = spec->color,
                            .no_bg = spec->no_bg,
                            .render = render,
                            .render_data = spec->render_data,
                            .width = spec->win_w,
                            .height = spec->win_h,
                            .name = spec->name,
                            .rounded = spec->rounded,
                            .radius = spec->radius,
                            .fps = spec->fps});
}

void listbox_compute_layout(FnListboxComputeLayoutSpec *argspec) {

  if (!argspec->f_out) {
    return;
  }
  memset(argspec->f_out, 0, sizeof(*argspec->f_out));
  if (!argspec->f_spec) {
    return;
  }

  argspec->f_out->y0 = argspec->f_spec->y ? argspec->f_spec->y : argspec->f_default_y0;
  argspec->f_out->item_h = argspec->f_item_h > 0 ? argspec->f_item_h : 1;
  argspec->f_out->list_h =
      argspec->f_spec->h ? argspec->f_spec->h
              : (argspec->f_popup_h > 0 ? (argspec->f_popup_h - argspec->f_out->y0 - prompt_list_bottom_margin())
                             : PROMPT_CUSTOM_FALLBACK_LIST_H);
  if (!argspec->f_spec->x && !argspec->f_spec->w) {
    argspec->f_out->bx = PROMPT_SEL_X;
    argspec->f_out->bw = (argspec->f_popup_w > 0 ? argspec->f_popup_w : PROMPT_CUSTOM_FALLBACK_W) -
              PROMPT_SEL_W_SHRINK;
    argspec->f_out->fw = argspec->f_popup_w > 0 ? argspec->f_popup_w : PromptW;
    argspec->f_out->fh = argspec->f_popup_h > 0 ? argspec->f_popup_h : PromptH;
  } else {
    argspec->f_out->bx = argspec->f_spec->x + PROMPT_SEL_INSET;
    argspec->f_out->bw = (argspec->f_spec->w ? argspec->f_spec->w : PROMPT_CUSTOM_FALLBACK_LIST_W) - 2;
    argspec->f_out->fw = argspec->f_spec->w ? argspec->f_spec->w : PromptW;
    argspec->f_out->fh = argspec->f_spec->h ? argspec->f_spec->h : PromptH;
  }
  int visible = argspec->f_out->list_h / argspec->f_out->item_h;
  if (visible < 1)
    visible = 1;
  if (argspec->f_count > 0 && visible > argspec->f_count)
    visible = argspec->f_count;
  argspec->f_out->visible = visible;
}

bool terminal_guard(const char *working_dir, const char *extra_args) {
  if (!working_dir) {
    log_error("working_dir is NULL extra_args='%s'",
              extra_args ? extra_args : "null");
    return false;
  }
  if (!UsedTerminal || !UsedTerminal[0]) {
    log_error("UsedTerminal not configured working_dir='%s' extra_args='%s'",
              working_dir, extra_args ? extra_args : "null");
    return false;
  }
  return true;
}

void prompt_process_pending_subwindow(void) {
  SkaldContext *_c = SkaldContext_Get();
  if (!_c || !_c->window.subwindow.pending)
    return;
  _c->window.subwindow.pending = false;
  _c->window.subwindow.created = true;
  _c->window.selection_complete = false;
  WindowManager *wm = WindowManager_Get();
  if (wm && wm->popup_count > 1)
    WindowManager_PopPopup(wm);
  Popup *p = prompt_popup_create(wm, &_c->window.subwindow.pending_subwindow);
  if (p && p->vtable && p->vtable->on_render)
    p->vtable->on_render(p);
  Platform_PromptFlushDisplay(wm);
}

char *prompt_user_run(WindowManager *wm, const PromptSpec *spec) {
  SkaldContext *_ctx = SkaldContext_Get();
  if (_ctx)
    _ctx->window.subwindow.created = false;
  log_info("prompt_user_run started wm=%p spec=%p", wm, spec);
  if (!wm || !spec)
    return NULL;
  if (spec->subwindow) {
    SkaldContext *_c = SkaldContext_Get();
    if (_c && _c->window.subwindow.in_enter) {
      _c->window.subwindow.pending = true;
      _c->window.subwindow.pending_subwindow = *spec;
      _c->window.subwindow.pending_subwindow.subwindow = false;
      _c->window.subwindow.created = false;
      return NULL;
    }
    if (wm && wm->popup_count > 1) {
      return NULL;
    }
    PromptSpec heap_spec = *spec;
    heap_spec.subwindow = false;
    (void)prompt_popup_create(wm, &heap_spec);
    return NULL;
  }
  const Popup *popup = prompt_popup_create(wm, spec);
  if (!popup)
    return NULL;
  long long last_tick = 0;
  int last_mx = 0, last_my = 0;
  int last_btn = -1;
  while (wm->running) {
    bool _still_open = false;
    for (int _i = 0; _i < wm->popup_count; _i++) {
      if (wm->popups[_i] == popup) {
        _still_open = true;
        break;
      }
    }
    if (!_still_open)
      break;
    WindowManager_ProcessEvents(wm);
    bool mouse_changed = false;
    int mx = 0, my = 0;
    if (Platform_GetMousePos(&mx, &my)) {
      listbox_publish_mouse_pos(mx, my);
      int btn = input_mouse_state_down(MOUSE_BTN_LEFT) ? 1 : 0;
      if (mx != last_mx || my != last_my || btn != last_btn) {
        last_mx = mx;
        last_my = my;
        last_btn = btn;
        mouse_changed = true;
      }
    }
    Popup *mouse_top = WindowManager_GetTopPopup(wm);
    if (mouse_changed && mouse_top && mouse_top->vtable &&
        mouse_top->vtable->on_render) {
      mouse_top->vtable->on_render(mouse_top);
      Platform_PromptFlushDisplay(wm);
    }
    int fps = WindowManager_TopPopupFps(wm);
    if (fps > 0) {
      long long now = Platform_GetWallClockMs();
      long long interval = 1000 / fps;
      if (interval < 1)
        interval = 1;
      if (now - last_tick >= interval) {
        last_tick = now;
        WindowManager_RenderTick(wm);
        Platform_PromptFlushDisplay(wm);
      }
    }
  }
  {
    bool _still_open = false;
    for (int _i = 0; _i < wm->popup_count; _i++) {
      if (wm->popups[_i] == popup) {
        _still_open = true;
        break;
      }
    }
    if (_still_open) {
      const SkaldContext *_ctx2 = SkaldContext_Get();
      if (!_ctx2 || !_ctx2->window.subwindow.created)
        WindowManager_PopPopup(wm);
    }
  }
  Platform_PromptFlushDisplay(wm);
  SkaldContext *_c = SkaldContext_Get();
  return _c->window.output_buffer[0] ? _c->window.output_buffer : NULL;
}

int draw_thickness_px(float thickness) {
  if (thickness <= 0.0f)
    return 0;
  int px = (int)(thickness + 0.5f);
  if (px < 1)
    px = 1;
  if (px > 255)
    px = 255;
  return px;
}

void draw_measure_track(int x, int y, int w, int h) {
  draw_state_track_measure(x, y, w, h);
}

void update_text_caret(void) {
  WindowManager *wm = WindowManager_Get();
  Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
  if (top && top->vtable && top->vtable->on_render) {
    top->vtable->on_render(top);
    Platform_PromptFlushDisplay(wm);
  }
}

void set_transparent_window(bool on) { draw_state_get()->transparent = on; }

bool get_transparent_window(void) { return draw_state_get()->transparent; }

void set_draw_bg_color(unsigned long hex) {
  draw_state_get()->bg_override = hex;
}

unsigned long get_draw_bg_color(void) { return draw_state_get()->bg_override; }

void begin_measure(void) { draw_state_begin_measure(); }

void end_measure(int *out_w, int *out_h) {
  draw_state_end_measure(out_w, out_h);
}

void get_last_measure_origin(int *out_x, int *out_y) {
  if (out_x)
    *out_x = draw_state_get()->meas_origin_x;
  if (out_y)
    *out_y = draw_state_get()->meas_origin_y;
}

void set_draw_offset(int dx, int dy) {
  draw_state_get()->off_x = dx;
  draw_state_get()->off_y = dy;
}

void get_draw_offset(int *out_x, int *out_y) {
  if (out_x)
    *out_x = draw_state_get()->off_x;
  if (out_y)
    *out_y = draw_state_get()->off_y;
}

int draw_line_prep(FnDrawLinePrepSpec *argspec) {
  int t = draw_thickness_px(argspec->f_spec->thickness);
  if (t < 1)
    t = 1;
  *argspec->f_x1 = argspec->f_spec->x1 + argspec->f_off_x;
  *argspec->f_y1 = argspec->f_spec->y1 + argspec->f_off_y;
  *argspec->f_x2 = argspec->f_spec->x2 + argspec->f_off_x;
  *argspec->f_y2 = argspec->f_spec->y2 + argspec->f_off_y;
  int bx = *argspec->f_x1 < *argspec->f_x2 ? *argspec->f_x1 : *argspec->f_x2;
  int by = *argspec->f_y1 < *argspec->f_y2 ? *argspec->f_y1 : *argspec->f_y2;
  int bw = (*argspec->f_x1 > *argspec->f_x2 ? *argspec->f_x1 - *argspec->f_x2 : *argspec->f_x2 - *argspec->f_x1) + t;
  int bh = (*argspec->f_y1 > *argspec->f_y2 ? *argspec->f_y1 - *argspec->f_y2 : *argspec->f_y2 - *argspec->f_y1) + t;
  draw_measure_track(bx - t / 2, by - t / 2, bw, bh);
  return t;
}

int draw_circle_prep(FnDrawCirclePrepSpec *argspec) {
  int t = draw_thickness_px(argspec->f_spec->thickness);
  *argspec->f_cx = argspec->f_spec->cx + argspec->f_off_x;
  *argspec->f_cy = argspec->f_spec->cy + argspec->f_off_y;
  draw_measure_track(*argspec->f_cx - argspec->f_spec->radius, *argspec->f_cy - argspec->f_spec->radius, argspec->f_spec->radius * 2,
                     argspec->f_spec->radius * 2);
  return t;
}

int bg_tile_lru_pick(FnBgTileLruPickSpec *argspec) {
  int i;
  for (i = 0; i < argspec->f_count; i++)
    if (argspec->f_is_empty((const char *)argspec->f_slots + (size_t)i * argspec->f_stride))
      return i;

  int oldest = 0;
  for (i = 1; i < argspec->f_count; i++) {
    const void *cand = (const char *)argspec->f_slots + (size_t)i * argspec->f_stride;
    const void *best = (const char *)argspec->f_slots + (size_t)oldest * argspec->f_stride;
    if (argspec->f_stamp(cand) < argspec->f_stamp(best))
      oldest = i;
  }
  return oldest;
}

void draw_text_centered_impl(const DrawTextCenteredSpec *spec,
                             int line_height) {
  int tw = Platform_MeasureTextWidth(
      (MeasureTextWidthSpec){.text = spec->text,
                             .len = (int)strlen(spec->text),
                             .font_size = spec->font_size});
  int tx = spec->x + (spec->w - tw) / 2;
  int ty = spec->y + (spec->h - line_height) / 2;
  Platform_DrawTextRaw((DrawTextRawSpec){.x = tx,
                                         .y = ty,
                                         .text = spec->text,
                                         .color = spec->color,
                                         .font = spec->font,
                                         .align = "LEFT",
                                         .pad = 0,
                                         .font_size = spec->font_size});
}

int msg_anchor_center(FnMsgAnchorCenterSpec *argspec) {
  if (argspec->f_slot == 0)
    return argspec->f_outward ? argspec->f_rect - argspec->f_gap - argspec->f_win_size / 2 : argspec->f_rect + argspec->f_gap + argspec->f_win_size / 2;
  if (argspec->f_slot == 2)
    return argspec->f_outward ? argspec->f_rect + argspec->f_rect_size + argspec->f_gap + argspec->f_win_size / 2
                   : argspec->f_rect + argspec->f_rect_size - argspec->f_gap - argspec->f_win_size / 2;
  return argspec->f_rect + argspec->f_rect_size / 2;
}

int msg_stack_dir(int v_slot, bool outward) {
  int dir = (v_slot == 2) ? -1 : 1;
  return outward ? -dir : dir;
}

int prompt_row_height(void) { return FontSize + PROMPT_ROW_H_BONUS; }

int prompt_list_top(void) { return FontSize + PROMPT_LIST_TOP_BONUS; }

int prompt_caret_height(void) { return FontSize + PROMPT_CARET_H_BONUS; }

int prompt_custom_edit_height(void) {
  return FontSize + PROMPT_CUSTOM_EDIT_H_BONUS;
}

int prompt_list_bottom_margin(void) { return 40; }

int64_t timer_now_ticks(void) { return Platform_TimerNowTicks(); }

int64_t timer_tick_frequency(void) { return Platform_TimerTickFrequency(); }
