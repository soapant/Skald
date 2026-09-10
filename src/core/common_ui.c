#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include "helpers.h"
#include "core/common_ui.h"
#include "core/vim.h"
#include "core/platform.h"
#if !defined(_WIN32)
#include <strings.h>
#include <ctype.h>
#include <string.h>
#else
#include <shlwapi.h>
#endif

bool g_add_alias_to_string = true;
char g_alias_display[MAX_ITEMS][512] = {0};
int g_alias_display_count = 0;
bool g_alias_display_active = false;

static bool FuzzyMatch(const char* text, const char* pattern) {
    if (!pattern || !pattern[0]) return true;
    if (!text || !text[0]) return false;
    while (*pattern && *text) {
        if (tolower((unsigned char)*pattern) == tolower((unsigned char)*text)) pattern++;
        text++;
    }
    return *pattern == '\0';
}

void BuildAliasDisplayItems(const char** items, int count) {
  g_alias_display_active = false;
  g_alias_display_count = 0;
  if (!items || count <= 0) return;
  if (count > MAX_ITEMS) count = MAX_ITEMS;
  for (int i = 0; i < count; i++) {
    g_alias_display[i][0] = '\0';
    if (!items[i]) continue;
    size_t len = strlen(items[i]);
    if (len >= 512) len = 511;
    memcpy(g_alias_display[i], items[i], len);
    g_alias_display[i][len] = '\0';
    while (len > 0 && (g_alias_display[i][len - 1] == '\r' || g_alias_display[i][len - 1] == '\n')) {
      g_alias_display[i][--len] = '\0';
    }
    const char* alias = get_alias_for_item(g_alias_display[i]);
    if (alias) {
      snprintf(g_alias_display[i] + len, (size_t)(512 - len), " [%s]", alias);
    }
    g_alias_display_count = i + 1;
  }
  if (g_alias_display_count > 0) g_alias_display_active = true;
}

void PopulateListBox(ListBoxInfo* lb, WindowHandle hListBox, const char* filter,
    const char** items, int item_count,
    bool (*filter_match)(const char* item, const char* filter)) {

  if (!lb) return;
  WinTimer timer_alias = TimerStart();
#if defined(_WIN32)
  if (WH_TO_HWND(FlintContext_Get()->window.hListBox) == NULL) return;
#endif
  if (SequenceIsWaiting()) return;
  if (!filter) filter = "";
  BuildAliasDisplayItems(items, item_count);
  TimerEnd(timer_alias,"PopulateListBox - Aliases");


  WinTimer timer_total = TimerStart();
  int old_count = lb->total_matches;

  WinTimer timer_current_sel = TimerStart();
  int current_sel = lb->selected_index;
  char current_text[256] = {0};
  if (current_sel >= 0 && current_sel < lb->populated_count) {
    strncpy(current_text, lb->populated_cache[current_sel], sizeof(current_text) - 1);
    current_text[sizeof(current_text) - 1] = '\0';
  }
  TimerEnd(timer_current_sel, "  PopulateListBox - Get current selection");

  WinTimer timer_clean_filter = TimerStart();
  int total_matches = 0;
  char clean_filter[256] = {0};
  if (filter) {
    strncpy(clean_filter, filter, sizeof(clean_filter) - 1);
    clean_filter[sizeof(clean_filter) - 1] = '\0';
  }
  TimerEnd(timer_clean_filter, "  PopulateListBox - Clean filter");

  WinTimer timer_inc = TimerStart();
  int use_previous = 0;
  int prev_count = 0;
  if (strlen(clean_filter) > 0 &&
      lb->last_items == items &&
      lb->last_item_count == item_count &&
      lb->last_filter_valid &&
      strlen(clean_filter) > strlen(lb->last_filter) &&
      Platform_StringStartsWithCaseInsensitive(clean_filter, lb->last_filter)) {

    use_previous = 1;
    prev_count = lb->last_filter_full_count;
  }
  TimerEnd(timer_inc, "  PopulateListBox - Incremental check");

  WinTimer timer_filter = TimerStart();
  if (use_previous && prev_count == 0) {
    total_matches = 0;
  } else if (use_previous && prev_count > 0) {
    int* prev_matches = lb->prev_matches;

    for (int i = 0; i < prev_count && total_matches < MAX_MATCHES; i++) {
      int idx = prev_matches[i];
      if (!items[idx]) continue;

      const char* search_text = items[idx];
      if (g_alias_display_active && idx >= 0 && idx < g_alias_display_count && g_alias_display[idx][0] != '\0') {
        search_text = g_alias_display[idx];
      }
      bool match = false;
      if (filter_match) {
        match = filter_match(search_text, clean_filter);
      } else {
        match = FuzzyMatch(search_text, clean_filter);
      }

      if (match) {
        lb->matching_indices[total_matches] = idx;
        total_matches++;
      }
    }
  } else {
    bool empty_filter = (clean_filter[0] == '\0');
    for (int i = 0; i < item_count && total_matches < MAX_MATCHES; i++) {
      if (!items[i]) continue;

      if (empty_filter) {
        lb->matching_indices[total_matches] = i;
        total_matches++;
        continue;
      }

      const char* search_text = items[i];
      if (g_alias_display_active && i < g_alias_display_count && g_alias_display[i][0] != '\0') {
        search_text = g_alias_display[i];
      }
      bool match = filter_match ? filter_match(search_text, clean_filter) : FuzzyMatch(search_text, clean_filter);

      if (match) {
        lb->matching_indices[total_matches] = i;
        total_matches++;
      }
    }
  }
  TimerEnd(timer_filter, "  PopulateListBox - Filtering");

  WinTimer timer_history = TimerStart();
  if (total_matches > 0 && strlen(clean_filter) > 0) {
    memcpy(lb->prev_matches, lb->matching_indices, total_matches * sizeof(int));
    lb->last_filter_full_count = total_matches;
  } else {
    lb->last_filter_full_count = 0;
  }
  TimerEnd(timer_history, "  PopulateListBox - History save");

  WinTimer timer_matches = TimerStart();
  if (total_matches == 0 && item_count > 0 && strlen(clean_filter) == 0) {
    int force_count = (item_count < 30) ? item_count : 30;
    for (int i = 0; i < force_count; i++) {
      lb->matching_indices[total_matches] = i;
      total_matches++;
    }
  }
  TimerEnd(timer_matches," PopulateListBox - Matches");

  WinTimer timer_pool = TimerStart();
  int pool_max = MAX_VISIBLE_ITEMS;
  if (total_matches > pool_max) {
    total_matches = pool_max;
  }
  TimerEnd(timer_pool, "  PopulateListBox - Pool adjust");

  WinTimer timer_cache = TimerStart();
  lb->populated_count = 0;

  for (int i = 0; i < total_matches; i++) {
    int raw_data_index = lb->matching_indices[i];
    if (!items[raw_data_index]) continue;

    strncpy(lb->populated_cache[lb->populated_count], items[raw_data_index], 511);
    lb->populated_cache[lb->populated_count][511] = '\0';
    lb->populated_cache[lb->populated_count][strcspn(lb->populated_cache[lb->populated_count], "\r\n")] = '\0';
    lb->populated_count++;
  }
  TimerEnd(timer_cache, "  PopulateListBox - Cache build");

  UI_LB_SetCount(lb, FlintContext_Get()->window.hListBox, total_matches);

  WinTimer timer_ui = TimerStart();
  if (!SequenceIsWaiting()) {
    if (IsNormalMode()) {
      int keep;
      if (RememberLastIndex) {
        keep = lb->selected_index;
        if (keep < 0 && last_index >= 0) {
          keep = last_index;
        }
        if (total_matches == 0) keep = -1;
        else if (keep < 0) keep = 0;
        else if (keep >= total_matches) keep = total_matches - 1;
        UI_NavigateTo(lb, FlintContext_Get()->window.hListBox, keep);
      } else {
        keep = (total_matches > 0) ? 0 : -1;
        UI_SetSelection(lb, FlintContext_Get()->window.hListBox, keep);
      }
    } else if (total_matches != 0 && strlen(filter) > 0) {
      UI_SetSelection(lb, FlintContext_Get()->window.hListBox, 0);
#if defined(_WIN32)
      FlintContext_Get()->draw_background = false;
#endif
    } else {
      UI_SetSelection(lb, FlintContext_Get()->window.hListBox, -1);
#if defined(_WIN32)
      FlintContext_Get()->draw_background = true;
#endif
    }
    if (total_matches == 0) {
      if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
#if defined(_WIN32)
        KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
        DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
#endif
        FlintContext_Get()->window.hDisplayMessage = WH_NULL;
      }
    }
  }

  if (total_matches != old_count || clean_filter[0] != '\0') {
#if defined(_WIN32)
    InvalidateRect(WH_TO_HWND(FlintContext_Get()->window.hListBox), NULL, FALSE);
#endif
  }
  TimerEnd(timer_ui, "  PopulateListBox - UI update");

  WinTimer timer_cleanup = TimerStart();
  if (strlen(clean_filter) > 0) {
    strncpy(lb->last_filter, clean_filter, sizeof(lb->last_filter) - 1);
    lb->last_filter[sizeof(lb->last_filter) - 1] = '\0';
    lb->last_filter_valid = true;
  } else {
    lb->last_filter_valid = false;
  }
  lb->last_items = items;
  lb->last_item_count = item_count;
  lb->total_matches = total_matches;
  if (total_matches != old_count) {
#if defined(_WIN32)
    FlintContext_Get()->draw_background = true;
#endif
  }
  TimerEnd(timer_cleanup, "  PopulateListBox - Cleanup");

  TimerEnd(timer_total, "PopulateListBox TOTAL");
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
    float r, g, b;
    switch (i) {
      case 0: r = v; g = t_; b = p; break;
      case 1: r = q; g = v; b = p; break;
      case 2: r = p; g = v; b = t_; break;
      case 3: r = p; g = q; b = v; break;
      case 4: r = t_; g = p; b = v; break;
      case 5: r = v; g = p; b = q; break;
    }
    return RGB((uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255));
  } else {
    return COLOR_TEXT;
  }
}
