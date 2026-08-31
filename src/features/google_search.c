#include "core/flapi.h"
#include "core/zoxide_helpers.h"

#define FILE_GOOGLE_ZOXIDE 5
#define GoogleZoxideFile G_FILE_ENTRIES[FILE_GOOGLE_ZOXIDE].mf

static char** g_cached_google_queries = NULL;
static int g_cached_google_count = 0;

static void RebuildGoogleCache(void) {
    ZoxideHelper_RebuildCache(GoogleZoxideFile, &g_cached_google_queries, &g_cached_google_count);
}


static void google_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    if (!lb) return;
    ZoxideHelper_Refresh(filter, hListBox, lb, GoogleZoxideFile, &g_cached_google_queries, &g_cached_google_count);
}


static void DoGoogleSearch(const char* query) {
    if (!query || strlen(query) == 0) return;
    
    char url[2048] = {0};
    char encoded_query[1024] = {0};
    
    for (int i = 0; query[i] && i < sizeof(encoded_query) - 1; i++) {
        char c = query[i];
        if (c == ' ') {
            encoded_query[strlen(encoded_query)] = '+';
        } else if (c == '#') {
            strcat(encoded_query, "%23");
        } else if (c == '&') {
            strcat(encoded_query, "%26");
        } else if (c == '?') {
            strcat(encoded_query, "%3F");
        } else if (c == '%') {
            strcat(encoded_query, "%25");
        } else if (c == '+') {
            strcat(encoded_query, "%2B");
        } else {
            encoded_query[strlen(encoded_query)] = c;
        }
    }
    
    snprintf(url, sizeof(url), "https://www.google.com/search?q=%s", encoded_query);
    
    printf("[GOOGLE] Searching: %s\n", url);
    
    Platform_ShellExecuteWithVerb(url, "open", true);
}

static void google_enter_callback(const char* selection, ListBoxInfo* lb) {
      ZoxideHelper_AppendNewEntry(GoogleZoxideFile, selection, lb, &g_cached_google_queries, &g_cached_google_count);
      DoGoogleSearch(selection);
}

static void google_edit_callback(const char* selection, ListBoxInfo* lb) {
    ZoxideHelper_Edit(selection, lb, GoogleZoxideFile, &g_cached_google_queries, &g_cached_google_count, google_refresh_callback);
}

void TriggerGoogleSearch(void) {
    if (!g_cached_google_queries) RebuildGoogleCache();
    const char** list = ZoxideHelper_PrepareTriggerList(NULL, &g_cached_google_queries, &g_cached_google_count, NULL);
    PROMPT(.title = "🔍 Google", .items = list, .count = g_cached_google_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = google_refresh_callback,
           .enter = google_enter_callback,
           .edit = google_edit_callback);
    ZoxideHelper_CleanupTriggerList(list);
}

void google_search_init(void) {
    fl_register_hotkey("Alt+G", TriggerGoogleSearch, NULL, NULL);
}
