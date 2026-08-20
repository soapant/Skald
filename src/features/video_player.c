#include "common.h"
#include "features/video_player.h"
#include "config.h"
#include "core/common_ui.h"
#include "features/file_search.h"
#include "features/app_launcher.h"
#include "platform/platform.h"

static void video_refresh_callback(const char* filter, WindowHandle hListBox) {
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
    char* result = SearchFileRecursive(VideosPath, selection);
    
    if (!result) return;
    
#if defined(_WIN32)
    HWND playerHwnd = FindWindow(NULL, "Video Viewer");
    if (playerHwnd) {
        COPYDATASTRUCT cds;
        cds.dwData = 0;
        cds.cbData = (DWORD)(strlen(result) + 1);
        cds.lpData = (PVOID)result;
        SendMessage(playerHwnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
        printf("[VIDEO] Sent to existing player: %s\n", result);
        return;
    }
#else
    if (Platform_SetX11StringProperty("Video Viewer", "_MP_TRACK", result)) {
        printf("[VIDEO] Sent to existing player: %s\n", result);
        return;
    }
#endif
    {
        if (strlen(VideoViewerPath) > 0) {
            char quoted_result[MAX_PATH + 3] = {0};
            snprintf(quoted_result, sizeof(quoted_result), "\"%s\"", result);
            launch_exe_with_args(VideoViewerPath, quoted_result);
            printf("[VIDEO] Launched new player with: %s\n", result);
        }
    }
}
bool HandleVideoPlayer(int vk_code, int modifiers, const char* current_title) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return false;
    if (!VideoViewerAvailable) return false;
    
    for (int i = 0; i < VIDEO_PLAYER_COUNT; i++) {
        const VideoPlayerBinding* binding = &G_VIDEO_PLAYERS[i];
        if (binding->vk_code == vk_code && binding->modifiers == modifiers) {
#if defined(_WIN32)
            HWND playerHwnd = FindWindow(NULL, "Video Viewer");
#endif
            
            if (binding->code != 69) {
#if defined(_WIN32)
                if (playerHwnd) {
                    PostMessage(playerHwnd, WM_USER + 1, binding->code, 0);
                    return true;
                } else {
                    return false;
                }
#else
                return Platform_SetX11Property("Video Viewer", "_MP_CMD", binding->code);
#endif
            } else {
                const char* prompt[1] = { "Search for video..." };
                
                ClearFileList();
                GetFilesRecursive(VideosPath,false);
                
                const char** initial_items = (const char**)malloc(GetFileCount() * sizeof(char*));
                if (!initial_items) return false;
                
                for (int i = 0; i < GetFileCount(); i++) {
                    initial_items[i] = G_FILES_WITH_TIME[i].name;
                }
                
                ImportantPromptUser(initial_items,"🎥 Viewer",GetFileCount(), 920, 650,32,
                                    video_refresh_callback,  
                                    video_enter_callback,    
                                    NULL,NULL);
                
                free((void*)initial_items);

                return true;
            }
        }
    }
    return false;
}
