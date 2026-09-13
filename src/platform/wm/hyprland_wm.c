#include "wm/hyprland_wm.h"
#include "deps/log.h"
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
  snprintf(cmd, sizeof(cmd), "hyprctl dispatch focuswindow 'title:%s'", title);


    if (!succeded_hypr(Platform_RunTerminalCommand(cmd, false))) {
        if (title[0] != '\0') {
            snprintf(cmd, sizeof(cmd), "hyprctl dispatch focuswindow 'title:%c%s'",
                     toupper((unsigned char)title[0]), 
                     &title[1]);
            if (succeded_hypr(Platform_RunTerminalCommand(cmd, false))) {
                return true;
            }
        }
        return false;
    }
}

WindowList get_window_names_hyprland() {
   char* output = Platform_RunTerminalCommand("hyprctl clients -j", false);
   WindowList list = { .windows = NULL, .count = 0 };
   char buffer[512];
   if (output == NULL || strlen(output) == 0) return list;
   StringList lines = String_Split(output, "\n");
   for (size_t i = 0; i < lines.count; i++) {
       char *match = strstr(lines.items[i], "\"class\"");
       if (match) {
           if (sscanf(match, "\"class\": \"%[^\"]\"", buffer) == 1) {
               if (strlen(buffer) == 0) continue;
               buffer[0] = toupper((unsigned char)buffer[0]);
               char** temp = realloc(list.windows, (list.count + 1) * sizeof(char*));
               if (temp == NULL) break;
               list.windows = temp;
               list.windows[list.count] = strdup(buffer);
               list.count++;
           }
       }
   }
   StringList_Free(&lines);
   free(output);
   return list;
}
WindowList get_window_titles_hyprland() {
   char* output = Platform_RunTerminalCommand("hyprctl clients -j", false);
   WindowList list = { .windows = NULL, .count = 0 };
   char buffer[512];
   if (output == NULL || strlen(output) == 0) return list;
   StringList lines = String_Split(output, "\n");
   for (size_t i = 0; i < lines.count; i++) {
       char *match = strstr(lines.items[i], "\"title\"");
       if (match) {
           if (sscanf(match, "\"title\": \"%[^\"]\"", buffer) == 1) {
               if (strlen(buffer) == 0) continue;
               char** temp = realloc(list.windows, (list.count + 1) * sizeof(char*));
               if (temp == NULL) break;
               list.windows = temp;
               list.windows[list.count] = strdup(buffer);
               list.count++;
           }
       }
   }
   StringList_Free(&lines);
   free(output);
   return list;
}
WindowList get_list_of_windows_hyprland() { return get_window_names_hyprland(); }
