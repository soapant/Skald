#ifndef GOOGLE_SEARCH_H
#define GOOGLE_SEARCH_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <shellapi.h>
#include "types.h"

extern char* PromptUser(const char* window_list[], int count, int height, int width, 
                                 RefreshCallback refresh_callback, 
                                 EnterCallback enter_callback,
                                 CtrlEnterCallback ctrl_enter_callback,
                                 TabCallback tab_callback);
extern HWND hPopupWnd;
extern HWND hEditControl;
extern HWND hListBox;
extern bool selection_complete;
extern char output_buffer[256];

static void TriggerGoogleSearch() {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        return;
    }

    const char* empty_list[1] = { "Enter search query..." };
    char* query = PromptUser(empty_list, 1, 220, 150,NULL,NULL,NULL,NULL);

    if (query != NULL && strlen(query) > 0) {
        // Build Google search URL
        char url[512];
        snprintf(url, sizeof(url), "https://www.google.com/search?q=%s", query);
        
        // URL encode spaces
        for (int i = 0; url[i]; i++) {
            if (url[i] == ' ') {
                url[i] = '+';
            }
        }
        
        // Open in default browser
        ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOW);

        Sleep(200);

        const char* browser_classes[] = {
            "Chrome_WidgetWin_1",  // Chrome
            "MozillaWindowClass",   // Firefox
            "IEFrame",              // Internet Explorer / Edge
            "ApplicationFrameWindow" // Edge (new)
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
}

#endif
