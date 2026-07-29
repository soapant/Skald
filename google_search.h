#ifndef GOOGLE_SEARCH_H
#define GOOGLE_SEARCH_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <shellapi.h>
#include "types.h"
#include "helpers.h"
#include "config.h"

#define FILE_GOOGLE_ZOXIDE 5
#define GoogleZoxideFile G_FILE_ENTRIES[FILE_GOOGLE_ZOXIDE].mf

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
    Line* lines = FileGetLines(GoogleZoxideFile, &lineCount);
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


static void google_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    if (!g_cached_google_queries) {
        RebuildGoogleCache();
    }
    
    PopulateListBox(hListBox, filter, (const char**)g_cached_google_queries, g_cached_google_count, NULL);
}


static void DoGoogleSearch(const char* query) {
    if (!query || strlen(query) == 0) return;
    
    char url[2048] = {0};
    char encoded_query[1024] = {0};
    
    // URL encode the query
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
    
    ShellExecuteWithVerb(url, "open", true);
    // ... rest of function ...
}

static void google_enter_callback(const char* selection) {
    int selected_index = LB_ERR;
    if (hListBox != NULL) {
        selected_index = UI_GetSelection(hListBox);
    }
    
    if (selected_index == LB_ERR) {
        char input[256] = {0};
        if (hEditControl != NULL) {
            UI_GetText(hEditControl, input, sizeof(input));
            if (strlen(input) > 0) {
                FileAppend(GoogleZoxideFile, input);
                FileAppend(GoogleZoxideFile, "\n");
                RebuildGoogleCache();
                printf("[GOOGLE] Saved query: %s\n", input);
                DoGoogleSearch(input);
            }
        }
    } else {
        DoGoogleSearch(selection);
    }
}

static void google_edit_callback(const char* selection) {
    char* NewgoogleQuery = EditTextPopup(selection,960,540,1000,300);
    
    if (NewgoogleQuery != NULL) {
        WriteToCacheIndex(g_SelectedIndex,NewgoogleQuery);
    }
    
    ReplaceLineInMappedFile(GoogleZoxideFile,g_SelectedIndex,NewgoogleQuery);
    
    RebuildGoogleCache();
    
    google_refresh_callback("",hListBox);
}

static void TriggerGoogleSearch() {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    if (!g_cached_google_queries) {
        RebuildGoogleCache();
    }
    
    const char** list = (const char**)malloc(g_cached_google_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_google_count; i++) {
        list[i] = g_cached_google_queries[i];
    }
    
    PromptUser(list, "🔍 Google", g_cached_google_count, 920, 650, 32, 
               google_refresh_callback, 
               google_enter_callback, 
               NULL, 
               NULL,
               NULL,
               google_edit_callback);
    
    free(list);
}

#endif
