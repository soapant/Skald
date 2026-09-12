#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
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
  PROFILE_START(timer_alias);
#if defined(_WIN32)
  if (WH_TO_HWND(FlintContext_Get()->window.hListBox) == NULL) return;
#endif
  if (SequenceIsWaiting()) return;
  if (!filter) filter = "";
  BuildAliasDisplayItems(items, item_count);
  PROFILE_END_VAR(timer_alias, "PopulateListBox - Aliases");


  PROFILE_START(timer_total);
  int old_count = lb->total_matches;

  PROFILE_START(timer_current_sel);
  int current_sel = lb->selected_index;
  char current_text[256] = {0};
  if (current_sel >= 0 && current_sel < lb->populated_count) {
    strncpy(current_text, lb->populated_cache[current_sel], sizeof(current_text) - 1);
    current_text[sizeof(current_text) - 1] = '\0';
  }
  PROFILE_END_VAR(timer_current_sel, "  PopulateListBox - Get current selection");

  PROFILE_START(timer_clean_filter);
  int total_matches = 0;
  char clean_filter[256] = {0};
  if (filter) {
    strncpy(clean_filter, filter, sizeof(clean_filter) - 1);
    clean_filter[sizeof(clean_filter) - 1] = '\0';
  }
  PROFILE_END_VAR(timer_clean_filter, "  PopulateListBox - Clean filter");

  PROFILE_START(timer_inc);
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
  PROFILE_END_VAR(timer_inc, "  PopulateListBox - Incremental check");

  PROFILE_START(timer_filter);
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
        if (Platform_StringStartsWithCaseInsensitive(FlintContext_Get()->window.current_feature,"center")) {
          match = FuzzyMatchWithTypo(search_text, clean_filter);
        } else {
          if (Platform_StringStartsWithCaseInsensitive(search_text, clean_filter)) match = true;
          else {
            const char* alias = get_alias_for_item(items[idx]);
            if (alias && Platform_StringStartsWithCaseInsensitive(alias, clean_filter)) match = true;
          }
        }
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
      bool match = false;
      if (filter_match) {
        match = filter_match(search_text, clean_filter);
      } else {
        if (Platform_StringStartsWithCaseInsensitive(FlintContext_Get()->window.current_feature,"center")) {
          match = FuzzyMatchWithTypo(search_text, clean_filter);
        } else {
          if (Platform_StringStartsWithCaseInsensitive(search_text, clean_filter)) match = true;
          else {
            const char* alias = get_alias_for_item(items[i]);
            if (alias && Platform_StringStartsWithCaseInsensitive(alias, clean_filter)) match = true;
          }
        }
      }

      if (match) {
        lb->matching_indices[total_matches] = i;
        total_matches++;
      }
    }
  }
  PROFILE_END_VAR(timer_filter, "  PopulateListBox - Filtering");
  if (clean_filter[0] != '\0' && total_matches > 1) {
      int scores[MAX_MATCHES];
      for (int i=0;i<total_matches;i++) {
          int idx = lb->matching_indices[i];
          const char* txt = items[idx];
          if (g_alias_display_active && idx >=0 && idx < g_alias_display_count && g_alias_display[idx][0] != '\0') txt = g_alias_display[idx];
          if (FuzzyMatch(txt, clean_filter)) scores[i] = FuzzyScore(txt, clean_filter);
          else scores[i] = 500 + Levenshtein(txt, clean_filter) * 20;
      }
      for (int i=0;i<total_matches-1;i++) for (int j=i+1;j<total_matches;j++) if (scores[j] < scores[i]) {
          int ti = lb->matching_indices[i]; lb->matching_indices[i]=lb->matching_indices[j]; lb->matching_indices[j]=ti;
          int ts = scores[i]; scores[i]=scores[j]; scores[j]=ts;
      }
  }

  PROFILE_START(timer_history);
  if (total_matches > 0 && strlen(clean_filter) > 0) {
    memcpy(lb->prev_matches, lb->matching_indices, total_matches * sizeof(int));
    lb->last_filter_full_count = total_matches;
  } else {
    lb->last_filter_full_count = 0;
  }
  PROFILE_END_VAR(timer_history, "  PopulateListBox - History save");

  PROFILE_START(timer_matches);
  if (total_matches == 0 && item_count > 0 && strlen(clean_filter) == 0) {
    int force_count = (item_count < 30) ? item_count : 30;
    for (int i = 0; i < force_count; i++) {
      lb->matching_indices[total_matches] = i;
      total_matches++;
    }
  }
  PROFILE_END_VAR(timer_matches, " PopulateListBox - Matches");

  PROFILE_START(timer_pool);
  int pool_max = MAX_VISIBLE_ITEMS;
  if (total_matches > pool_max) {
    total_matches = pool_max;
  }
  PROFILE_END_VAR(timer_pool, "  PopulateListBox - Pool adjust");

  PROFILE_START(timer_cache);
  lb->populated_count = 0;

  for (int i = 0; i < total_matches; i++) {
    int raw_data_index = lb->matching_indices[i];
    if (!items[raw_data_index]) continue;

    strncpy(lb->populated_cache[lb->populated_count], items[raw_data_index], 511);
    lb->populated_cache[lb->populated_count][511] = '\0';
    lb->populated_cache[lb->populated_count][strcspn(lb->populated_cache[lb->populated_count], "\r\n")] = '\0';
    lb->populated_count++;
  }
  PROFILE_END_VAR(timer_cache, "  PopulateListBox - Cache build");

  UI_LB_SetCount(lb, FlintContext_Get()->window.hListBox, total_matches);

  PROFILE_START(timer_ui);
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
  PROFILE_END_VAR(timer_ui, "  PopulateListBox - UI update");

  PROFILE_START(timer_cleanup);
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
  PROFILE_END_VAR(timer_cleanup, "  PopulateListBox - Cleanup");

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
