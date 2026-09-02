#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H

#include "core/flapi.h"

typedef struct {
    int vk_code;
    int modifiers;
    int code;
} MusicPlayerBinding;


char *allowed_audio_formats[] = {"mp3","ogg","flac", NULL};

ListOfFiles s_music_files = {0};

void music_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    s_music_files.count = 0;
    fl_get_all_files_from_dir(MusicPath, allowed_audio_formats, &s_music_files, NULL);
    
    const char** items = (const char**)malloc(s_music_files.count * sizeof(char*));
    if (!items) return;
    for (size_t i = 0; i < s_music_files.count; i++) {
        items[i] = s_music_files.items[i].name;
    }
    PopulateListBox(lb, hListBox, filter, items, (int)s_music_files.count, NULL);
    free(items);
}

void music_enter_callback(const char* selection, ListBoxInfo* lb) {
    char* result = NULL;
    for (size_t i = 0; i < s_music_files.count; i++) {
        if (strcmp(s_music_files.items[i].name, selection) == 0) {
            result = s_music_files.items[i].path;
            break;
        }
    }
    if (!result) {
        ListOfFiles tmp = {0};
        fl_get_all_files_from_dir(MusicPath, allowed_audio_formats, &tmp, NULL);
        for (size_t i = 0; i < tmp.count; i++) {
            if (strcmp(tmp.items[i].name, selection) == 0) {
                result = tmp.items[i].path;
                break;
            }
        }
        if (result) {
            char found_path[MAX_PATH];
            strncpy(found_path, result, sizeof(found_path)-1);
            found_path[sizeof(found_path)-1] = '\0';
            result = found_path;
        }
        free(tmp.items);
    }
    
    if (!result) return;
    
#if defined(_WIN32)
    HWND playerHwnd = FindWindow(NULL, "Music Player");
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
        fl_launch_with_args(MusicPlayerPath, quoted_result);
    }
#else
    if (Platform_SetX11StringProperty("Music Player", "_MP_TRACK", result) &&
        Platform_SendX11Command("Music Player", "_MP_TRACK_CHANGED", 1)) {
        return;
    }
    if (strlen(MusicPlayerPath) > 0) {
        char quoted_result[MAX_PATH + 3] = {0};
        snprintf(quoted_result, sizeof(quoted_result), "\"%s\"", result);
        fl_launch_with_args(MusicPlayerPath, quoted_result);
    }
#endif
}

void music_cmd_1(void) {
#if defined(_WIN32)
    HWND playerHwnd = FindWindow(NULL, "Music Player");
    if (playerHwnd) PostMessage(playerHwnd, WM_USER + 1, 1, 0);
#else
    Platform_SendX11Command("Music Player", "_MP_PLAYER_CMD", 1);
#endif
}
void music_cmd_5(void) {
#if defined(_WIN32)
    HWND playerHwnd = FindWindow(NULL, "Music Player");
    if (playerHwnd) PostMessage(playerHwnd, WM_USER + 1, 5, 0);
#else
    Platform_SendX11Command("Music Player", "_MP_PLAYER_CMD", 5);
#endif
}
void music_cmd_6(void) {
#if defined(_WIN32)
    HWND playerHwnd = FindWindow(NULL, "Music Player");
    if (playerHwnd) PostMessage(playerHwnd, WM_USER + 1, 6, 0);
#else
    Platform_SendX11Command("Music Player", "_MP_PLAYER_CMD", 6);
#endif
}
void music_cmd_2(void) {
#if defined(_WIN32)
    HWND playerHwnd = FindWindow(NULL, "Music Player");
    if (playerHwnd) PostMessage(playerHwnd, WM_USER + 1, 2, 0);
#else
    Platform_SendX11Command("Music Player", "_MP_PLAYER_CMD", 2);
#endif
}
void music_prompt(void) {
    if (!MusicPlayerAvailable) return;
    s_music_files.count = 0;
    fl_get_all_files_from_dir(MusicPath, allowed_audio_formats, &s_music_files, NULL);
    const char** initial_items = (const char**)malloc(s_music_files.count * sizeof(char*));
    if (!initial_items) return;
    for (size_t i = 0; i < s_music_files.count; i++) {
        initial_items[i] = s_music_files.items[i].name;
    }
    PROMPT(.title = "🎵 Player", .items = initial_items, .count = (int)s_music_files.count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = music_refresh_callback,
           .enter = music_enter_callback);
    free((void*)initial_items);
}

static inline void music_player_init(void) {
    fl_register_hotkey("Alt+,", music_cmd_1, NULL, NULL);
    fl_register_hotkey("Alt+/", music_cmd_5, NULL, NULL);
    fl_register_hotkey("Alt+;", music_cmd_6, NULL, NULL);
    fl_register_hotkey("Alt+'", music_cmd_2, NULL, NULL);
    fl_register_hotkey("Alt+P", music_prompt, NULL, NULL);
}

#endif
