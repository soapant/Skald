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

extern HWND hPopupWnd;

static void music_refresh_callback(const char* filter, HWND hListBox) {
    ClearFileList();
    GetFilesRecursive(MusicPath, false);
    
    const char** items = (const char**)malloc(GetFileCount() * sizeof(char*));
    if (!items) return;
    for (int i = 0; i < GetFileCount(); i++) {
        items[i] = G_FILES_WITH_TIME[i].name;
    }
    PopulateListBox(hListBox, filter, items, GetFileCount(), NULL);
    free(items);
}

static void music_enter_callback(const char* selection) {
    if (selection == NULL || strlen(selection) == 0) return;
    
    HWND playerHwnd = FindWindow(NULL, "Music Player");
    char* result = SearchFileInDirectory(MusicPath, selection);
    
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
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return false;
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
                const char* prompt[1] = { "Search for music..." };
                
                ClearFileList();
                GetFilesRecursive(MusicPath,false);
                
                const char** initial_items = (const char**)malloc(GetFileCount() * sizeof(char*));
                if (!initial_items) return false;
                
                for (int i = 0; i < GetFileCount(); i++) {
                    initial_items[i] = G_FILES_WITH_TIME[i].name;
                }
                
                PromptUser(initial_items,"🎵 Player",GetFileCount(), 520, 650,32,
                                    music_refresh_callback,  // refresh -> filters on typing
                                    music_enter_callback,    // enter -> plays music
                                    NULL,                    // ctrl enter
                                    NULL,
                                    NULL);                   // tab
                
                free((void*)initial_items);
                return true;
            }
        }
    }
    return false;
}

#endif
