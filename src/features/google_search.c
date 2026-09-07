#include "core/flapi.h"
#include "core/zoxide_helpers.h"

const char* GoogleZoxideFilePath = "data/GoogleZoxide.txt";

char** g_cached_google_queries = NULL;
int g_cached_google_count = 0;

void RebuildGoogleCache(void) {
    ZoxideHelper_RebuildCache(GoogleZoxideFilePath, &g_cached_google_queries, &g_cached_google_count);
}


void google_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    if (!lb) return;
    ZoxideHelper_Refresh(filter, FlintContext_Get()->window.hListBox, lb, GoogleZoxideFilePath, &g_cached_google_queries, &g_cached_google_count);
}



bool google_enter_callback(const char* selection, ListBoxInfo* lb) {
      ZoxideHelper_AppendNewEntry(GoogleZoxideFilePath, selection, lb, &g_cached_google_queries, &g_cached_google_count);
      Platform_LaunchBrowserAtUrl(selection,false);
    return true;
}

void google_edit_callback(const char* selection, ListBoxInfo* lb) {
    ZoxideHelper_Edit(selection, lb, GoogleZoxideFilePath, &g_cached_google_queries, &g_cached_google_count, google_refresh_callback);
}

void TriggerGoogleSearch(void) {
    if (!g_cached_google_queries) RebuildGoogleCache();
    const char** list = ZoxideHelper_PrepareTriggerList(GoogleZoxideFilePath, &g_cached_google_queries, &g_cached_google_count, NULL);
    PROMPT(.title = "🔍 Google", .items = list, .count = g_cached_google_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = google_refresh_callback,
           .enter = google_enter_callback,
           .edit = google_edit_callback);
    ZoxideHelper_CleanupTriggerList(list);
}

FLINT_FEATURE_INIT {
  fl_register_hotkey("Alt+G", TriggerGoogleSearch);
}
