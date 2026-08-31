#include "core/flapi.h"

char *allowed_video_formats[] = {"webm","mp4", NULL};

static ListOfFiles s_video_files = {0};

static void video_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    s_video_files.count = 0;
    fl_get_all_files_from_dir(VideosPath, allowed_video_formats, &s_video_files, NULL);
    
    const char** items = (const char**)malloc(s_video_files.count * sizeof(char*));
    if (!items) return;
    for (size_t i = 0; i < s_video_files.count; i++) {
        items[i] = s_video_files.items[i].name;
    }
    PopulateListBox(lb, hListBox, filter, items, (int)s_video_files.count, NULL);
    free(items);
}

static void video_enter_callback(const char* selection, ListBoxInfo* lb) {
    char* result = NULL;
    for (size_t i = 0; i < s_video_files.count; i++) {
        if (strcmp(s_video_files.items[i].name, selection) == 0) {
            result = s_video_files.items[i].path;
            break;
        }
    }
    if (!result) {
        ListOfFiles tmp = {0};
        fl_get_all_files_from_dir(VideosPath, allowed_video_formats, &tmp, NULL);
        for (size_t i = 0; i < tmp.count; i++) {
            if (strcmp(tmp.items[i].name, selection) == 0) {
                result = tmp.items[i].path;
                break;
            }
        }
        if (result) {
            static char found_path[MAX_PATH];
            strncpy(found_path, result, sizeof(found_path)-1);
            found_path[sizeof(found_path)-1] = '\0';
            result = found_path;
        }
        free(tmp.items);
    }
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
        fl_launch_with_args(VideoViewerPath, quoted_result);
    }
#else
    if (Platform_SetX11StringProperty("Video Viewer", "_MP_TRACK", result) &&
        Platform_SendX11Command("Video Viewer", "_MP_TRACK_CHANGED", 1)) {
        return;
    }
    if (strlen(VideoViewerPath) > 0) {
        char quoted_result[MAX_PATH + 3] = {0};
        snprintf(quoted_result, sizeof(quoted_result), "\"%s\"", result);
        fl_launch_with_args(VideoViewerPath, quoted_result);
    }
#endif
}
bool HandleVideoViewer(int vk_code, int modifiers, const char* current_title) {
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
                
                s_video_files.count = 0;
                fl_get_all_files_from_dir(VideosPath, allowed_video_formats, &s_video_files, NULL);
                
                const char** initial_items = (const char**)malloc(s_video_files.count * sizeof(char*));
                if (!initial_items) return false;
                
                for (size_t i = 0; i < s_video_files.count; i++) {
                    initial_items[i] = s_video_files.items[i].name;
                }
                
                PROMPT(.title = "🎥 Viewer", .items = initial_items, .count = (int)s_video_files.count,
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
