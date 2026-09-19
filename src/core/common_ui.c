#include "core/context.h"
#include "deps/log.h"
#include "core/modal.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include "core/vim.h"
#include "core/platform.h"
#if !defined(_WIN32)
#include <strings.h>
#include <ctype.h>
#include <string.h>
#else
#include <shlwapi.h>
#endif

char g_alias_display[MAX_VISIBLE_ITEMS][512];
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
static int Levenshtein(const char* a, const char* b) {
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (abs(la - lb) > 2) return 100;
    int dp[256][256];
    for (int i=0;i<=la && i<256;i++) dp[i][0]=i;
    for (int j=0;j<=lb && j<256;j++) dp[0][j]=j;
    for (int i=1;i<=la && i<256;i++) for(int j=1;j<=lb && j<256;j++){
        int cost = tolower((unsigned char)a[i-1])==tolower((unsigned char)b[j-1]) ? 0 : 1;
        int v = dp[i-1][j]+1;
        if (dp[i][j-1]+1 < v) v = dp[i][j-1]+1;
        if (dp[i-1][j-1]+cost < v) v = dp[i-1][j-1]+cost;
        if (i>1 && j>1 && tolower((unsigned char)a[i-1])==tolower((unsigned char)b[j-2]) && tolower((unsigned char)a[i-2])==tolower((unsigned char)b[j-1])) {
            if (dp[i-2][j-2]+1 < v) v = dp[i-2][j-2]+1;
        }
        dp[i][j]=v;
    }
    return dp[la][lb];
}
static bool FuzzyMatchWithTypo(const char* text, const char* pattern) {
    if (FuzzyMatch(text, pattern)) return true;
    if (!text || !pattern) return false;
    if (strlen(pattern) < 3) return false;
    int dist = Levenshtein(text, pattern);
    if (dist <= 1) return true;
    if (strlen(pattern) >= 4 && dist == 2) return true;
    return false;
}
static int FuzzyScore(const char* text, const char* pattern) {
    if (!pattern || !pattern[0]) return 0;
    if (!text || !text[0]) return 1000000;
    int score = 0;
    int last = -1;
    int consecutive = 0;
    int pos = 0;
    int tlen = (int)strlen(text);
    for (int i=0; pattern[i]; i++) {
        char pc = tolower((unsigned char)pattern[i]);
        bool found = false;
        for (int j=pos; text[j]; j++) {
            if (tolower((unsigned char)text[j]) == pc) {
                if (last >= 0) {
                    int gap = j - last - 1;
                    score += gap * 5;
                    if (gap==0) consecutive++;
                } else {
                    score += j * 2;
                }
                if (j==0 || text[j-1]==' ' || text[j-1]=='/' || text[j-1]=='_' || text[j-1]=='-' || text[j-1]=='.') score -= 3;
                last = j;
                pos = j+1;
                found = true;
                break;
            }
        }
        if (!found) return 1000000;
    }
    score -= consecutive * 4;
    score += tlen / 8;
    return score;
}

void BuildAliasDisplayItems(const char** items, int count) {
  g_alias_display_active = false;
  g_alias_display_count = 0;
  if (!items || count <= 0) return;
  if (count > MAX_VISIBLE_ITEMS) count = MAX_VISIBLE_ITEMS;
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

static void Populate_CaptureSelection(ListBoxInfo* lb, char* out_text, size_t out_size, int* out_sel) {
  int current_sel = lb->selected_index;
  if (out_sel) *out_sel = current_sel;
  if (out_text && out_size > 0) {
    out_text[0] = '\0';
    if (current_sel >= 0 && current_sel < lb->populated_count) {
      strncpy(out_text, lb->populated_cache[current_sel], out_size - 1);
      out_text[out_size - 1] = '\0';
    }
  }
}

static void Populate_CleanFilter(const char* filter, char* clean_filter, size_t size) {
  clean_filter[0] = '\0';
  if (filter) {
    strncpy(clean_filter, filter, size - 1);
    clean_filter[size - 1] = '\0';
  }
}

static bool Populate_ShouldUsePrevious(ListBoxInfo* lb, const char** items, int item_count, const char* clean_filter, int* out_prev_count) {
  int prev_count = 0;
  bool use_previous = false;
  if (strlen(clean_filter) > 0 &&
      lb->last_items == items &&
      lb->last_item_count == item_count &&
      lb->last_filter_valid &&
      strlen(clean_filter) > strlen(lb->last_filter) &&
      Platform_StringStartsWithCaseInsensitive(clean_filter, lb->last_filter)) {
    use_previous = true;
    prev_count = lb->last_filter_full_count;
  }
  if (out_prev_count) *out_prev_count = prev_count;
  return use_previous;
}

static bool Populate_ItemMatches(const char* search_text, const char* original_item, const char* clean_filter, bool (*filter_match)(const char*, const char*)) {
  if (filter_match) return filter_match(search_text, clean_filter);
  if (Platform_StringStartsWithCaseInsensitive(FlintContext_Get()->window.current_feature, "center")) {
    return FuzzyMatchWithTypo(search_text, clean_filter);
  }
  if (Platform_StringContainsCaseInsensitive(search_text, clean_filter)) return true;
  const char* alias = get_alias_for_item(original_item);
  if (alias && Platform_StringContainsCaseInsensitive(alias, clean_filter)) return true;
  return false;
}

static int Populate_FilterIncremental(ListBoxInfo* lb, const char** items, const char* clean_filter, bool (*filter_match)(const char*, const char*), int prev_count) {
  int total_matches = 0;
  int* prev_matches = lb->prev_matches;
  for (int i = 0; i < prev_count && total_matches < MAX_VISIBLE_ITEMS; i++) {
    int idx = prev_matches[i];
    if (!items[idx]) continue;
    const char* search_text = items[idx];
    if (g_alias_display_active && idx >= 0 && idx < g_alias_display_count && g_alias_display[idx][0] != '\0') {
      search_text = g_alias_display[idx];
    }
    if (Populate_ItemMatches(search_text, items[idx], clean_filter, filter_match)) {
      lb->matching_indices[total_matches++] = idx;
    }
  }
  return total_matches;
}

static int Populate_FilterFull(ListBoxInfo* lb, const char** items, int item_count, const char* clean_filter, bool (*filter_match)(const char*, const char*)) {
  int total_matches = 0;
  bool empty_filter = (clean_filter[0] == '\0');
  for (int i = 0; i < item_count && total_matches < MAX_VISIBLE_ITEMS; i++) {
    if (!items[i]) continue;
    if (empty_filter) {
      lb->matching_indices[total_matches++] = i;
      continue;
    }
    const char* search_text = items[i];
    if (g_alias_display_active && i < g_alias_display_count && g_alias_display[i][0] != '\0') {
      search_text = g_alias_display[i];
    }
    if (Populate_ItemMatches(search_text, items[i], clean_filter, filter_match)) {
      lb->matching_indices[total_matches++] = i;
    }
  }
  return total_matches;
}

static int Populate_Filter(ListBoxInfo* lb, const char** items, int item_count, const char* clean_filter, bool (*filter_match)(const char*, const char*), bool use_previous, int prev_count) {
  if (use_previous && prev_count == 0) return 0;
  if (use_previous && prev_count > 0) return Populate_FilterIncremental(lb, items, clean_filter, filter_match, prev_count);
  return Populate_FilterFull(lb, items, item_count, clean_filter, filter_match);
}

static void Populate_RankMatches(ListBoxInfo* lb, const char** items, const char* clean_filter, int total_matches) {
  if (clean_filter[0] == '\0' || total_matches <= 1) return;
  int scores[MAX_VISIBLE_ITEMS];
  for (int i = 0; i < total_matches; i++) {
    int idx = lb->matching_indices[i];
    const char* txt = items[idx];
    if (g_alias_display_active && idx >= 0 && idx < g_alias_display_count && g_alias_display[idx][0] != '\0') txt = g_alias_display[idx];
    if (FuzzyMatch(txt, clean_filter)) scores[i] = FuzzyScore(txt, clean_filter);
    else scores[i] = 500 + Levenshtein(txt, clean_filter) * 20;
  }
  for (int i = 0; i < total_matches - 1; i++) for (int j = i + 1; j < total_matches; j++) if (scores[j] < scores[i]) {
    int ti = lb->matching_indices[i]; lb->matching_indices[i] = lb->matching_indices[j]; lb->matching_indices[j] = ti;
    int ts = scores[i]; scores[i] = scores[j]; scores[j] = ts;
  }
}

static void Populate_SaveHistory(ListBoxInfo* lb, int total_matches, const char* clean_filter) {
  if (total_matches > 0 && strlen(clean_filter) > 0) {
    memcpy(lb->prev_matches, lb->matching_indices, total_matches * sizeof(int));
    lb->last_filter_full_count = total_matches;
  } else {
    lb->last_filter_full_count = 0;
  }
}

static void Populate_EnsureMatches(ListBoxInfo* lb, int item_count, const char* clean_filter, int* total_matches) {
  if (*total_matches == 0 && item_count > 0 && strlen(clean_filter) == 0) {
    int force_count = (item_count < 30) ? item_count : 30;
    for (int i = 0; i < force_count; i++) {
      lb->matching_indices[*total_matches] = i;
      (*total_matches)++;
    }
  }
  int pool_max = MAX_VISIBLE_ITEMS;
  if (*total_matches > pool_max) *total_matches = pool_max;
}

static void Populate_BuildCache(ListBoxInfo* lb, const char** items, int total_matches) {
  lb->populated_count = 0;
  for (int i = 0; i < total_matches; i++) {
    int raw_data_index = lb->matching_indices[i];
    if (!items[raw_data_index]) continue;
    strncpy(lb->populated_cache[lb->populated_count], items[raw_data_index], 511);
    lb->populated_cache[lb->populated_count][511] = '\0';
    lb->populated_cache[lb->populated_count][strcspn(lb->populated_cache[lb->populated_count], "\r\n")] = '\0';
    lb->populated_count++;
  }
}

static void Populate_UpdateUI(ListBoxInfo* lb, WindowHandle hListBox, int total_matches, const char* filter) {
  UI_LB_SetCount(lb, hListBox, total_matches);
  if (SequenceIsWaiting()) return;
  if (IsNormalMode()) {
    int keep;
    if (RememberLastIndex) {
      keep = lb->selected_index;
      if (keep < 0 && last_index >= 0) keep = last_index;
      if (total_matches == 0) keep = -1;
      else if (keep < 0) keep = 0;
      else if (keep >= total_matches) keep = total_matches - 1;
      UI_NavigateTo(lb, hListBox, keep);
    } else {
      keep = (total_matches > 0) ? 0 : -1;
      UI_SetSelection(lb, hListBox, keep);
    }
  } else if (total_matches != 0 && strlen(filter) > 0) {
    UI_SetSelection(lb, hListBox, 0);
  } else {
    UI_SetSelection(lb, hListBox, -1);
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

static void Populate_CommitState(ListBoxInfo* lb, const char** items, int item_count, const char* clean_filter, int total_matches, int old_count) {
  (void)old_count;
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
}

void PopulateListBox(ListBoxInfo* lb, WindowHandle hListBox, const char* filter,
    const char** items, int item_count,
    bool (*filter_match)(const char* item, const char* filter)) {
  if (!lb) return;
  PROFILE_START(timer_alias);
  if (SequenceIsWaiting()) return;
  if (!filter) filter = "";
  BuildAliasDisplayItems(items, item_count);
  if (items && item_count > 0) {
    if (FlintContext_Get()->item_index_map) {
      shfree(FlintContext_Get()->item_index_map);
      FlintContext_Get()->item_index_map = NULL;
    }
    FlintContext_Get()->item_index_map = NULL;
    sh_new_strdup(FlintContext_Get()->item_index_map);
    for (int i = 0; i < item_count; i++) if (items[i]) shput(FlintContext_Get()->item_index_map, (char*)items[i], (void*)(size_t)(i + 1));
  }
  PROFILE_END_VAR(timer_alias, "PopulateListBox - Aliases");

  PROFILE_START(timer_total);
  int old_count = lb->total_matches;

  PROFILE_START(timer_current_sel);
  int current_sel;
  char current_text[256];
  Populate_CaptureSelection(lb, current_text, sizeof(current_text), &current_sel);
  PROFILE_END_VAR(timer_current_sel, "  PopulateListBox - Get current selection");

  PROFILE_START(timer_clean_filter);
  char clean_filter[256];
  Populate_CleanFilter(filter, clean_filter, sizeof(clean_filter));
  int total_matches = 0;
  PROFILE_END_VAR(timer_clean_filter, "  PopulateListBox - Clean filter");

  PROFILE_START(timer_inc);
  int prev_count = 0;
  bool use_previous = Populate_ShouldUsePrevious(lb, items, item_count, clean_filter, &prev_count);
  PROFILE_END_VAR(timer_inc, "  PopulateListBox - Incremental check");

  PROFILE_START(timer_filter);
  total_matches = Populate_Filter(lb, items, item_count, clean_filter, filter_match, use_previous, prev_count);
  PROFILE_END_VAR(timer_filter, "  PopulateListBox - Filtering");
  Populate_RankMatches(lb, items, clean_filter, total_matches);

  PROFILE_START(timer_history);
  Populate_SaveHistory(lb, total_matches, clean_filter);
  PROFILE_END_VAR(timer_history, "  PopulateListBox - History save");

  PROFILE_START(timer_matches);
  PROFILE_START(timer_pool);
  Populate_EnsureMatches(lb, item_count, clean_filter, &total_matches);
  PROFILE_END_VAR(timer_matches, " PopulateListBox - Matches");
  PROFILE_END_VAR(timer_pool, "  PopulateListBox - Pool adjust");

  PROFILE_START(timer_cache);
  Populate_BuildCache(lb, items, total_matches);
  PROFILE_END_VAR(timer_cache, "  PopulateListBox - Cache build");

  PROFILE_START(timer_ui);
  Populate_UpdateUI(lb, hListBox, total_matches, filter);
  PROFILE_END_VAR(timer_ui, "  PopulateListBox - UI update");

  PROFILE_START(timer_cleanup);
  Populate_CommitState(lb, items, item_count, clean_filter, total_matches, old_count);
  PROFILE_END_VAR(timer_cleanup, "  PopulateListBox - Cleanup");

  log_info("PopulateListBox completed for filter='%s'", filter ? filter : "");
  PROFILE_END_VAR(timer_total, "PopulateListBox TOTAL");
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

bool UI_IsListBoxSelected(ListBoxInfo* lb) {
  return lb && lb->selected_index != -1;
}
