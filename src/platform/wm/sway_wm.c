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

static bool sway_json_string(const char *at, char *out, size_t cap, const char *limit) {
    if (!at || !out || cap == 0) return false;
    const char *s = strchr(at, '"');
    if (!s || (limit && s >= limit)) return false;
    s++;
    const char *e = s;
    while (*e && *e != '"' && (!limit || e < limit)) {
        if (*e == '\\' && *(e+1)) e += 2;
        else e++;
    }
    if (!*e || (limit && e >= limit)) return false;
    size_t len = e - s;
    if (len >= cap) len = cap - 1;
    size_t o = 0;
    for (size_t i = 0; i < len && o + 1 < cap; i++) {
        if (s[i] == '\\' && i + 1 < len) {
            if (s[i+1] == '/' || s[i+1] == '"') { out[o++] = s[i+1]; i++; }
            else out[o++] = s[i];
        } else out[o++] = s[i];
    }
    out[o] = '\0';
    return o > 0;
}

static bool sway_name_for_pid(const char *json, const char *pid_pos, const char *next_pid, char *out, size_t cap) {
    const char *search = pid_pos - 3000;
    if (search < json) search = json;
    const char *last = NULL;
    const char *cur = search;
    while ((cur = strstr(cur, "\"name\":")) && cur < pid_pos) {
        last = cur;
        cur += 7;
    }
    if (!last) return false;
    const char *colon = strchr(last, ':');
    if (!colon || (next_pid && colon > next_pid)) return false;
    return sway_json_string(colon, out, cap, next_pid);
}

static bool sway_app_for_pid(const char *pid_pos, const char *next_pid, char *out, size_t cap) {
    const char *app = strstr(pid_pos, "\"app_id\":");
    if (app && (!next_pid || app < next_pid)) {
        if (strncmp(app, "\"app_id\": \"", 11) == 0) {
            const char *colon = strchr(app, ':');
            return colon ? sway_json_string(colon, out, cap, next_pid) : false;
        }
        if (strncmp(app, "\"app_id\": null", 14) == 0) {
            const char *cls = strstr(pid_pos, "\"class\":");
            if (cls && (!next_pid || cls < next_pid)) {
                const char *c2 = strchr(cls, ':');
                return c2 ? sway_json_string(c2, out, cap, next_pid) : false;
            }
            return false;
        }
    }
    const char *cls = strstr(pid_pos, "\"class\":");
    if (cls && (!next_pid || cls < next_pid)) {
        const char *c2 = strchr(cls, ':');
        return c2 ? sway_json_string(c2, out, cap, next_pid) : false;
    }
    return false;
}

WindowList get_window_names_sway() {
    char *json = Platform_RunTerminalCommand("swaymsg -t get_tree", false);
    WindowList list = {0};
    if (!json || !*json) { log_error("output is NULL or empty"); free(json); return list; }
    for (char *p = json; (p = strstr(p, "\"pid\":")); p += 6) {
        char *next = strstr(p + 6, "\"pid\":");
        char buf[256];
        if (!sway_app_for_pid(p, next, buf, sizeof(buf))) continue;
        if (!buf[0] || !strcmp(buf, "null")) continue;
        buf[0] = toupper((unsigned char)buf[0]);
        char **tmp = realloc(list.windows, (list.count + 1) * sizeof(char*));
        if (!tmp) break;
        list.windows = tmp;
        list.windows[list.count++] = strdup(buf);
    }
    free(json);
    return list;
}

WindowList get_window_titles_sway() {
    char *json = Platform_RunTerminalCommand("swaymsg -t get_tree", false);
    WindowList list = {0};
    if (!json || !*json) { log_error("output is NULL or empty"); free(json); return list; }
    for (char *p = json; (p = strstr(p, "\"pid\":")); p += 6) {
        char *next = strstr(p + 6, "\"pid\":");
        char buf[512];
        if (!sway_name_for_pid(json, p, next, buf, sizeof(buf))) continue;
        if (!buf[0] || !strcmp(buf, "null")) continue;
        char **tmp = realloc(list.windows, (list.count + 1) * sizeof(char*));
        if (!tmp) break;
        list.windows = tmp;
        list.windows[list.count++] = strdup(buf);
    }
    free(json);
    return list;
}
