#ifndef VIDEO_VIEWER_H
#define VIDEO_VIEWER_H

#include "core/flapi.h"

static char *allowed_video_formats[] = {"webm","mp4", NULL};

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
static void video_cmd_1(void) {
    if (!VideoViewerAvailable) return;
#if defined(_WIN32)
    HWND playerHwnd = FindWindow(NULL, "Video Viewer");
    if (playerHwnd) PostMessage(playerHwnd, WM_USER + 1, 1, 0);
#else
    Platform_SetX11Property("Video Viewer", "_MP_CMD", 1);
#endif
}
static void video_cmd_5(void) {
    if (!VideoViewerAvailable) return;
#if defined(_WIN32)
    HWND playerHwnd = FindWindow(NULL, "Video Viewer");
    if (playerHwnd) PostMessage(playerHwnd, WM_USER + 1, 5, 0);
#else
    Platform_SetX11Property("Video Viewer", "_MP_CMD", 5);
#endif
}
static void video_cmd_6(void) {
    if (!VideoViewerAvailable) return;
#if defined(_WIN32)
    HWND playerHwnd = FindWindow(NULL, "Video Viewer");
    if (playerHwnd) PostMessage(playerHwnd, WM_USER + 1, 6, 0);
#else
    Platform_SetX11Property("Video Viewer", "_MP_CMD", 6);
#endif
}
static void video_prompt(void) {
    if (!VideoViewerAvailable) return;
    s_video_files.count = 0;
    fl_get_all_files_from_dir(VideosPath, allowed_video_formats, &s_video_files, NULL);
    const char** initial_items = (const char**)malloc(s_video_files.count * sizeof(char*));
    if (!initial_items) return;
    for (size_t i = 0; i < s_video_files.count; i++) {
        initial_items[i] = s_video_files.items[i].name;
    }
    PROMPT(.title = "🎥 Viewer", .items = initial_items, .count = (int)s_video_files.count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = video_refresh_callback,
           .enter = video_enter_callback);
    free((void*)initial_items);
}

static inline void video_viewer_init(void) {
    fl_register_hotkey("Alt+Shift+,", video_cmd_1, NULL, NULL);
    fl_register_hotkey("Alt+Shift+.", video_cmd_5, NULL, NULL);
    fl_register_hotkey("Alt+Shift+;", video_cmd_6, NULL, NULL);
    fl_register_hotkey("Alt+V", video_prompt, NULL, NULL);
}

#endif
