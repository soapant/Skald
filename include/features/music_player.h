#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H

#include "core/flapi.h"

static const MusicPlayerBinding G_MUSIC_PLAYERS[] = {
  { VK_OEM_COMMA, HOTKEY_MOD_ALT, 1 },
  { VK_OEM_2, HOTKEY_MOD_ALT, 5 },
  { VK_OEM_1, HOTKEY_MOD_ALT, 6 },
  { VK_OEM_7, HOTKEY_MOD_ALT, 2 },
  { 'M', HOTKEY_MOD_ALT, 69 },
};
static const unsigned int MUSIC_PLAYER_COUNT = sizeof(G_MUSIC_PLAYERS) / sizeof(G_MUSIC_PLAYERS[0]);

char *allowed_audio_formats[] = {"mp3","ogg","flac", NULL};

static ListOfFiles s_music_files = {0};

static void music_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
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

static void music_enter_callback(const char* selection, ListBoxInfo* lb) {
    char* result = NULL;
    for (size_t i = 0; i < s_music_files.count; i++) {
        if (strcmp(s_music_files.items[i].name, selection) == 0) {
            result = s_music_files.items[i].path;
            break;
        }
    }
    // Fallback: rescan if not found (e.g., list was cleared)
    if (!result) {
        ListOfFiles tmp = {0};
        fl_get_all_files_from_dir(MusicPath, allowed_audio_formats, &tmp, NULL);
        for (size_t i = 0; i < tmp.count; i++) {
            if (strcmp(tmp.items[i].name, selection) == 0) {
                result = tmp.items[i].path;
                break;
            }
        }
        // Note: tmp will leak a bit but selection is used immediately and tmp is on stack, items will be leaked - ok for now, will be cleaned on next refresh
        // To avoid leak, we could keep it, but we just need the path for immediate use, so we copy it
        if (result) {
            static char found_path[MAX_PATH];
            strncpy(found_path, result, sizeof(found_path)-1);
            found_path[sizeof(found_path)-1] = '\0';
            result = found_path;
        }
        // free tmp items
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

bool HandleMusicPlayer(int vk_code, int modifiers, const char* current_title) {
    if (!MusicPlayerAvailable) return false;
    // very simple helper from flapi/helpers - one-liner binding lookup
    int _idx = fl_find_music_idx(vk_code, modifiers, G_MUSIC_PLAYERS, MUSIC_PLAYER_COUNT);
    if (_idx < 0) return false;
    {
        const MusicPlayerBinding* binding = &G_MUSIC_PLAYERS[_idx];
#if defined(_WIN32)
            HWND playerHwnd = FindWindow(NULL, "Music Player");
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
                return Platform_SendX11Command("Music Player", "_MP_PLAYER_CMD", binding->code);
#endif
            } else {
                s_music_files.count = 0;
                fl_get_all_files_from_dir(MusicPath, allowed_audio_formats, &s_music_files, NULL);
                
                const char** initial_items = (const char**)malloc(s_music_files.count * sizeof(char*));
                if (!initial_items) return false;
                
                for (size_t i = 0; i < s_music_files.count; i++) {
                    initial_items[i] = s_music_files.items[i].name;
                }

                PROMPT(.title = "🎵 Player", .items = initial_items, .count = (int)s_music_files.count,
                       .height = 920, .width = 650, .item_height = 32,
                       .refresh = music_refresh_callback,
                       .enter = music_enter_callback);
                
                free((void*)initial_items);

                return true;
            }
        }
    return false;
}

#endif
