#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <shlwapi.h>
#include "config.h"
#include "file_search.h"
#include "app_launcher.h"
#include "types.h"

extern char* PromptQuickSwitcher(const char* window_list[], int count, int height, int width, 
                                 RefreshCallback refresh_callback, 
                                 EnterCallback enter_callback,
                                 CtrlEnterCallback ctrl_enter_callback,
                                 TabCallback tab_callback);
extern HWND hPopupWnd;

static void music_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    // Get the current file list
    ClearFileList();
    GetFilesRecursive("D:\\Music");
    
    SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
    
    if (filter == NULL || strlen(filter) == 0) {
        for (int i = 0; i < GetFileCount(); i++) {
            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)G_FILES[i].name);
        }
    } else {
        for (int i = 0; i < GetFileCount(); i++) {
            if (StrStrIA(G_FILES[i].name, filter) != NULL) {
                SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)G_FILES[i].name);
            }
        }
    }
    SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
}

static void music_enter_callback(const char* selection) {
    if (selection == NULL || strlen(selection) == 0) return;
    
    HWND playerHwnd = FindWindow(NULL, "Music Player");
    char* result = SearchFileInDirectory("D:\\Music", selection);
    
    if (!result) return;
    
    if (playerHwnd) {
        COPYDATASTRUCT cds;
        cds.dwData = 0;
        cds.cbData = (DWORD)(strlen(result) + 1);
        cds.lpData = (PVOID)result;
        SendMessage(playerHwnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
        printf("[MUSIC] Sent to existing player: %s\n", result);
    } else {
        char music_player_path[MAX_PATH] = {0};
        for (int i = 0; i < G_APP_COUNT; i++) {
            if (StrStrIA(G_APPS[i].name, "music_player") != NULL) {
                strncpy(music_player_path, G_APPS[i].path, sizeof(music_player_path) - 1);
                break;
            }
        }
        if (strlen(music_player_path) > 0) {
            char quoted_result[MAX_PATH + 3] = {0};
            snprintf(quoted_result, sizeof(quoted_result), "\"%s\"", result);
            launch_exe_with_args(music_player_path, quoted_result);
            printf("[MUSIC] Launched new player with: %s\n", result);
        }
    }
}

static bool HandleMusicPlayer(int vk_code, int modifiers, const char* current_title) {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        return false;
    }
    
    for (int i = 0; i < MUSIC_PLAYER_COUNT; i++) {
        const MusicPlayerBinding* binding = &G_MUSIC_PLAYERS[i];
        if (binding->vk_code == vk_code && binding->modifiers == modifiers) {
            HWND playerHwnd = FindWindow(NULL, "Music Player");
            
            if (binding->code != 69) {
                if (playerHwnd) {
                    PostMessage(playerHwnd, WM_USER + 1, binding->code, 0);
                    return true;
                } else {
                    return false;
                }
            } else {
                // Code 69 - open file picker with refresh
                const char* prompt[1] = { "Search for music..." };
                
                // Initialize the file list for the first time
                ClearFileList();
                GetFilesRecursive("D:\\Music");
                
                const char** initial_items = (const char**)malloc(GetFileCount() * sizeof(char*));
                if (!initial_items) return false;
                
                for (int i = 0; i < GetFileCount(); i++) {
                    initial_items[i] = G_FILES[i].name;
                }
                
                PromptQuickSwitcher(initial_items, GetFileCount(), 220, 350,
                                    music_refresh_callback,  // refresh -> filters on typing
                                    music_enter_callback,    // enter -> plays music
                                    NULL,                    // ctrl enter
                                    NULL);                   // tab
                
                free((void*)initial_items);
                return true;
            }
        }
    }
    return false;
}

#endif
