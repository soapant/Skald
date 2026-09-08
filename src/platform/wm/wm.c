#include "wm/wm.h"
#include "wm/sway_wm.h"
#include "wm/hyprland_wm.h"
#include "wm/x11_wm.h"
#include <string.h>
#include <stdlib.h>

bool Wm_GetActiveWindowTitle(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    if (get_title_hyprland(buffer, buffer_size)) return true;
    if (get_title_sway(buffer, buffer_size)) return true;
    return get_title_x11(buffer, buffer_size);
}

bool Wm_FocusByTitle(const char *title) {
    (void)title;
    return false;
}

char* Wm_GetClipboardLastEntry(void) {
    char* result;
    
    if ((result = get_clipboard_entry_hyprland())) return result;
    if ((result = get_clipboard_entry_sway())) return result;
    if ((result = get_clipboard_entry_x11())) return result;
    
    return NULL;
}
