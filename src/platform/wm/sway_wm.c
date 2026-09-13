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
    if (!buffer || buffer_size <= 0) { log_error("buffer='%s' buffer_size=%d", buffer ? buffer : "null", buffer_size); return false; }
    buffer[0] = '\0';
    if (!getenv("SWAYSOCK") && !getenv("I3SOCK")) { log_error("SWAYSOCK and I3SOCK not set buffer='%s' buffer_size=%d", buffer ? buffer : "null", buffer_size); return false; }
    char *command_output = Platform_RunTerminalCommand(
        "swaymsg -t get_tree --raw | grep -A 40 '\"focused\": true'",
        false
    );
    if (!command_output) { log_error("command_output is NULL buffer='%s' buffer_size=%d", buffer ? buffer : "null", buffer_size); return false; }
    if (command_output[0] == '\0') { log_error("command_output empty buffer='%s' buffer_size=%d", buffer ? buffer : "null", buffer_size); free(command_output); return false; }
    StringList lines = String_Split(command_output, "\n");
    char *name = NULL;
    char *app_id = NULL;
    for (size_t i = 0; i < lines.count; i++) {
        if (!lines.items[i]) continue;
        if (!name && Platform_StringContainsCaseInsensitive(lines.items[i], "\"name\"")){
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
    if (!output_name) { log_error("malloc failed size=%d buffer='%s' buffer_size=%d", (int)size, buffer ? buffer : "null", buffer_size); StringList_Free(&lines); free(command_output); return false; }
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
  char* out = Platform_RunTerminalCommand("wl-paste",false);
  if (!out) log_error("wl-paste failed");
  return out;
}

char* paste_sway(char* content) {
  if (!content) { log_error("content is NULL"); return NULL; }
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"wtype %s",content);
  char* out = Platform_RunTerminalCommand(buffer,false);
  if (!out) log_error("content='%s'", content ? content : "null");
  return out;
}
bool succeded_sway(char* output) {
    if (!output) { log_error("output is NULL"); return false; }
    StringList lines = String_Split(output, "\n");
    for (size_t i = 0; i < lines.count; i++) {
      if (Platform_StringContainsCaseInsensitive(lines.items[i],"\"error\"")) {
        log_error("output='%s'", output ? output : "null");
        return false;
      }
    }
    return true;
}

bool focus_window_title_sway(char* title) {
    if (!title) { log_error("title is NULL"); return false; }

    char escaped_title[512];
    title[0] = tolower(title[0]);
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
    if (succeded_sway(Platform_RunTerminalCommand(cmd, false))) return true;

    snprintf(cmd, sizeof(cmd), "swaymsg '[app_id=\"%s\"] focus'", escaped_title);
    if (succeded_sway(Platform_RunTerminalCommand(cmd, false))) return true;

    if (escaped_title[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "swaymsg '[title=\"%c%s\"] focus'", 
                 toupper((unsigned char)escaped_title[0]), 
                 &escaped_title[1]);
        if (succeded_sway(Platform_RunTerminalCommand(cmd, false))) return true;
    }

    if (escaped_title[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "swaymsg '[app_id=\"%c%s\"] focus'", 
                 toupper((unsigned char)escaped_title[0]), 
                 &escaped_title[1]);
        if (succeded_sway(Platform_RunTerminalCommand(cmd, false))) return true;
    }
    log_error("title='%s'", title ? title : "null");
    return false;
}

WindowList get_window_names_sway() {
   char* output = Platform_RunTerminalCommand("swaymsg -t get_tree", false);
   WindowList list = { .windows = NULL, .count = 0 };
   char buffer[256];
   if (!output) { log_error("output is NULL"); return list; }
   StringList lines = String_Split(output, "\n");
   for (size_t i = 0; i < lines.count; i++) {
       char *match = strstr(lines.items[i], "\"app_id\"");
       if (match) {
           if (sscanf(match, "\"app_id\": \"%[^\"]\"", buffer) == 1) {
               if (buffer[0]=='\0') continue;
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
WindowList get_window_titles_sway() {
   char* output = Platform_RunTerminalCommand("swaymsg -t get_tree", false);
   WindowList list = { .windows = NULL, .count = 0 };
   char buffer[512];
   if (!output) { log_error("output is NULL"); return list; }
   //temporary hardcoding of sway things like root i3 and numbers because being fucky wucky
   StringList lines = String_Split(output, "\n");
   for (size_t i = 0; i < lines.count; i++) {
       char *match = strstr(lines.items[i], "\"name\"");
       if (match) {
           if (sscanf(match, "\"name\": \"%511[^\"]\"", buffer) == 1) {
               if (buffer[0] == '\0' || strcmp(buffer, "null") == 0 || strcmp(buffer,"root") == 0 || Platform_StringContainsCaseInsensitive(buffer,"i3")) continue;
               bool has_digit = false;
               for (int j = 0; buffer[j] != '\0'; j++) {
                   if (isdigit((unsigned char)buffer[j])) {
                       has_digit = true;
                       break; 
                   }
               }

               if (has_digit & strlen(buffer) < 10) continue;

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
