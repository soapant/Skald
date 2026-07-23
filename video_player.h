#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <shlwapi.h>
#include "config.h"
#include "file_search.h"
#include "app_launcher.h"
#include "types.h"

extern HWND hPopupWnd;
extern HWND hEditControl;

static void video_refresh_callback(const char* filter, HWND hListBox) {
    ClearFileList();
    GetFilesRecursive(VideosPath, false);
    
    const char** items = (const char**)malloc(GetFileCount() * sizeof(char*));
    if (!items) return;
    for (int i = 0; i < GetFileCount(); i++) {
        items[i] = G_FILES_WITH_TIME[i].name;
    }
    PopulateListBox(hListBox, filter, items, GetFileCount(), NULL);
    free(items);
}

static void video_enter_callback(const char* selection) {
    if (selection == NULL || strlen(selection) == 0) return;
    
    HWND playerHwnd = FindWindow(NULL, "Video Viewer");
    char* result = SearchFileRecursive(VideosPath, selection);
    
    if (!result) return;
    
    if (playerHwnd) {
        COPYDATASTRUCT cds;
        cds.dwData = 0;
        cds.cbData = (DWORD)(strlen(result) + 1);
        cds.lpData = (PVOID)result;
        SendMessage(playerHwnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
        printf("[VIDEO] Sent to existing player: %s\n", result);
    } else {
        char video_player_path[MAX_PATH] = {0};
        for (int i = 0; i < G_APP_COUNT; i++) {
            if (StrStrIA(G_APPS[i].name, "video_viewer") != NULL) {
                strncpy(video_player_path, G_APPS[i].path, sizeof(video_player_path) - 1);
                break;
            }
        }
        if (strlen(video_player_path) > 0) {
            char quoted_result[MAX_PATH + 3] = {0};
            snprintf(quoted_result, sizeof(quoted_result), "\"%s\"", result);
            launch_exe_with_args(video_player_path, quoted_result);
            printf("[VIDEO] Launched new player with: %s\n", result);
        }
    }
}
static bool HandleVideoPlayer(int vk_code, int modifiers, const char* current_title) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return false;
    
    for (int i = 0; i < VIDEO_PLAYER_COUNT; i++) {
        const VideoPlayerBinding* binding = &G_VIDEO_PLAYERS[i];
        if (binding->vk_code == vk_code && binding->modifiers == modifiers) {
            HWND playerHwnd = FindWindow(NULL, "Video Viewer");
            
            if (binding->code != 69) {
                if (playerHwnd) {
                    PostMessage(playerHwnd, WM_USER + 1, binding->code, 0);
                    return true;
                } else {
                    return false;
                }
            } else {
                // Code 69 - open file picker with refresh
                const char* prompt[1] = { "Search for video..." };
                
                // Initialize the file list for the first time
                ClearFileList();
                GetFilesRecursive(VideosPath,false);
                
                const char** initial_items = (const char**)malloc(GetFileCount() * sizeof(char*));
                if (!initial_items) return false;
                
                for (int i = 0; i < GetFileCount(); i++) {
                    initial_items[i] = G_FILES_WITH_TIME[i].name;
                }
                
                PromptUser(initial_items,"🎥 Player",GetFileCount(), 920, 650,32,
                                    video_refresh_callback,  
                                    video_enter_callback,    
                                    NULL,                    
                                    NULL,
                                    NULL);                   
                
                free((void*)initial_items);
                return true;
            }
        }
    }
    return false;
}

#endif
