#include "wm/wm.h"
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
        if ((result = paste_hyprland(content))) return result;
        if ((result = paste_sway(content))) return result;
    }
    if ((result = paste_x11(content))) return result;
    log_error("content='%s'", content ? content : "null");
    return NULL;
}

Fl_WindowList Wm_GetWindowTitles() {
    Fl_WindowList result;
    Fl_WindowList empty = { .windows = NULL, .count = 0 };
    if (getenv("WAYLAND_DISPLAY")) {
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
Fl_WindowList Wm_GetWindowNames() {
    Fl_WindowList result;
    Fl_WindowList empty = { .windows = NULL, .count = 0 };
    if (getenv("WAYLAND_DISPLAY")) {
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
Fl_Pos Wm_GetCursorPosition() {
    Fl_Pos result;
    Fl_Pos empty = { .x = 0, .y = 0 };
    if (getenv("WAYLAND_DISPLAY")) {
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
