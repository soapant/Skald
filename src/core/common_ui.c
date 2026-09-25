#include "core/context.h"
#include "deps/log.h"
#include "core/modal.h"
#include "config.h"
#include "platform/platform.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include "core/vim.h"
#include "platform/platform.h"
#include "core/timer.h"
#if !defined(_WIN32)
#include <strings.h>
#include <ctype.h>
#include <string.h>
#else
#include <shlwapi.h>
#endif



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

void BuildAliasDisplayItems(ListBoxInfo* lb, const char** items, int count) {
  if (!lb) return;
  lb->alias_suffix_active = false;
  lb->alias_suffix_count = 0;
  if (!items || count <= 0) return;
  if (count > MAX_VISIBLE_ITEMS) count = MAX_VISIBLE_ITEMS;
  for (int i = 0; i < count; i++) {
    lb->alias_suffix[i] = NULL;
    if (!items[i]) continue;
    lb->alias_suffix[i] = get_alias_for_item(items[i]);
    lb->alias_suffix_count = i + 1;
  }
  if (lb->alias_suffix_count > 0) lb->alias_suffix_active = true;
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
  if (!filter) return;
  while (*filter && isspace((unsigned char)*filter)) filter++;
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
  while (j > 0 && clean_filter[j - 1] == ' ') j--;
  clean_filter[j] = '\0';
}

static void NormalizeForMatch(const char* src, char* dst, size_t dst_size) {
  if (!src || !dst || dst_size == 0) { if (dst && dst_size) dst[0]='\0'; return; }
  while (*src && isspace((unsigned char)*src)) src++;
  size_t j = 0;
  bool last_was_space = false;
  for (size_t i = 0; src[i] && j + 1 < dst_size; i++) {
    char c = src[i];
    if (isspace((unsigned char)c)) {
      if (!last_was_space) { dst[j++] = ' '; last_was_space = true; }
    } else { dst[j++] = c; last_was_space = false; }
  }
  while (j > 0 && dst[j - 1] == ' ') j--;
  dst[j] = '\0';
}

static bool Populate_ItemMatches(const char* search_text, const char* original_item, const char* clean_filter, bool (*filter_match)(const char*, const char*)) {
  if (filter_match) return filter_match(search_text, clean_filter);
  char norm_search[1024];
  char norm_alias[512];
  NormalizeForMatch(search_text, norm_search, sizeof(norm_search));
  if (Platform_StringStartsWithCaseInsensitive(FlintContext_Get()->window.current_feature, "center")) {
    return FuzzyMatchWithTypo(norm_search, clean_filter);
  }
  if (Platform_StringContainsCaseInsensitive(norm_search, clean_filter)) return true;
  const char* alias = get_alias_for_item(original_item);
  if (alias) {
    NormalizeForMatch(alias, norm_alias, sizeof(norm_alias));
    if (Platform_StringContainsCaseInsensitive(norm_alias, clean_filter)) return true;
    if (Platform_StringContainsCaseInsensitive(norm_search, clean_filter)) return true;
  }
  return false;
}

static const char* AliasComposedText(ListBoxInfo* lb, const char* item, int raw, char* out, size_t out_size) {
  if (lb && lb->alias_suffix_active && raw >= 0 && raw < lb->alias_suffix_count && lb->alias_suffix[raw] && item) {
    snprintf(out, out_size, "%.*s [%s]", (int)strcspn(item, "\r\n"), item, lb->alias_suffix[raw]);
    return out;
  }
  return item;
}

static int Populate_Filter(ListBoxInfo* lb, const char** items, int item_count, const char* clean_filter, bool (*filter_match)(const char*, const char*)) {
  int total_matches = 0;
  bool empty_filter = (clean_filter[0] == '\0');
  for (int i = 0; i < item_count && total_matches < MAX_VISIBLE_ITEMS; i++) {
    if (!items[i]) continue;
    if (empty_filter) {
      lb->matching_indices[total_matches++] = i;
      continue;
    }
    char composed[1024];
    const char* search_text = AliasComposedText(lb, items[i], i, composed, sizeof(composed));
    if (Populate_ItemMatches(search_text, items[i], clean_filter, filter_match)) {
      lb->matching_indices[total_matches++] = i;
    }
  }
  return total_matches;
}



static void Populate_RankMatches(ListBoxInfo* lb, const char** items, const char* clean_filter, int total_matches) {
  if (clean_filter[0] == '\0' || total_matches <= 1) return;
  int scores[MAX_VISIBLE_ITEMS];
  for (int i = 0; i < total_matches; i++) {
    int idx = lb->matching_indices[i];
    char composed[1024];
    const char* txt = AliasComposedText(lb, items[idx], idx, composed, sizeof(composed));
    if (FuzzyMatch(txt, clean_filter)) scores[i] = FuzzyScore(txt, clean_filter);
    else scores[i] = 500 + Levenshtein(txt, clean_filter) * 20;
  }
  for (int i = 0; i < total_matches - 1; i++) for (int j = i + 1; j < total_matches; j++) if (scores[j] < scores[i]) {
    int ti = lb->matching_indices[i]; lb->matching_indices[i] = lb->matching_indices[j]; lb->matching_indices[j] = ti;
    int ts = scores[i]; scores[i] = scores[j]; scores[j] = ts;
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
  } else if (lb->selected_index >= 0) {
    UI_NavigateTo(lb, hListBox, lb->selected_index);
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

static void Populate_CommitState(ListBoxInfo* lb, int item_count, int total_matches) {
  lb->last_item_count = item_count;
  lb->total_matches = total_matches;
}

void PopulateListBox(ListBoxInfo* lb, WindowHandle hListBox, const char* filter,
    const char** items, int item_count,
    bool (*filter_match)(const char* item, const char* filter)) {
  if (!lb) return;
  PROFILE_START(timer_alias);
  if (SequenceHasPendingWait()) {
    log_info("PopulateListBox waiting filter='%s'", filter ? filter : "");
    return;
  }
  if (!filter) filter = "";
  BuildAliasDisplayItems(lb, items, item_count);
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

  PROFILE_START(timer_filter);
  total_matches = Populate_Filter(lb, items, item_count, clean_filter, filter_match);
  PROFILE_END_VAR(timer_filter, "  PopulateListBox - Filtering");
  Populate_RankMatches(lb, items, clean_filter, total_matches);

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
  Populate_CommitState(lb, item_count, total_matches);
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
