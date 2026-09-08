#include "wm/hyprland_wm.h"
#include <stddef.h>

bool get_title_hyprland(char *buffer, int buffer_size) {
    (void)buffer; (void)buffer_size;
    return false;
}

char* get_clipboard_entry_hyprland() {
  return Platform_RunTerminalCommand("wl-paste",false);
}

char* paste_hyprland(char* content) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"wtype %s",content);
  return Platform_RunTerminalCommand(buffer,false);
}
