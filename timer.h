#ifndef TIMER_H
#define TIMER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "app_launcher.h"
#include "types.h"

extern char* PromptQuickSwitcher(const char* window_list[], int count, int height, int width, 
                                 RefreshCallback refresh_callback, 
                                 EnterCallback enter_callback,
                                 CtrlEnterCallback ctrl_enter_callback,
                                 TabCallback tab_callback);
extern HWND hPopupWnd;
static void timer_enter_callback(const char* selection) {
  
    if (selection != NULL && strlen(selection) > 0) {
        // Find sowon in cache
        char sowon_path[MAX_PATH] = {0};
        for (int i = 0; i < G_APP_COUNT; i++) {
            if (StrStrIA(G_APPS[i].name, "sowon") != NULL) {
                strncpy(sowon_path, G_APPS[i].path, sizeof(sowon_path) - 1);
                break;
            }
        }

        if (strlen(sowon_path) == 0) {
            printf("[TIMER] ERROR: sowon.exe not found in cache!\n");
            return;
        }

        // Launch sowon with the selection as argument
        launch_exe_with_args(sowon_path, selection);
        printf("[TIMER] Launching: %s %s\n", sowon_path, selection);
    }
}
static void timer_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
    // Show the current input or a message
    if (filter && strlen(filter) > 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Running: sowon.exe %s", filter);
        SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)msg);
    } else {
        SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)"Enter timer duration...");
    }
    SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
}
static void TriggerTimer(HWND hPopupWnd) {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        return;
    }
    
    const char* prompt[1] = { "Enter timer duration (e.g., 5m, 10h,20s or 0 for a stopwatch" };
    
    PromptQuickSwitcher(prompt, 1, 220, 150,timer_refresh_callback,timer_enter_callback,NULL,NULL);

}

#endif
