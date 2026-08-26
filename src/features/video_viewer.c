#include "common.h"
#include "features/video_viewer.h"
#include "config.h"
#include "core/common_ui.h"
#include "features/file_search.h"
#include "features/app_launcher.h"
#include "platform/platform.h"

char *allowed_video_formats[] = {"webm","mp4"};

static void video_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    ClearFileList();
    GetFilesRecursive(VideosPath, false,allowed_video_formats);
    
    const char** items = (const char**)malloc(GetFileCount() * sizeof(char*));
    if (!items) return;
    for (int i = 0; i < GetFileCount(); i++) {
        items[i] = G_FILES_WITH_TIME[i].name;
    }
    PopulateListBox(lb, hListBox, filter, items, GetFileCount(), NULL);
    free(items);
}

static void video_enter_callback(const char* selection, ListBoxInfo* lb) {
    char* result = SearchFileInDirectory(VideosPath, selection);
    if (!result) return;
    
#if defined(_WIN32)
    HWND playerHwnd = FindWindow(NULL, "Video Viewer");
    if (playerHwnd) {
        COPYDATASTRUCT cds;
        cds.dwData = 0;
        cds.cbData = (DWORD)(strlen(result) + 1);
        cds.lpData = (PVOID)result;
        SendMessage(playerHwnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
        return;
    } else {
        char quoted_result[MAX_PATH + 3] = {0};
        snprintf(quoted_result, sizeof(quoted_result), "\"%s\"", result);
        launch_exe_with_args(VideoViewerPath, quoted_result);
    }
#else
    if (Platform_SetX11StringProperty("Video Viewer", "_MP_TRACK", result) &&
        Platform_SendX11Command("Video Viewer", "_MP_TRACK_CHANGED", 1)) {
        return;
    }
    if (strlen(VideoViewerPath) > 0) {
        char quoted_result[MAX_PATH + 3] = {0};
        snprintf(quoted_result, sizeof(quoted_result), "\"%s\"", result);
        launch_exe_with_args(VideoViewerPath, quoted_result);
    }
#endif
}
bool HandleVideoViewer(int vk_code, int modifiers, const char* current_title) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return false;
    if (!VideoViewerAvailable) return false;
    
    for (int i = 0; i < VIDEO_PLAYER_COUNT; i++) {
        const VideoViewerBinding* binding = &G_VIDEO_PLAYERS[i];
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
                const char* prompt[1] = { "Your Directory Is Empty (Has No Videos)" };
                
                ClearFileList();
                GetFilesRecursive(VideosPath,false,allowed_video_formats);
                
                const char** initial_items = (const char**)malloc(GetFileCount() * sizeof(char*));
                if (!initial_items) return false;
                
                for (int i = 0; i < GetFileCount(); i++) {
                    initial_items[i] = G_FILES_WITH_TIME[i].name;
                }
                
                PROMPT(.title = "🎥 Viewer", .items = initial_items, .count = GetFileCount(),
                       .height = 920, .width = 650, .item_height = 32,
                       .refresh = video_refresh_callback,
                       .enter = video_enter_callback);
                
                free((void*)initial_items);

                return true;
            }
        }
    }
    return false;
}
