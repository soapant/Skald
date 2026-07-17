#ifndef GOOGLE_SEARCH_H
#define GOOGLE_SEARCH_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <shellapi.h>
#include "types.h"
#include "helpers.h"
#include "config.h"

extern HWND hPopupWnd;
extern HWND hEditControl;
extern HWND hListBox;
extern bool selection_complete;
extern char output_buffer[256];
extern FileEntry G_FILE_ENTRIES[];
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
    
    SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
    
    if (!filter || strlen(filter) == 0) {
        for (int i = 0; i < g_cached_google_count; i++) {
            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)g_cached_google_queries[i]);
        }
    } else {
        for (int i = 0; i < g_cached_google_count; i++) {
            if (StrStrIA(g_cached_google_queries[i], filter) != NULL) {
                SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)g_cached_google_queries[i]);
            }
        }
    }
    
    if (g_cached_google_count > 0) {
        SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
    }
}

static void DoGoogleSearch(const char* query) {
    if (!query || strlen(query) == 0) return;
    
    char url[512];
    snprintf(url, sizeof(url), "https://www.google.com/search?q=%s", query);
    
    for (int i = 0; url[i]; i++) {
        if (url[i] == ' ') {
            url[i] = '+';
        }
    }
    
    ShellExecuteWithVerb(url, "open", true);

    const char* browser_classes[] = {
        "Chrome_WidgetWin_1",
        "MozillaWindowClass",
        "IEFrame",
        "ApplicationFrameWindow"
    };

    HWND hBrowser = NULL;
    for (int i = 0; i < 4; i++) {
        hBrowser = FindWindowA(browser_classes[i], NULL);
        if (hBrowser && IsWindowVisible(hBrowser)) {
            break;
        }
    }

    if (hBrowser && IsWindow(hBrowser)) {
        SetForegroundWindow(hBrowser);
        SetActiveWindow(hBrowser);
        SetFocus(hBrowser);
        ShowWindow(hBrowser, SW_RESTORE);
    }
}

static void google_enter_callback(const char* selection) {
    int selected_index = LB_ERR;
    if (hListBox != NULL) {
        selected_index = SendMessageA(hListBox, LB_GETCURSEL, 0, 0);
    }
    
    if (selected_index == LB_ERR) {
        char input[256] = {0};
        if (hEditControl != NULL) {
            GetWindowTextA(hEditControl, input, sizeof(input));
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
    
    PromptUser(list, "🔍 Google", g_cached_google_count, 520, 650, 32, 
               google_refresh_callback, 
               google_enter_callback, 
               NULL, 
               NULL, 
               NULL);
    
    free(list);
}

#endif
