#include "wm/sway_wm.h"
#include "deps/log.h"
#include "core/platform.h"
#include "config.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

bool get_title_sway(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    if (!getenv("SWAYSOCK") && !getenv("I3SOCK")) return false;
    char *command_output = Platform_RunTerminalCommand(
        "sh -c \"swaymsg -t get_tree | jq '.. | objects | select(.focused? == true)'\"",
        false
    );
    if (!command_output) return false;
    if (command_output[0] == '\0') { free(command_output); return false; }
    StringList lines = String_Split(command_output, "\n");
    char *name = NULL;
    char *app_id = NULL;
    for (size_t i = 0; i < lines.count; i++) {
        if (!lines.items[i]) continue;
        if (!name && Platform_StringContainsCaseInsensitive(lines.items[i], "\"name\"")) {
            name = lines.items[i];
        } else if (Platform_StringContainsCaseInsensitive(lines.items[i], "\"app_id\"")) {
            app_id = lines.items[i];
            break;
        }
    }
    if (!name) name = (char*)"";
    if (!app_id) app_id = (char*)"";
    size_t nlen = strlen(name);
    size_t alen = strlen(app_id);
    size_t size = nlen + alen + 2;
    char *output_name = malloc(size);
    if (!output_name) { StringList_Free(&lines); free(command_output); return false; }
    output_name[0] = '\0';
    if (nlen) strncpy(output_name, name, size-1);
    if (alen) strncat(output_name, app_id, size-1 - strlen(output_name));
    strncpy(buffer, output_name, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    free(output_name);
    StringList_Free(&lines);
    free(command_output);
    return buffer[0] != '\0';
}

char* get_clipboard_entry_sway() {
  return Platform_RunTerminalCommand("wl-paste",false);
}

char* paste_sway(char* content) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"wtype %s",content);
  return Platform_RunTerminalCommand(buffer,false);
}
bool succeded_sway(char* output) {
    StringList lines = String_Split(output, "\n");
    for (size_t i = 0; i < lines.count; i++) {
      if (Platform_StringContainsCaseInsensitive(lines.items[i],"\"error\"")) {
        return false;
      }
    }
    return true;
}

bool focus_window_title_sway(char* title) {
    if (!title) return false;

    char escaped_title[512];
    char* src = title;
    char* dst = escaped_title;
    while (*src && (dst - escaped_title) < sizeof(escaped_title) - 1) {
        if (*src == '"') {
            *dst++ = '\\';
            *dst++ = '"';
        } else {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "swaymsg '[title=\"%s\"] focus'", escaped_title);

    if (!succeded_sway(Platform_RunTerminalCommand(cmd, false))) {
        if (escaped_title[0] != '\0') {
            snprintf(cmd, sizeof(cmd), "swaymsg '[title=\"%c%s\"] focus'", 
                     toupper((unsigned char)escaped_title[0]), 
                     &escaped_title[1]);
            if (succeded_sway(Platform_RunTerminalCommand(cmd, false))) {
                return true;
            }
        }
        return false;
    }
}

WindowList get_list_of_windows_sway() {
   char* output = Platform_RunTerminalCommand("swaymsg -t get_tree", false);
   
   WindowList list = { .windows = NULL, .count = 0 };
   char buffer[50];
   
   StringList lines = String_Split(output, "\n");
   for (size_t i = 0; i < lines.count; i++) {
       char *match = strstr(lines.items[i], "\"app_id\"");
       if (match) {
           if (sscanf(match, "\"app_id\": \"%[^\"]\"", buffer) == 1) {
               
               buffer[0] = toupper((unsigned char)buffer[0]);
               char** temp = realloc(list.windows, (list.count + 1) * sizeof(char*));
               if (temp == NULL) {
                   break; 
               }
               list.windows = temp;
               
               list.windows[list.count] = strdup(buffer);
               list.count++;
           }
       }
   }
   
   return list;
}
