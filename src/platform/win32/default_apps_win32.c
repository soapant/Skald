#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "core/platform.h"

static char g_default_apps_win32[FL_DEFAULT_APP_COUNT][256];

static const char* default_apps_win32_progid(const char* subkey, int slot) {
    g_default_apps_win32[slot][0] = '\0';
    DWORD size = sizeof(g_default_apps_win32[slot]);
    if (RegGetValueA(HKEY_CURRENT_USER, subkey, "ProgId", RRF_RT_REG_SZ, NULL, g_default_apps_win32[slot], &size) != ERROR_SUCCESS) {
        g_default_apps_win32[slot][0] = '\0';
    }
    return g_default_apps_win32[slot];
}

const char* Platform_GetDefaultApp(Fl_DefaultAppKind kind) {
    if (kind < 0 || kind >= FL_DEFAULT_APP_COUNT) return "";
    if (kind == FL_DEFAULT_BROWSER) return default_apps_win32_progid("Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http\\UserChoice", FL_DEFAULT_BROWSER);
    if (kind == FL_DEFAULT_IMAGE_VIEWER) return default_apps_win32_progid("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.png\\UserChoice", FL_DEFAULT_IMAGE_VIEWER);
    if (kind == FL_DEFAULT_VIDEO_PLAYER) return default_apps_win32_progid("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.mp4\\UserChoice", FL_DEFAULT_VIDEO_PLAYER);
    g_default_apps_win32[kind][0] = '\0';
    return g_default_apps_win32[kind];
}

bool Platform_ResolveDesktopExec(const char* desktop_id, char* out, size_t out_size) {
    if (!desktop_id || !out || out_size == 0) return false;
    out[0] = '\0';
    return false;
}
