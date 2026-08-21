#include "common.h"
#include "features/google_search.h"
#include "helpers.h"
#include "core/common_ui.h"
#if defined(_WIN32)
#include <shellapi.h>
#endif

static char** g_cached_google_queries = NULL;
static int g_cached_google_count = 0;

static void RebuildGoogleCache(void) {
    if (g_cached_google_queries) {
        for (int i = 0; i < g_cached_google_count; i++) {
            free(g_cached_google_queries[i]);
        }
        free(g_cached_google_queries);
        g_cached_google_queries = NULL;
        g_cached_google_count = 0;
    }
    
    if (!GoogleZoxideFile || GoogleZoxideFile->fileSize == 0) {
        return;
    }
    
    size_t lineCount = 0;
    Line* lines = Platform_FileGetLines(GoogleZoxideFile, &lineCount);
    if (!lines) return;
    
    g_cached_google_queries = (char**)malloc(lineCount * sizeof(char*));
    g_cached_google_count = 0;
    
    for (size_t i = 0; i < lineCount; i++) {
        size_t len = lines[i].length;
        while (len > 0 && lines[i].ptr[len-1] == '\r') len--;
        
        if (len > 0) {
            char* cmd = (char*)malloc(len + 1);
            memcpy(cmd, lines[i].ptr, len);
            cmd[len] = '\0';
            g_cached_google_queries[g_cached_google_count++] = cmd;
        }
    }
    
    free(lines);
}


static void google_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    if (!lb) return;
    
    if (!g_cached_google_queries) {
        RebuildGoogleCache();
    }
    
    PopulateListBox(lb, hListBox, filter, (const char**)g_cached_google_queries, g_cached_google_count, NULL);
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
        if (GoogleZoxideFile->fileSize > 0) {
                  char lastChar = Platform_GetLastChar(GoogleZoxideFile);
            if (lastChar != '\n') {
                Platform_FileAppend(GoogleZoxideFile, "\n");
            }
        }
        if (!UI_IsListBoxSelected(lb)) {
        Platform_FileAppend(GoogleZoxideFile, selection);
        Platform_FileAppend(GoogleZoxideFile, "\n");
        RebuildGoogleCache();
        }
        DoGoogleSearch(selection);
}

static void google_edit_callback(const char* selection, ListBoxInfo* lb) {
    char* NewgoogleQuery = Platform_EditTextPopup(selection,960,540,1000,300);
    
    if (NewgoogleQuery != NULL) {
        WriteToCacheIndex(lb, lb->selected_index,NewgoogleQuery);
    }
    
    Platform_ReplaceLineInMappedFile(GoogleZoxideFile,lb->selected_index,NewgoogleQuery);
    
    RebuildGoogleCache();
    
    google_refresh_callback("", hListBox, lb);
}

void TriggerGoogleSearch(void) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    if (!g_cached_google_queries) {
        RebuildGoogleCache();
    }
    
    const char** list = (const char**)malloc(g_cached_google_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_google_count; i++) {
        list[i] = g_cached_google_queries[i];
    }
    
    ImportantPromptUser(list, "🔍 Google", g_cached_google_count, 920, 650, 32, 
               google_refresh_callback, 
               google_enter_callback, 
               google_edit_callback,NULL);
    
    free(list);
}
