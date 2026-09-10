#include "wm/hyprland_wm.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
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


bool succeded_hypr(char* output) {
    StringList lines = String_Split(output, "\n");
    for (size_t i = 0; i < lines.count; i++) {
      if (Platform_StringContainsCaseInsensitive(lines.items[i],"\"error\"")) {
        return false;
      }
    }
    return true;
}

bool focus_window_title_hyprland(char* title) {
  if (!title) return false;
  
  char cmd[512];
  snprintf(cmd, sizeof(cmd), "hyprctl dispatch focuswindow title:%s",title);

    if (!succeded_hypr(Platform_RunTerminalCommand(cmd, false))) {
        if (title[0] != '\0') {
            snprintf(cmd, sizeof(cmd), "hyprctl dispatch focuswindow title:%c%s", 
                     toupper((unsigned char)title[0]), 
                     &title[1]);
            if (succeded_hypr(Platform_RunTerminalCommand(cmd, false))) {
                return true;
            }
        }
        return false;
    }
}