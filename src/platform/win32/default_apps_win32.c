#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "core/platform.h"
#include "config.h"

char default_apps_win32[DEFAULT_APP_COUNT][256];

const char* default_apps_win32_read_sz(const char* subkey,
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

const char* terminal_name_from_guid(const char* console_guid,const char* terminal_guid) {

    if (strstr(console_guid,  "2EACA947-7F5F-4CFA-BA87-8F7FBEEFBE69") ||
        strstr(terminal_guid, "E12CFF52-A866-4C77-9A90-F570A7AA2C6B")) {
        return "Windows Terminal";
    }
    if (strstr(console_guid,  "B23D10C0-E52E-411E-9D5B-C09FDF709C7D")) {
        return "Legacy Console Host";
    }
    return NULL;
}

const char* default_apps_win32_progid(const char* subkey, int slot) {
    return default_apps_win32_read_sz(subkey, "ProgId", slot);
}
const char* get_default_terminal() {
    ListOfFiles files = {0};
    Platform_GetAllFilesFromDir("C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs", NULL, &files, NULL);
    const char* possible_terminals[] = {
        "Alacritty",
        "WezTerm",
        "Tabby",
        "Warp",
        "Ghostty",
        "Hyper",
        "Yaw",
        "Wave Terminal",
        "Fluent Terminal",
        "Rio",
        "Tilix (via WSL)",

        "Windows Console Host (conhost.exe)",
        "Cmder",
        "ConEmu",
        "PuTTY",
        "MobaXterm",
        "Git Bash Terminal (mintty)",
        "SecureCRT",
        "Xshell",
        "ZOC Terminal",
        "KiTTY",

        "PowerShell (Modern pwsh)",
        "Windows PowerShell (Classic v5.1)",
        "Command Prompt (cmd.exe)",
        "WSL (Windows Subsystem for Linux)",
        "Nushell",
        "Git Bash (bash)",
        "Cygwin",
        "MSYS2"
    };
    for (int j = 0; j < ARRAY_LEN(possible_terminals);j++) {
        for (int i =0;i < files.count;i++) {
          if (Platform_StringContainsCaseInsensitive(files.files[i].name,possible_terminals[j])) {
            char *name = files.files[i].name;
            name[0] = tolower(name[0]);
            return name;
          }
        }
    }
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

    const char* name = terminal_name_from_guid(console_guid, terminal_guid);
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

    switch (kind) {
        case DEFAULT_BROWSER:
            return default_apps_win32_progid(
                "Software\\Microsoft\\Windows\\Shell\\Associations"
                "\\UrlAssociations\\http\\UserChoice",
                DEFAULT_BROWSER);

        case DEFAULT_IMAGE_VIEWER:
            return default_apps_win32_progid(
                "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"
                "\\FileExts\\.png\\UserChoice",
                DEFAULT_IMAGE_VIEWER);

        case DEFAULT_VIDEO_PLAYER:
            return default_apps_win32_progid(
                "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"
                "\\FileExts\\.mp4\\UserChoice",
                DEFAULT_VIDEO_PLAYER);

        case DEFAULT_TERMINAL:
            return Platform_GetDefaultTerminal();

        default:
            default_apps_win32[kind][0] = '\0';
            return default_apps_win32[kind];
    }
}


bool Platform_ResolveDesktopExec(const char* desktop_id, char* out, size_t out_size) {
    (void)desktop_id;
    return false;
}
