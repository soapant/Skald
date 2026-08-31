#include "common.h"
#include "features/music_player.h"
#include "core/common_ui.h"
#include "features/file_search.h"
#include "features/app_launcher.h"
#include "core/platform.h"

char *allowed_audio_formats[] = {"mp3","ogg","flac"};

static void music_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    ClearFileList();
    GetFilesRecursive(MusicPath, false,allowed_audio_formats);
    
    const char** items = (const char**)malloc(GetFileCount() * sizeof(char*));
    if (!items) return;
    for (int i = 0; i < GetFileCount(); i++) {
        items[i] = G_FILES_WITH_TIME[i].name;
    }
    PopulateListBox(lb, hListBox, filter, items, GetFileCount(), NULL);
    free(items);
}

static void music_enter_callback(const char* selection, ListBoxInfo* lb) {
    char* result = SearchFileInDirectory(MusicPath, selection);
    
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
        launch_exe_with_args(MusicPlayerPath, quoted_result);
    }
#else
    if (Platform_SetX11StringProperty("Music Player", "_MP_TRACK", result) &&
        Platform_SendX11Command("Music Player", "_MP_TRACK_CHANGED", 1)) {
        return;
    }
    if (strlen(MusicPlayerPath) > 0) {
        char quoted_result[MAX_PATH + 3] = {0};
        snprintf(quoted_result, sizeof(quoted_result), "\"%s\"", result);
        launch_exe_with_args(MusicPlayerPath, quoted_result);
    }
#endif
}

bool HandleMusicPlayer(int vk_code, int modifiers, const char* current_title) {
    if (!MusicPlayerAvailable) return false;
    for (int i = 0; i < MUSIC_PLAYER_COUNT; i++) {
        const MusicPlayerBinding* binding = &G_MUSIC_PLAYERS[i];
        if (binding->vk_code == vk_code && binding->modifiers == modifiers) {
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
                ClearFileList();
                GetFilesRecursive(MusicPath,false,allowed_audio_formats);
                
                const char** initial_items = (const char**)malloc(GetFileCount() * sizeof(char*));
                if (!initial_items) return false;
                
                for (int i = 0; i < GetFileCount(); i++) {
                    initial_items[i] = G_FILES_WITH_TIME[i].name;
                }

                PROMPT(.title = "🎵 Player", .items = initial_items, .count = GetFileCount(),
                       .height = 920, .width = 650, .item_height = 32,
                       .refresh = music_refresh_callback,
                       .enter = music_enter_callback);
                
                free((void*)initial_items);

                return true;
            }
        }
    }
    return false;
}
