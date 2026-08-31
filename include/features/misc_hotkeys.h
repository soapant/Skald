#ifndef MISC_HOTKEYS_H
#define MISC_HOTKEYS_H

#include "core/flapi.h"
#include "core/platform.h"
#include "features/window_switcher.h"
#include "features/process_killer.h"
#include "features/app_launcher.h"
#if defined(_WIN32)
#include <shellapi.h>
#include <shlwapi.h>
#endif

static const MiscHotkey G_MISC_HOTKEYS[] = {
    // {'G', HOTKEY_MOD_ALT_CTRL, "*", "{DEV}/retard games/Grand Theft Auto V Legacy/PlayGTAV.bat" },
    // {'O', HOTKEY_MOD_ALT_CTRL, "*", "{APPDATA}/Microsoft/Windows/Start Menu/Programs/OnTopReplica/OnTopReplica.lnk"}
};
static const unsigned int MISC_HOTKEY_COUNT = sizeof(G_MISC_HOTKEYS) / sizeof(G_MISC_HOTKEYS[0]);

static void HandleKillMyProgram(void) {
    terminate_process_by_name("flint.exe");
    terminate_process_by_name("main.exe");
}

static void HandleSettingsTrigger(void) {
    Platform_SettingsPopUp();
}

static inline void RunMiscApplication(const char* path) {
    char expanded_path[MAX_PATH];
    if (path != NULL && !Platform_ExpandPath(path, expanded_path, sizeof(expanded_path))) return;
    if (path != NULL) path = expanded_path;
#if defined(_WIN32)
    if (path == NULL || strlen(path) == 0) return;
    const char* ext = PathFindExtensionA(path);
    SHELLEXECUTEINFOA sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFOA);
    sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_ASYNCOK;
    sei.lpVerb = "open";
    sei.lpFile = path;
    sei.nShow = SW_SHOWNORMAL;
    if (StrCmpIA(ext, ".bat") == 0) {
        char dir_path[MAX_PATH];
        strncpy(dir_path, path, sizeof(dir_path) - 1);
        PathRemoveFileSpecA(dir_path);
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "cmd /c \"cd /d \"%s\" && \"%s\"\"", dir_path, path);
        system(cmd);
        printf("[MISC] Running .bat in: %s\n", dir_path);
        return;
    }
    if (StrCmpIA(ext, ".png") == 0 || StrCmpIA(ext, ".jpg") == 0 || StrCmpIA(ext, ".jpeg") == 0 || StrCmpIA(ext, ".bmp") == 0) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "mspaint \"%s\"", path);
        system(cmd);
        printf("[MISC] Opening image with mspaint: %s\n", path);
        return;
    }
    if (StrCmpIA(ext, ".exe") == 0) {
        sei.lpFile = path;
        if (ShellExecuteExA(&sei)) printf("[MISC] Launched: %s\n", path);
        else printf("[MISC] Failed to launch: %s (Error: %lu)\n", path, GetLastError());
        return;
    }
    if (StrCmpIA(ext, ".url") == 0) {
        char url[2048] = {0};
        FILE* f = fopen(path, "r");
        if (f) {
            char line[1024];
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "URL=", 4) == 0) { strncpy(url, line + 4, sizeof(url) - 1); url[strcspn(url, "\r\n")] = '\0'; break; }
            }
            fclose(f);
        }
        if (strlen(url) > 0) { ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL); printf("[MISC] Opening URL: %s\n", url); }
        else printf("[MISC] Failed to read URL from: %s\n", path);
        return;
    }
    if (StrCmpIA(ext, ".txt") == 0) { char cmd[2048]; snprintf(cmd, sizeof(cmd), "notepad \"%s\"", path); system(cmd); printf("[MISC] Opening text file: %s\n", path); return; }
    if (StrCmpIA(ext, ".lnk") == 0) { sei.lpFile = path; if (ShellExecuteExA(&sei)) printf("[MISC] Launched shortcut: %s\n", path); else printf("[MISC] Failed to launch shortcut: %s (Error: %lu)\n", path, GetLastError()); return; }
    if (ShellExecuteExA(&sei)) printf("[MISC] Opened: %s\n", path); else printf("[MISC] Failed to open: %s (Error: %lu)\n", path, GetLastError());
#else
    (void)path;
#endif
}

static inline bool CheckMiscHotkeyBindings(int pressed_vk, int modifiers) {
#if defined(_WIN32)
    WindowHandle hActiveWnd = WH_FROM_HWND(GetForegroundWindow());
#else
    WindowHandle hActiveWnd = WH_NULL;
#endif
    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    int idx = fl_find_misc_hotkey_idx(pressed_vk, modifiers, activeExe, G_MISC_HOTKEYS, MISC_HOTKEY_COUNT);
    if (idx < 0) return false;
    RunMiscApplication(G_MISC_HOTKEYS[idx].misc_path);
    return true;
}

static inline void misc_hotkeys_init(void) {
    fl_register_hotkey("Alt+Shift+K", HandleKillMyProgram, NULL, NULL);
    fl_register_hotkey("Alt+Shift+T", Platform_HandleShowCurrentTime, NULL, NULL);
    fl_register_hotkey("Alt+D", HandleSettingsTrigger, NULL, NULL);
}

#endif
