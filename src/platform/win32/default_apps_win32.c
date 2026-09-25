#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "core/platform.h"
#include "config.h"

char default_apps_win32[DEFAULT_APP_COUNT][256];

const char* win32_default_apps_read_sz(const char* subkey,
                                             const char* value_name,
                                             int slot) {
    default_apps_win32[slot][0] = '\0';

    DWORD size = (DWORD)sizeof(default_apps_win32[slot]);
    LONG rc = RegGetValueA(HKEY_CURRENT_USER,
                           subkey,
                           value_name,
                           RRF_RT_REG_SZ,
                           NULL,
                           default_apps_win32[slot],
                           &size);
    if (rc != ERROR_SUCCESS) {
        default_apps_win32[slot][0] = '\0';
    }
    return default_apps_win32[slot];
}

const char* win32_terminal_name_from_guid(const char* console_guid, const char* terminal_guid) {
    if (strstr(console_guid,  "2EACA947-7F5F-4CFA-BA87-8F7FBEEFBE69") ||
        strstr(terminal_guid, "E12CFF52-A866-4C77-9A90-F570A7AA2C6B")) {
        return "Windows Terminal";
    }
    if (strstr(console_guid,  "B23D10C0-E52E-411E-9D5B-C09FDF709C7D")) {
        return "Legacy Console Host";
    }
    return NULL;
}

const char* win32_default_apps_progid(const char* subkey, int slot) {
    return win32_default_apps_read_sz(subkey, "ProgId", slot);
}

static const char* scan_start_menu_for_app(const char* const possibilities[], size_t count) {
    ListOfFiles files = {0};
    Platform_GetAllFilesFromDir("C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs", NULL, &files, NULL);

    for (size_t j = 0; j < count; j++) {
        for (int i = 0; i < files.count; i++) {
            if (Platform_StringContainsCaseInsensitive(files.files[i].name, possibilities[j])) {
                char* name = files.files[i].name;
                name[0] = (char)tolower((unsigned char)name[0]);
                return name;
            }
        }
    }
    return NULL;
}

const char* get_default_terminal(void) {
    const char* possible_terminals[] = {
        "Alacritty", "WezTerm", "Tabby", "Warp", "Ghostty", "Hyper", "Yaw",
        "Wave Terminal", "Fluent Terminal", "Rio",
        "Cmder", "ConEmu", "PuTTY",
        "MobaXterm", "Git Bash Terminal (mintty)", "SecureCRT", "Xshell",
        "ZOC Terminal", "KittyY", "PowerShell (Modern pwsh)",
        "Windows PowerShell "
    };
    
    const char* found = scan_start_menu_for_app(possible_terminals, ARRAY_LEN(possible_terminals));
    return found ? found : "pwsh";
}

const char* get_default_browser(void) {
    const char* possible_browsers[] = {
        "Google Chrome", "Mozilla Firefox", "Microsoft Edge", "Brave", "Opera",
        "Vivaldi", "Tor Browser", "Waterfox", "LibreWolf", "Arc", "Sidekick",
        "Pale Moon", "Internet Explorer","Zen"
    };

    const char* found = scan_start_menu_for_app(possible_browsers, ARRAY_LEN(possible_browsers));
    return found ? found : "chrome";
}

const char* get_default_video_player(void) {
    const char* possible_video_players[] = {
        "VLC media player", "Media Player", "Movies & TV", "MPC-HC", "MPC-BE",
        "PotPlayer", "KMPlayer", "GOM Player", "MPV", "5KPlayer", " Kodi",
        "Windows Media Player"
    };

    const char* found = scan_start_menu_for_app(possible_video_players, ARRAY_LEN(possible_video_players));
    return found ? found : "vlc";
}

const char* get_default_music_player(void) {
    const char* possible_music_players[] = {
        "Spotify", "Media Player", "Groove Music", "Foobar2000", "Winamp",
        "AIMP", "MusicBee", "iTunes", "TIDAL", "Amazon Music", "Apple Music",
        "Clementine", "VLC media player"
    };

    const char* found = scan_start_menu_for_app(possible_music_players, ARRAY_LEN(possible_music_players));
    return found ? found : "vlc";
}

const char* get_default_image_viewer(void) {
    const char* possible_image_viewers[] = {
        "Photos", "Windows Photo Viewer", "IrfanView", "XnView", "FastStone",
        "Honeyview", "ImageGlass", "DigiKam"
    };

    const char* found = scan_start_menu_for_app(possible_image_viewers, ARRAY_LEN(possible_image_viewers));
    return found ? found : "mspaint";
}

const char* Platform_GetDefaultTerminal(void) {
    int slot = DEFAULT_TERMINAL;
    default_apps_win32[slot][0] = '\0';

    HKEY hKey = NULL;
    if (RegOpenKeyExA(HKEY_CURRENT_USER,
                      "Console\\%%Startup",
                      0,
                      KEY_READ,
                      &hKey) != ERROR_SUCCESS) {
        snprintf(default_apps_win32[slot],
                 sizeof(default_apps_win32[slot]),
                 "Let Windows Decide");
        return default_apps_win32[slot];
    }

    char console_guid[256]  = {0};
    char terminal_guid[256] = {0};
    DWORD size = 0;
    DWORD type = 0;

    size = (DWORD)sizeof(console_guid);
    RegQueryValueExA(hKey, "DelegationConsole", NULL, &type,
                     (BYTE*)console_guid, &size);

    size = (DWORD)sizeof(terminal_guid);
    RegQueryValueExA(hKey, "DelegationTerminal", NULL, &type,
                     (BYTE*)terminal_guid, &size);

    RegCloseKey(hKey);

    const char* name = win32_terminal_name_from_guid(console_guid, terminal_guid);
    if (!name) {
        name = get_default_terminal();
    }
    snprintf(default_apps_win32[slot],
             sizeof(default_apps_win32[slot]),
             "%s", name);
    return default_apps_win32[slot];
}

const char* Platform_GetDefaultApp(DefaultAppKind kind) {
    if (kind < 0 || kind >= DEFAULT_APP_COUNT) return "";

    const char* progid = NULL;
    const char* fallback_name = NULL;

    switch (kind) {
        case DEFAULT_BROWSER:
            progid = win32_default_apps_progid(
                "Software\\Microsoft\\Windows\\Shell\\Associations"
                "\\UrlAssociations\\http\\UserChoice",
                DEFAULT_BROWSER);
            fallback_name = get_default_browser();
            break;

        case DEFAULT_IMAGE_VIEWER:
            progid = win32_default_apps_progid(
                "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"
                "\\FileExts\\.png\\UserChoice",
                DEFAULT_IMAGE_VIEWER);
            fallback_name = get_default_image_viewer();
            break;

        case DEFAULT_VIDEO_PLAYER:
            progid = win32_default_apps_progid(
                "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"
                "\\FileExts\\.mp4\\UserChoice",
                DEFAULT_VIDEO_PLAYER);
            fallback_name = get_default_video_player();
            break;

        case DEFAULT_TERMINAL:
            return Platform_GetDefaultTerminal();

        default:
            default_apps_win32[kind][0] = '\0';
            return default_apps_win32[kind];
    }

    if (progid && progid[0] != '\0') {
        snprintf(default_apps_win32[kind], sizeof(default_apps_win32[kind]), "%s", progid);
    } else {
        snprintf(default_apps_win32[kind], sizeof(default_apps_win32[kind]), "%s", fallback_name);
    }

    return default_apps_win32[kind];
}

bool Platform_ResolveDesktopExec(const char* desktop_id, char* out, size_t out_size) {
    (void)desktop_id;
    (void)out;
    (void)out_size;
    return false;
}
