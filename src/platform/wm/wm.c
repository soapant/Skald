#include "wm/wm.h"
#include "wm/kwin_wm.h"
#include "wm/niri_wm.h"
#include "wm/sway_wm.h"
#include "wm/hyprland_wm.h"
#include "wm/x11_wm.h"
#include "deps/log.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>

char* Wm_GetActiveWindowTitle(void) {
    static char buffer[1024];
    
    buffer[0] = '\0'; 
    
    if (getenv("WAYLAND_DISPLAY")) {
        if (get_title_kwin(buffer, sizeof(buffer))) {
        if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0) return buffer;
        }
        if (get_title_niri(buffer, sizeof(buffer))) {
        if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0) return buffer;
        }
        if (get_title_hyprland(buffer, sizeof(buffer))) {
        if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0) return buffer;
        }
        if (get_title_sway(buffer, sizeof(buffer))) {
        if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0) return buffer;
        }
    }
    if (get_title_x11(buffer, sizeof(buffer))) {
    if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0) return buffer;
    }
    log_error("Coulnt Get Active Window Title");
    return NULL;
}

char* Wm_GetActiveWindowAppId(void) {
    static char buffer[1024];

    buffer[0] = '\0';

    if (getenv("WAYLAND_DISPLAY")) {
        if (get_app_id_kwin(buffer, sizeof(buffer))) {
        if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0) return buffer;
        }
        if (get_app_id_niri(buffer, sizeof(buffer))) {
        if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0) return buffer;
        }
        if (get_app_id_hyprland(buffer, sizeof(buffer))) {
        if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0) return buffer;
        }
        if (get_app_id_sway(buffer, sizeof(buffer))) {
        if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0) return buffer;
        }
    }
    if (get_app_id_x11(buffer, sizeof(buffer))) {
    if (buffer[0] != '\0' && strcasecmp(buffer, "colored") != 0) return buffer;
    }
    return NULL;
}

bool Wm_FocusByTitle(const char *title) {
    if (getenv("WAYLAND_DISPLAY")) {
        if (focus_window_title_kwin((char*)title)) {
            return true;
        }
        if (focus_window_title_niri((char*)title)) {
            return true;
        }
        if (focus_window_title_hyprland(title)) {
            return true;
        }
        if (focus_window_title_sway(title)) {
            return true;
        }
    }
    if (focus_window_title_x11(title)) {
        return true;
    }
    log_error("Coulnt Focus By TItle %s",title);
    
    return NULL;
}

char* Wm_Paste(char* content) {
    char* result;
    if (getenv("WAYLAND_DISPLAY")) {
        if ((result = paste_kwin(content))) return result;
        if ((result = paste_niri(content))) return result;
        if ((result = paste_hyprland(content))) return result;
        if ((result = paste_sway(content))) return result;
    }
    if ((result = paste_x11(content))) return result;
    log_error("content='%s'", content ? content : "null");
    return NULL;
}

PlWindowList Wm_GetWindowTitles() {
    PlWindowList result;
    PlWindowList empty = { .windows = NULL, .count = 0 };
    if (getenv("WAYLAND_DISPLAY")) {
        result = get_window_titles_kwin();
        if (result.windows != NULL) return result;
        result = get_window_titles_niri();
        if (result.windows != NULL) return result;
        result = get_window_titles_hyprland();
        if (result.windows != NULL) return result;
        result = get_window_titles_sway();
        if (result.windows != NULL) return result;
    }
    result = get_window_titles_x11();
    if (result.windows != NULL) return result;
    log_error("GetWindowTitles Didnt Work");
    return empty;
}
PlWindowList Wm_GetWindowNames() {
    PlWindowList result;
    PlWindowList empty = { .windows = NULL, .count = 0 };
    if (getenv("WAYLAND_DISPLAY")) {
        result = get_window_names_kwin();
        if (result.windows != NULL) return result;
        result = get_window_names_niri();
        if (result.windows != NULL) return result;
        result = get_window_names_hyprland();
        if (result.windows != NULL) return result;
        result = get_window_names_sway();
        if (result.windows != NULL) return result;
    }
    result = get_window_names_x11();
    if (result.windows != NULL) return result;
    log_error("GetWindowNames Didnt Work");
    return empty;
}

PlPos Wm_GetCursorPosition() {
    PlPos result;
    PlPos empty = { .x = 0, .y = 0 };
    if (getenv("WAYLAND_DISPLAY")) {
        result = get_cursor_position_kwin();
        if (result.x != 0) return result;
        result = get_cursor_position_niri();
        if (result.x != 0) return result;
        result = get_cursor_position_hyprland();
        if (result.x != 0) return result;
        result = get_cursor_position_sway();
        if (result.x != 0) return result;
    } else {
        result = get_cursor_position_x11();
        if (result.x != 0) return result;
    }
    log_error("GetWindowNames Didnt Work");
    return empty;
}

bool Wm_CloseActiveWindow(void) {
    if (getenv("WAYLAND_DISPLAY")) {
        if (close_active_window_kwin()) return true;
        if (close_active_window_niri()) return true;
        if (close_active_window_hyprland()) return true;
        if (close_active_window_sway()) return true;
    }
    if (close_active_window_x11()) return true;
    log_error("CloseActiveWindow Didnt Work");
    return false;
}

bool Wm_ToggleFullscreenActive(void) {
    if (getenv("WAYLAND_DISPLAY")) {
        if (toggle_fullscreen_active_kwin()) return true;
        if (toggle_fullscreen_active_niri()) return true;
        if (toggle_fullscreen_active_hyprland()) return true;
        if (toggle_fullscreen_active_sway()) return true;
    }
    if (toggle_fullscreen_active_x11()) return true;
    log_error("ToggleFullscreenActive Didnt Work");
    return false;
}

bool Wm_MoveActiveWindowToMonitor(bool right) {
    if (getenv("WAYLAND_DISPLAY")) {
        if (move_active_window_to_monitor_kwin(right)) return true;
        if (move_active_window_to_monitor_niri(right)) return true;
        if (move_active_window_to_monitor_hyprland(right)) return true;
        if (move_active_window_to_monitor_sway(right)) return true;
    }
    if (move_active_window_to_monitor_x11(right)) return true;
    log_error("MoveActiveWindowToMonitor Didnt Work");
    return false;
}

bool Wm_MoveActiveWindowLeft(void) {
    if (getenv("WAYLAND_DISPLAY")) {
        if (move_active_window_left_kwin()) return true;
        if (move_active_window_left_niri()) return true;
        if (move_active_window_left_hyprland()) return true;
        if (move_active_window_left_sway()) return true;
    }
    if (move_active_window_left_x11()) return true;
    log_error("MoveActiveWindowLeft Didnt Work");
    return false;
}

bool Wm_MoveActiveWindowRight(void) {
    if (getenv("WAYLAND_DISPLAY")) {
        if (move_active_window_right_kwin()) return true;
        if (move_active_window_right_niri()) return true;
        if (move_active_window_right_hyprland()) return true;
        if (move_active_window_right_sway()) return true;
    }
    if (move_active_window_right_x11()) return true;
    log_error("MoveActiveWindowRight Didnt Work");
    return false;
}

bool Wm_FocusWindowLeft(void) {
    if (getenv("WAYLAND_DISPLAY")) {
        if (focus_window_left_kwin()) return true;
        if (focus_window_left_niri()) return true;
        if (focus_window_left_hyprland()) return true;
        if (focus_window_left_sway()) return true;
    }
    if (focus_window_left_x11()) return true;
    log_error("FocusWindowLeft Didnt Work");
    return false;
}

bool Wm_FocusWindowRight(void) {
    if (getenv("WAYLAND_DISPLAY")) {
        if (focus_window_right_kwin()) return true;
        if (focus_window_right_niri()) return true;
        if (focus_window_right_hyprland()) return true;
        if (focus_window_right_sway()) return true;
    }
    if (focus_window_right_x11()) return true;
    log_error("FocusWindowRight Didnt Work");
    return false;
}

bool Wm_MinimizeActiveWindow(void) {
    if (getenv("WAYLAND_DISPLAY")) {
        if (minimize_active_window_kwin()) return true;
        if (minimize_active_window_niri()) return true;
        if (minimize_active_window_hyprland()) return true;
        if (minimize_active_window_sway()) return true;
    }
    if (minimize_active_window_x11()) return true;
    log_error("MinimizeActiveWindow Didnt Work");
    return false;
}

bool Wm_ScreenshotCopyArea(void) {
    if (getenv("WAYLAND_DISPLAY")) {
        if (screenshot_copy_area_kwin()) return true;
    }
    return false;
}

bool Wm_ScreenshotCopy(void) {
    if (getenv("WAYLAND_DISPLAY")) {
        if (screenshot_copy_kwin()) return true;
    }
    return false;
}

bool Wm_ScreenshotSaveArea(char* save_path) {
    if (getenv("WAYLAND_DISPLAY")) {
        if (screenshot_save_area_kwin(save_path)) return true;
    }
    return false;
}

bool Wm_ScreenshotSave(char* save_path) {
    if (getenv("WAYLAND_DISPLAY")) {
        if (screenshot_save_kwin(save_path)) return true;
    }
    return false;
}

char* Wm_OcrArea(void) {
    if (getenv("WAYLAND_DISPLAY")) {
        char* out = ocr_area_kwin();
        if (out) return out;
    }
    return NULL;
}
