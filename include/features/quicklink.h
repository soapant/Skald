#ifndef QUICKLINK_H
#define QUICKLINK_H

#include "core/flapi.h"
#include "core/platform.h"
#include "features/window_switcher.h"

static const QuickLinkHotkey G_QUICKLINK_HOTKEYS[] = {
    {'Y', HOTKEY_MOD_ALT_CTRL, "*", "https://youtube.com" },
    // {'M', HOTKEY_MOD_ALT_CTRL, "*", "https://messenger.com" },
};
static const unsigned int QUICKLINK_HOTKEY_COUNT = sizeof(G_QUICKLINK_HOTKEYS) / sizeof(G_QUICKLINK_HOTKEYS[0]);

static inline void open_browser(const char *url) {
    if (!url) return;
    char command[1024];
#if defined(_WIN32)
    snprintf(command, sizeof(command), "start %s", url);
#else
    const char* real_user = Platform_GetRealUser();
    const char* display = getenv("DISPLAY");
    if (!display) display = ":0";
    const char* xauthority = getenv("XAUTHORITY");
    if (!xauthority) xauthority = "";
    char runtime_dir[256];
    const char* env_runtime = getenv("XDG_RUNTIME_DIR");
    if (env_runtime) snprintf(runtime_dir, sizeof(runtime_dir), "%s", env_runtime);
    else snprintf(runtime_dir, sizeof(runtime_dir), "/run/user/1000");
    if (Platform_IsRunningAsRoot()) {
        snprintf(command, sizeof(command), "sudo -u %s DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s xdg-open \"%s\" > /dev/null 2>&1 &", real_user, display, xauthority, runtime_dir, url);
    } else {
        snprintf(command, sizeof(command), "DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s xdg-open \"%s\" > /dev/null 2>&1 &", display, xauthority, runtime_dir, url);
    }
#endif
    int result = system(command);
    if (result == 0) printf("Browser opened successfully.\n");
    else printf("Failed to open the browser.\n");
}

static inline bool CheckQuickLinkHotkeyBindings(int pressed_vk, int modifiers) {
#if defined(_WIN32)
    WindowHandle hActiveWnd = WH_FROM_HWND(GetForegroundWindow());
#else
    WindowHandle hActiveWnd = WH_NULL;
#endif
    char activeExe[256] = {0};
    FetchExeNameOnDemand(hActiveWnd, activeExe, sizeof(activeExe) - 1);
    int idx = fl_find_quicklink_idx(pressed_vk, modifiers, activeExe, G_QUICKLINK_HOTKEYS, QUICKLINK_HOTKEY_COUNT);
    if (idx < 0) return false;
    open_browser(G_QUICKLINK_HOTKEYS[idx].QuickLinkQuery);
    if (MessageAfterEachHotkey) {
        char buffer[64];
        snprintf(buffer,sizeof(buffer),"[QuickLink] Hotkey Name %s Triggered \n",G_QUICKLINK_HOTKEYS[idx].QuickLinkQuery);
        MSG_BOTTOM(buffer,140,TimeOutForMessages,COLOR_TEXT);
    }
    return true;
}

static inline void quicklink_init(void) {
    // G_QUICKLINK_HOTKEYS are handled via CheckQuickLinkHotkeyBindings (polling), not via fl_register_hotkey
}

#endif
