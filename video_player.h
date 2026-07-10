#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <shlwapi.h>
#include "config.h"
#include "file_search.h"
#include "app_launcher.h"

extern char* PromptQuickSwitcher(const char** items, int count, int width, int height);
extern HWND hPopupWnd;

static bool HandleVideoPlayer(int vk_code, int modifiers, const char* current_title) {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        return false;
    }
    
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
                ClearFileList();
                GetFilesRecursive("C:\\videos");

                const char* prompt[MAX_FILES];
                for (int i = 0; i < GetFileCount(); i++) {
                    prompt[i] = G_FILES[i].name;
                }

                char* input = PromptQuickSwitcher(prompt, GetFileCount(), 220, 350);
                
                if (input != NULL && strlen(input) > 0) {
                    char* result = SearchFileRecursive("C:\\videos", input);
                    
                    if (result) {
                        if (playerHwnd) {
                            COPYDATASTRUCT cds;
                            cds.dwData = 0;                          
                            cds.cbData = (DWORD)(strlen(result) + 1);
                            cds.lpData = (PVOID)result;               
                            SendMessage(playerHwnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
                        } else {
                            char video_player_path[MAX_PATH] = {0};
                            for (int i = 0; i < G_APP_COUNT; i++) {
                                if (StrStrIA(G_APPS[i].name, "video_viewer") != NULL) {
                                    strncpy(video_player_path, G_APPS[i].path, sizeof(video_player_path) - 1);
                                    break;
                                }
                            }
                            char quoted_result[MAX_PATH + 3] = {0};
                            snprintf(quoted_result, sizeof(quoted_result), "\"%s\"", result);
                            launch_exe_with_args(video_player_path, quoted_result);
                        }
                    }
                }
                return true;
            }
        }
    }
    return false;
}

#endif
