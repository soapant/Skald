#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "core/context.h"
#include "core/types/prompt.h"
#include "deps/log.h"
#include "platform/platform.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include "core/listbox_hover.h"
#include "config_values.h"
#include "core/types.h"
#include "core/types/color.h"
#include "deps/stb_ds.h"
#include "core/window_manager.h"
#include "core/prompt_geometry.h"
#include "platform/draw.h"
#include "platform/input_state.h"
#include "platform/wm_platform.h"

PlMetrics *Metrics_Get(void) {
  static PlMetrics m = {0};
  static int init = 0;
  if (!init) {
    m.edit_cursor_width = 0;
    m.edit_cursor_height = PromptCaretHeight();
    m.editbox_padding = PROMPT_EDIT_INSET;
    m.listbox_padding = PromptListBottomMargin();
    m.listbox_item_height = PromptRowHeight();
    init = 1;
  }
  return &m;
}

void DrawPromptSeparator(PlDrawPromptSeparatorSpec spec) {
  if (!EnableBorders || spec.width <= 0)
    return;
  Platform_DrawRectRaw((PlDrawRectRawSpec){.x = 0,
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

void Item_GetColors(PlGetItemColorsSpec spec) {
  unsigned long col_text = rgb_to_hex(COLOR_TEXT);
  float t = (spec.count > 1) ? (float)spec.idx / (float)(spec.count - 1)
                             : (float)spec.idx / 10.0f;
  unsigned long col_rainbow = rgb_to_hex(GetRainbowColor(t, 0.55f, 1.0f));
  unsigned long n = col_text, b = col_text, p = col_rainbow;
  if (RainbowText) {
    n = b = p = col_rainbow;
  }
  if (spec.num_color)
    *spec.num_color = n;
  if (spec.body_color)
    *spec.body_color = b;
  if (spec.plain_color)
    *spec.plain_color = p;
}

static bool FuzzyMatch(const char *text, const char *pattern) {
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
static int Levenshtein(const char *a, const char *b) {
  int la = (int)strlen(a), lb = (int)strlen(b);
  if (abs(la - lb) > 2)
    return 100;
  if (la > 255 || lb > 255)
    return 100;
  int dp[256][256];
  for (int i = 0; i <= la && i < 256; i++)
    dp[i][0] = i;
  for (int j = 0; j <= lb && j < 256; j++)
    dp[0][j] = j;
  for (int i = 1; i <= la && i < 256; i++)
    for (int j = 1; j <= lb && j < 256; j++) {
      int cost =
          tolower((unsigned char)a[i - 1]) == tolower((unsigned char)b[j - 1])
              ? 0
              : 1;
      int v = dp[i - 1][j] + 1;
      if (dp[i][j - 1] + 1 < v)
        v = dp[i][j - 1] + 1;
      if (dp[i - 1][j - 1] + cost < v)
        v = dp[i - 1][j - 1] + cost;
      if (i > 1 && j > 1 &&
          tolower((unsigned char)a[i - 1]) ==
              tolower((unsigned char)b[j - 2]) &&
          tolower((unsigned char)a[i - 2]) ==
              tolower((unsigned char)b[j - 1])) {
        if (dp[i - 2][j - 2] + 1 < v)
          v = dp[i - 2][j - 2] + 1;
      }
      dp[i][j] = v;
    }
  return dp[la][lb];
}
static bool FuzzyMatchWithTypo(const char *text, const char *pattern) {
  if (FuzzyMatch(text, pattern))
    return true;
  if (!text || !pattern)
    return false;
  if (strlen(pattern) < 3)
    return false;
  int dist = Levenshtein(text, pattern);
  if (dist <= 1)
    return true;
  if (strlen(pattern) >= 4 && dist == 2)
    return true;
  return false;
}
static int FuzzyScore(const char *text, const char *pattern) {
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

static void Populate_CaptureSelection(ListBoxInfo *lb, char *out_text,
                                      size_t out_size, int *out_sel) {
  int current_sel = lb->selected_index;
  if (out_sel)
    *out_sel = current_sel;
  if (out_text && out_size > 0) {
    out_text[0] = '\0';
    const char *cur = ListBoxInfo_Row(lb, current_sel);
    lb->selected_text = cur;
    if (cur) {
      strncpy(out_text, cur, out_size - 1);
      out_text[out_size - 1] = '\0';
    }
  }
}

static void Populate_CleanFilter(const char *filter, char *clean_filter,
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

static void NormalizeForMatch(const char *src, char *dst, size_t dst_size) {
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
Populate_ItemMatches(const char *search_text, const char *clean_filter,
                     bool (*filter_match)(const char *, const char *)) {
  if (filter_match)
    return filter_match(search_text, clean_filter);
  char norm_search[1024];
  NormalizeForMatch(search_text, norm_search, sizeof(norm_search));
  if (Platform_StringStartsWithCaseInsensitive(
          SkaldContext_Get()->window.current_feature, "center")) {
    return FuzzyMatchWithTypo(norm_search, clean_filter);
  }
  return Platform_StringContainsCaseInsensitive(norm_search, clean_filter);
}

static int Populate_Filter(ListBoxInfo *lb, const char **items, int item_count,
                           const char *clean_filter,
                           bool (*filter_match)(const char *, const char *)) {
  int total_matches = 0;
  bool empty_filter = (clean_filter[0] == '\0');
  for (int i = 0; i < item_count && total_matches < MAX_VISIBLE_ITEMS; i++) {
    if (!items[i])
      continue;
    if (empty_filter) {
      lb->matching_indices[total_matches++] = i;
      continue;
    }
    if (Populate_ItemMatches(items[i], clean_filter, filter_match)) {
      lb->matching_indices[total_matches++] = i;
    }
  }
  return total_matches;
}

static void Populate_RankMatches(ListBoxInfo *lb, const char **items,
                                 const char *clean_filter, int total_matches) {
  if (clean_filter[0] == '\0' || total_matches <= 1)
    return;
  int scores[MAX_VISIBLE_ITEMS];
  for (int i = 0; i < total_matches; i++) {
    int idx = lb->matching_indices[i];
    const char *txt = items[idx] ? items[idx] : "";
    if (FuzzyMatch(txt, clean_filter))
      scores[i] = FuzzyScore(txt, clean_filter);
    else
      scores[i] = 500 + Levenshtein(txt, clean_filter) * 20;
  }
  for (int i = 0; i < total_matches - 1; i++)
    for (int j = i + 1; j < total_matches; j++)
      if (scores[j] < scores[i]) {
        int ti = lb->matching_indices[i];
        lb->matching_indices[i] = lb->matching_indices[j];
        lb->matching_indices[j] = ti;
        int ts = scores[i];
        scores[i] = scores[j];
        scores[j] = ts;
      }
}

static void Populate_EnsureMatches(ListBoxInfo *lb, int item_count,
                                   const char *clean_filter,
                                   int *total_matches) {
  if (*total_matches == 0 && item_count > 0 && strlen(clean_filter) == 0) {
    int force_count = (item_count < 30) ? item_count : 30;
    for (int i = 0; i < force_count; i++) {
      lb->matching_indices[*total_matches] = i;
      (*total_matches)++;
    }
  }
  int pool_max = MAX_VISIBLE_ITEMS;
  if (*total_matches > pool_max)
    *total_matches = pool_max;
}

static void Populate_BuildCache(ListBoxInfo *lb, const char **items,
                                int total_matches) {
  if (total_matches > MAX_VISIBLE_ITEMS)
    total_matches = MAX_VISIBLE_ITEMS;
  lb->item_count = 0;
  if (total_matches <= 0)
    return;
  if (!ListBoxInfo_EnsureRows(lb, total_matches)) {
    lb->item_count = 0;
    return;
  }
  for (int i = 0; i < total_matches; i++) {
    int raw_data_index = lb->matching_indices[i];
    if (!items[raw_data_index])
      continue;
    strncpy(lb->items[lb->item_count], items[raw_data_index],
            LISTBOX_ROW_BYTES - 1);
    lb->items[lb->item_count][LISTBOX_ROW_BYTES - 1] = '\0';
    lb->items[lb->item_count][strcspn(lb->items[lb->item_count], "\r\n")] =
        '\0';
    lb->item_count++;
  }
}

static void Populate_UpdateUI(ListBoxInfo *lb, int total_matches,
                              const char *filter) {
  UI_LB_BeginUpdate();
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
  if (filter)
    strncpy(lb->last_filter, filter, sizeof(lb->last_filter) - 1);
  else
    lb->last_filter[0] = '\0';
  lb->last_filter[sizeof(lb->last_filter) - 1] = '\0';
  ListBox_ScrollToSelection(lb);
  if (total_matches == 0) {
    Platform_DismissDisplayMessage();
  }
  UI_LB_EndUpdate();
}

static void Populate_CommitState(ListBoxInfo *lb, int item_count,
                                 int total_matches) {
  lb->total_matches = total_matches;
}

void PopulateListBox(ListBoxInfo *lb, const char *filter, const char **items,
                     int item_count,
                     bool (*filter_match)(const char *item,
                                          const char *filter)) {
  if (!lb)
    return;

  if (!filter)
    filter = "";
  int current_sel;
  char current_text[256];
  Populate_CaptureSelection(lb, current_text, sizeof(current_text),
                            &current_sel);

  char clean_filter[256];
  Populate_CleanFilter(filter, clean_filter, sizeof(clean_filter));
  int total_matches = 0;

  total_matches =
      Populate_Filter(lb, items, item_count, clean_filter, filter_match);

  Populate_RankMatches(lb, items, clean_filter, total_matches);

  Populate_EnsureMatches(lb, item_count, clean_filter, &total_matches);

  Populate_BuildCache(lb, items, total_matches);

  Populate_UpdateUI(lb, total_matches, filter);

  Populate_CommitState(lb, item_count, total_matches);

  log_info("PopulateListBox completed for filter='%s'", filter ? filter : "");
}

ColorRef GetRainbowColor(float t, float s, float v) {
  if (RainbowText) {
    float hue = t * 240.0f;
    float h = hue / 60.0f;
    int i = ((int)h) % 6;
    float f = h - (int)h;
    float p = v * (1 - s);
    float q = v * (1 - s * f);
    float t_ = v * (1 - s * (1 - f));
    float r = 0.0f, g = 0.0f, b = 0.0f;
    switch (i) {
    case 0:
      r = v;
      g = t_;
      b = p;
      break;
    case 1:
      r = q;
      g = v;
      b = p;
      break;
    case 2:
      r = p;
      g = v;
      b = t_;
      break;
    case 3:
      r = p;
      g = q;
      b = v;
      break;
    case 4:
      r = t_;
      g = p;
      b = v;
      break;
    case 5:
      r = v;
      g = p;
      b = q;
      break;
    }
    return RGB((uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255));
  } else {
    return COLOR_TEXT;
  }
}

bool UI_IsListBoxSelected(ListBoxInfo *lb) {
  return lb && lb->selected_index != -1;
}


void MessageSlotToRect(MessageSlot slot, int prompt_gap, int *out_x, int *out_y,
                       int *out_w, int *out_h, int *out_h_slot, int *out_v_slot,
                       int *out_gap, int *out_outward) {
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
  if (out_x)
    *out_x = x;
  if (out_y)
    *out_y = y;
  if (out_w)
    *out_w = w;
  if (out_h)
    *out_h = h;
  if (out_h_slot)
    *out_h_slot = h_slot;
  if (out_v_slot)
    *out_v_slot = v_slot;
  if (out_gap)
    *out_gap = gap;
  if (out_outward)
    *out_outward = outward;
}

void DisplayMessageSlot(const char *message, MessageSlot slot, int max_w,
                        int max_h, int prompt_gap, double timeout,
                        ColorRef color, void (*render)(void),
                        const char *name) {
  if (!message || !message[0])
    return;
  int x = 0, y = 0, w = 0, h = 0, h_slot = 1, v_slot = 1, gap = 0, outward = 0;
  MessageSlotToRect(slot, prompt_gap, &x, &y, &w, &h, &h_slot, &v_slot, &gap,
                    &outward);
  Platform_DisplayMessageAnchored((PlAnchoredMessageSpec){.message = message,
                                                          .x = x,
                                                          .y = y,
                                                          .w = w,
                                                          .h = h,
                                                          .h_slot = h_slot,
                                                          .v_slot = v_slot,
                                                          .gap = gap,
                                                          .outward = outward,
                                                          .max_width = max_w,
                                                          .max_height = max_h,
                                                          .timeout = timeout,
                                                          .color = color,
                                                          .no_bg = false,
                                                          .render = render,
                                                          .name = name});
}

void Listbox_ComputeLayout(const PlListSpec *spec, int default_y0, int item_h,
                           int popup_w, int popup_h, int count,
                           ListboxLayout *out) {

  if (!out) {
    return;
  }
  memset(out, 0, sizeof(*out));
  if (!spec) {
    return;
  }

  out->y0 = spec->y ? spec->y : default_y0;
  out->item_h = item_h > 0 ? item_h : 1;
  out->list_h =
      spec->h ? spec->h
              : (popup_h > 0 ? (popup_h - out->y0 - PromptListBottomMargin())
                             : PROMPT_CUSTOM_FALLBACK_LIST_H);
  if (!spec->x && !spec->w) {
    out->bx = PROMPT_SEL_X;
    out->bw = (popup_w > 0 ? popup_w : PROMPT_CUSTOM_FALLBACK_W) -
              PROMPT_SEL_W_SHRINK;
    out->fw = popup_w > 0 ? popup_w : PROMPT_W;
    out->fh = popup_h > 0 ? popup_h : PROMPT_H;
  } else {
    out->bx = spec->x + PROMPT_SEL_INSET;
    out->bw = (spec->w ? spec->w : PROMPT_CUSTOM_FALLBACK_LIST_W) - 2;
    out->fw = spec->w ? spec->w : PROMPT_W;
    out->fh = spec->h ? spec->h : PROMPT_H;
  }
  int visible = out->list_h / out->item_h;
  if (visible < 1)
    visible = 1;
  if (count > 0 && visible > count)
    visible = count;
  out->visible = visible;
}

char *PromptUserRun(WindowManager *wm, const PromptSpec *spec) {
  SkaldContext *_ctx = SkaldContext_Get();
  if (_ctx)
    _ctx->window.subwindow.created = false;
  log_info("PromptUserRun started wm=%p spec=%p", wm, spec);
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
    (void)PromptPopup_Create(wm, &heap_spec);
    return NULL;
  }
  Popup *popup = PromptPopup_Create(wm, spec);
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
    Platform_WM_ProcessEvents(wm);
    bool mouse_changed = false;
    int mx = 0, my = 0;
    if (Platform_GetMousePos(&mx, &my)) {
      ListBox_PublishMousePos(mx, my);
      int btn = Platform_MouseStateDown(MOUSE_BTN_LEFT) ? 1 : 0;
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
      Prompt_FlushDisplay(wm);
    }
    long long now = Platform_GetWallClockMs();
    if (now - last_tick >= 1000 && WindowManager_TopPopupWantsTick(wm)) {
      last_tick = now;
      Popup *top = WindowManager_GetTopPopup(wm);
      if (top && top->vtable && top->vtable->on_render) {
        top->vtable->on_render(top);
        Prompt_FlushDisplay(wm);
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
      SkaldContext *_ctx2 = SkaldContext_Get();
      if (!_ctx2 || !_ctx2->window.subwindow.created)
        WindowManager_PopPopup(wm);
    }
  }
  Prompt_FlushDisplay(wm);
  SkaldContext *_c = SkaldContext_Get();
  return _c->window.output_buffer[0] ? _c->window.output_buffer : NULL;
}
