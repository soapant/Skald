#include "platform/window.h"
#include "core/platform.h"
#include <string.h>

WM_Type g_current_wm = WM_UNKNOWN;

WM_Type Window_DetectWM(void) {
    if (g_current_wm != WM_UNKNOWN) return g_current_wm;
    const char *desktop = getenv("XDG_CURRENT_DESKTOP");
    const char *wayland = getenv("WAYLAND_DISPLAY");
    const char *sway_sock = getenv("SWAYSOCK");
    if (desktop) {
        if (strstr(desktop, "sway") || sway_sock) { g_current_wm = WM_SWAY; return g_current_wm; }
        if (strstr(desktop, "Hyprland")) { g_current_wm = WM_HYPRLAND; return g_current_wm; }
    }
    if (wayland && strstr(wayland, "hyprland")) { g_current_wm = WM_HYPRLAND; return g_current_wm; }
    g_current_wm = WM_X11;
    return g_current_wm;
}

bool Window_GetActiveWindowTitle(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    WM_Type wm = Window_DetectWM();
    bool ok = false;
    if (wm == WM_SWAY) ok = Window_Sway_GetActiveWindowTitle(buffer, buffer_size);
    else if (wm == WM_HYPRLAND) ok = Window_Hyprland_GetActiveWindowTitle(buffer, buffer_size);
    else ok = Window_X11_GetActiveWindowTitle(buffer, buffer_size);
    if (!ok && wm != WM_X11) ok = Window_X11_GetActiveWindowTitle(buffer, buffer_size);
    return ok;
}

bool Window_FocusByTitle(const char *title) {
    if (!title) return false;
    WM_Type wm = Window_DetectWM();
    // group window ops here - sway/hyprland/x11 each implement own focus logic
    // for now delegate to X11 as placeholder; per-WM files can override via Window_Sway_Focus etc.
    (void)wm;
    return false;
}

bool Window_SetFloating(const char *title, bool floating) {
    (void)title; (void)floating;
    return false;
}

// legacy compat for callers still using old name
bool Linux_GetActiveWindowTitle(char *buffer, int buffer_size) {
    return Window_GetActiveWindowTitle(buffer, buffer_size);
}

bool Platform_FocusWindow(const char* window_name) {
    return Window_FocusByTitle(window_name);
}
