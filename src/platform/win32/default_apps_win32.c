#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "platform/platform.h"
#include "config.h"

#include "core/timer.h"
char default_apps_win32[DEFAULT_APP_COUNT][256];

const char* win32_default_apps_read_sz(const char* subkey,
                                             const char* value_name,
                                             int slot) {
        PROFILE_START(_pt);
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
    { PROFILE_END_VAR(_pt, "win32_default_apps_read_sz"); return default_apps_win32[slot]; }
}

const char* win32_terminal_name_from_guid(const char* console_guid, const char* terminal_guid) {
        PROFILE_START(_pt);
if (strstr(console_guid,  "2EACA947-7F5F-4CFA-BA87-8F7FBEEFBE69") ||
        strstr(terminal_guid, "E12CFF52-A866-4C77-9A90-F570A7AA2C6B")) {
        return "Windows Terminal";
    }
    if (strstr(console_guid,  "B23D10C0-E52E-411E-9D5B-C09FDF709C7D")) {
        return "Legacy Console Host";
    }
    { PROFILE_END_VAR(_pt, "win32_terminal_name_from_guid"); return NULL; }
}

const char* win32_default_apps_progid(const char* subkey, int slot) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "win32_default_apps_progid"); return win32_default_apps_read_sz(subkey, "ProgId", slot); }
}

static ListOfFiles g_sm_files;
static bool g_sm_scanned = false;
#define SM_SLOTS 8
static char g_sm_match[SM_SLOTS][128];
static bool g_sm_slot_done[SM_SLOTS];

static const char* sm_lookup(int slot, const char* const possibilities[], size_t count) {
        PROFILE_START(_pt);
if (slot < 0 || slot >= SM_SLOTS) { PROFILE_END_VAR(_pt, "sm_lookup"); return NULL; }
    if (g_sm_slot_done[slot]) { PROFILE_END_VAR(_pt, "sm_lookup"); return g_sm_match[slot][0] ? g_sm_match[slot] : NULL; }
    g_sm_slot_done[slot] = true;

    if (!g_sm_scanned) {
        g_sm_scanned = true;
        Platform_GetAllFilesFromDir("C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs",
                                    NULL, &g_sm_files, NULL, true);
    }
    for (size_t j = 0; j < count; j++) {
        for (int i = 0; i < g_sm_files.count; i++) {
            if (Platform_StringContainsCaseInsensitive(g_sm_files.files[i].name, possibilities[j])) {
                char* name = g_sm_files.files[i].name;
                name[0] = (char)tolower((unsigned char)name[0]);
                snprintf(g_sm_match[slot], sizeof(g_sm_match[slot]), "%s", name);
                return g_sm_match[slot];
            }
        }
    }
    { PROFILE_END_VAR(_pt, "sm_lookup"); return NULL; }
}

void win32_default_apps_free_cache(void) {
        PROFILE_START(_pt);
if (g_sm_files.files) {
        free(g_sm_files.files);
        g_sm_files.files = NULL;
    }
    g_sm_files.count = 0;
    g_sm_files.capacity = 0;
    g_sm_scanned = false;
    for (int i = 0; i < SM_SLOTS; i++) {
        g_sm_match[i][0] = '\0';
        g_sm_slot_done[i] = false;
    }
}

const char* get_default_terminal(void) {
        PROFILE_START(_pt);
const char* possible_terminals[] = {
        "Alacritty", "WezTerm", "Tabby", "Warp", "Ghostty", "Hyper", "Yaw",
        "Wave Terminal", "Fluent Terminal", "Rio",
        "Cmder", "ConEmu", "PuTTY",
        "MobaXterm", "Git Bash Terminal (mintty)", "SecureCRT", "Xshell",
        "ZOC Terminal", "KittyY", "PowerShell (Modern pwsh)",
        "Windows PowerShell "
    };
    
    const char* found = sm_lookup(0, possible_terminals, ARRAY_LEN(possible_terminals));
    { PROFILE_END_VAR(_pt, "get_default_terminal"); return found ? found : "pwsh"; }
}

const char* get_default_browser(void) {
        PROFILE_START(_pt);
const char* possible_browsers[] = {
        "Google Chrome", "Mozilla Firefox", "Microsoft Edge", "Brave", "Opera",
        "Vivaldi", "Tor Browser", "Waterfox", "LibreWolf", "Arc", "Sidekick",
        "Pale Moon", "Internet Explorer","Zen"
    };

    const char* found = sm_lookup(1, possible_browsers, ARRAY_LEN(possible_browsers));
    { PROFILE_END_VAR(_pt, "get_default_browser"); return found ? found : "chrome"; }
}

const char* Platform_GetDefaultTerminal(void) {
        PROFILE_START(_pt);
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
    { PROFILE_END_VAR(_pt, "Platform_GetDefaultTerminal"); return default_apps_win32[slot]; }
}

const char* Platform_GetDefaultApp(DefaultAppKind kind) {
        PROFILE_START(_pt);
if (kind < 0 || kind >= DEFAULT_APP_COUNT) { PROFILE_END_VAR(_pt, "Platform_GetDefaultApp"); return ""; }

    const char* progid = NULL;
    const char* fallback_name = NULL;

    switch (kind) {
        case DEFAULT_BROWSER:
            fallback_name = get_default_browser();
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

    { PROFILE_END_VAR(_pt, "Platform_GetDefaultApp"); return default_apps_win32[kind]; }
}

bool Platform_ResolveDesktopExec(const char* desktop_id, char* out, size_t out_size) {
        PROFILE_START(_pt);
(void)desktop_id;
    (void)out;
    (void)out_size;
    { PROFILE_END_VAR(_pt, "Platform_ResolveDesktopExec"); return false; }
}
