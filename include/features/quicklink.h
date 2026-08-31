#ifndef QUICKLINK_H
#define QUICKLINK_H

#include "core/flapi.h"
#include "core/platform.h"
#include "features/window_switcher.h"



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

static void open_youtube(void) {
    open_browser("https://youtube.com");
    if (MessageAfterEachHotkey) {
        char buffer[64];
        snprintf(buffer,sizeof(buffer),"[QuickLink] Hotkey Name %s Triggered \n","https://youtube.com");
        MSG_BOTTOM(buffer,140,TimeOutForMessages,COLOR_TEXT);
    }
}

static inline void quicklink_init(void) {
    fl_register_hotkey("Alt+Ctrl+Y", open_youtube, NULL, NULL);
}

#endif
