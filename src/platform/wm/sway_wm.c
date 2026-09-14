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
   if (!output) { log_error("output is NULL"); return list; }
   if (output[0]=='\0') { log_error("output is NULL or empty"); free(output); return list; }
   char *p = output;
   while (1) {
       char *pid_pos = strstr(p, "\"pid\":");
       if (!pid_pos) break;
       char *app_pos = strstr(pid_pos, "\"app_id\":");
       char *next_pid = strstr(pid_pos + 6, "\"pid\":");
       char buf[256] = {0};
       bool found = false;
       if (app_pos && (!next_pid || app_pos < next_pid)) {
           if (strstr(app_pos, "\"app_id\": \"") == app_pos) {
               char *s = app_pos + 11;
               char *e = strchr(s, '"');
               if (e && (size_t)(e - s) < sizeof(buf)-1) {
                   memcpy(buf, s, e - s);
                   buf[e - s] = '\0';
                   found = true;
               }
           } else if (strstr(app_pos, "\"app_id\": null") == app_pos) {
               char *cls = strstr(pid_pos, "\"class\": \"");
               if (cls && (!next_pid || cls < next_pid)) {
                   char *s = cls + 10;
                   char *e = strchr(s, '"');
                   if (e && (size_t)(e - s) < sizeof(buf)-1) {
                       memcpy(buf, s, e - s);
                       buf[e - s] = '\0';
                       found = true;
                   }
               }
           }
       }
       if (!found) {
           char *cls = strstr(pid_pos, "\"class\": \"");
           if (cls && (!next_pid || cls < next_pid)) {
               char *s = cls + 10;
               char *e = strchr(s, '"');
               if (e && (size_t)(e - s) < sizeof(buf)-1) {
                   memcpy(buf, s, e - s);
                   buf[e - s] = '\0';
                   found = true;
               }
           }
       }
       if (found && buf[0]) {
           buf[0] = toupper((unsigned char)buf[0]);
           char **tmp = realloc(list.windows, (list.count + 1) * sizeof(char*));
           if (!tmp) break;
           list.windows = tmp;
           list.windows[list.count++] = strdup(buf);
       }
       p = pid_pos + 6;
   }
   free(output);
   return list;
}
WindowList get_window_titles_sway() {
   char* output = Platform_RunTerminalCommand("swaymsg -t get_tree", false);
   WindowList list = { .windows = NULL, .count = 0 };
   if (!output) { log_error("output is NULL"); return list; }
   if (output[0]=='\0') { log_error("output is NULL or empty"); free(output); return list; }
   char *p = output;
   while (1) {
       char *pid_pos = strstr(p, "\"pid\":");
       if (!pid_pos) break;
       char *next_pid = strstr(pid_pos + 6, "\"pid\":");
       char *name_key = "\"name\": \"";
       char *search_start = pid_pos - 2500;
       if (search_start < output) search_start = output;
       char *last_name = NULL;
       char *tmp = search_start;
       while ((tmp = strstr(tmp, name_key)) && tmp < pid_pos) {
           last_name = tmp;
           tmp += 9;
       }
       if (last_name) {
           char *s = last_name + 9;
           char *e = strchr(s, '"');
           size_t len = 0;
           if (e) len = e - s;
           if (e) {
               char *esc = strstr(s, "\\\"");
               if (esc && esc < e) {
                   char *real = strstr(s, "\",");
                   if (real) e = real;
               }
           }
           if (e && len > 0 && len < 512) {
               char buf[512];
               size_t out = 0;
               for (size_t i = 0; i < len && out < sizeof(buf)-1; i++) {
                   if (s[i]=='\\' && i+1 < len) {
                       if (s[i+1]=='/' || s[i+1]=='"') { buf[out++] = s[i+1]; i++; }
                       else buf[out++] = s[i];
                   } else buf[out++] = s[i];
               }
               buf[out]='\0';
               if (buf[0] && strcmp(buf,"null")!=0) {
                   char **t2 = realloc(list.windows, (list.count + 1) * sizeof(char*));
                   if (!t2) break;
                   list.windows = t2;
                   list.windows[list.count++] = strdup(buf);
               }
           }
       }
       p = pid_pos + 6;
   }
   free(output);
   return list;
}
