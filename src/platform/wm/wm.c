#include "wm/wm.h"
#include "wm/sway_wm.h"
#include "wm/hyprland_wm.h"
#include "wm/x11_wm.h"
#include <string.h>
#include <stdlib.h>

char* Wm_GetActiveWindowTitle(void) {
    static char buffer[1024];
    
    buffer[0] = '\0'; 
    
    if (get_title_hyprland(buffer, sizeof(buffer))) {
        return buffer;
    }
    if (get_title_sway(buffer, sizeof(buffer))) {
        return buffer;
    }
    if (get_title_x11(buffer, sizeof(buffer))) {
        return buffer;
    }
    
    return NULL;
}

bool Wm_FocusByTitle(const char *title) {
    
    if (focus_window_title_hyprland(title)) {
        return true;
    }
    if (focus_window_title_sway(title)) {
        return true;
    }
    if (focus_window_title_x11(title)) {
        return true;
    }
    
    return NULL;
}

char* Wm_GetClipboardLastEntry(void) {
    char* result;
    
    if ((result = get_clipboard_entry_hyprland())) return result;
    if ((result = get_clipboard_entry_sway())) return result;
    if ((result = get_clipboard_entry_x11())) return result;
    
    return NULL;
}
char* Wm_Paste(char* content) {
    char* result;
    
    if ((result = paste_hyprland(content))) return result;
    if ((result = paste_sway(content))) return result;
    if ((result = paste_x11(content))) return result;
    
    return NULL;
}
