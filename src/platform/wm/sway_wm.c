#include "wm/sway_wm.h"
#include "deps/log.h"
#include "platform/platform.h"
#include "helpers/helpers_text.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <glob.h>
#include <strings.h>
#include <sys/stat.h>

#include "core/timer.h"
static bool sway_sock_live(const char* path) {
        PROFILE_START(_pt);
struct stat st;
    { PROFILE_END_VAR(_pt, "sway_sock_live"); return path && path[0] && stat(path, &st) == 0; }
}

static void sway_ensure_sock(void) {
        PROFILE_START(_pt);
const char* cur = getenv("SWAYSOCK");
    if (!cur || !cur[0]) cur = getenv("I3SOCK");
    if (sway_sock_live(cur)) { PROFILE_END_VAR(_pt, "sway_ensure_sock"); return ; }
    if (cur) unsetenv(getenv("SWAYSOCK") && getenv("SWAYSOCK")[0] ? "SWAYSOCK" : "I3SOCK");
    const char* rtd = getenv("XDG_RUNTIME_DIR");
    if (!rtd || !rtd[0]) { PROFILE_END_VAR(_pt, "sway_ensure_sock"); return ; }
    char pat[512];
    snprintf(pat, sizeof(pat), "%s/sway-ipc.*.sock", rtd);
    glob_t g;
    memset(&g, 0, sizeof(g));
    if (glob(pat, 0, NULL, &g) == 0 && g.gl_pathc > 0) {
        setenv("SWAYSOCK", g.gl_pathv[g.gl_pathc - 1], 1);
        log_info("discovered SWAYSOCK='%s'", g.gl_pathv[g.gl_pathc - 1]);
    }
    globfree(&g);
}

bool get_title_sway(char *buffer, int buffer_size) {
        PROFILE_START(_pt);
if (!buffer || buffer_size <= 0) { log_error("buffer='%s' buffer_size=%d", buffer ? buffer : "null", buffer_size); return false; }
    buffer[0] = '\0';
    sway_ensure_sock();
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
    { PROFILE_END_VAR(_pt, "get_title_sway"); return buffer[0] != '\0'; }
}

bool get_app_id_sway(char *buffer, int buffer_size) {
        PROFILE_START(_pt);
if (!buffer || buffer_size <= 0) { PROFILE_END_VAR(_pt, "get_app_id_sway"); return false; }
    buffer[0] = '\0';
    sway_ensure_sock();
    if (!getenv("SWAYSOCK") && !getenv("I3SOCK")) { PROFILE_END_VAR(_pt, "get_app_id_sway"); return false; }
    char *command_output = Platform_RunTerminalCommand(
        "swaymsg -t get_tree --raw | grep -A 40 '\"focused\": true'",
        false
    );
    if (!command_output || command_output[0] == '\0') { free(command_output); return false; }
    bool ok = false;
    StringList lines = String_Split(command_output, "\n");
    for (size_t i = 0; i < lines.count; i++) {
        if (!lines.items[i]) continue;
        const char* p = strstr(lines.items[i], "\"app_id\"");
        if (!p) continue;
        p = strchr(p, ':');
        if (!p) continue;
        p++;
        while (*p && (isspace((unsigned char)*p) || *p == '"')) p++;
        size_t n = 0;
        while (p[n] && p[n] != '"' && p[n] != ',' && n < (size_t)(buffer_size - 1)) n++;
        if (n == 0 || (n == 4 && strncmp(p, "null", 4) == 0)) continue;
        memcpy(buffer, p, n);
        buffer[n] = '\0';
        ok = true;
        break;
    }
    StringList_Free(&lines);
    free(command_output);
    { PROFILE_END_VAR(_pt, "get_app_id_sway"); return ok; }
}

char* paste_sway(char* content) {
    PROFILE_START(_pt);
if (!content) { log_error("content is NULL"); return NULL; }
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"wtype %s",content);
  char* out = Platform_RunTerminalCommand(buffer,false);
  if (!out) log_error("content='%s'", content ? content : "null");
  { PROFILE_END_VAR(_pt, "paste_sway"); return out; }
}
bool succeded_sway(char* output) {
        PROFILE_START(_pt);
if (!output) { log_error("output is NULL"); return false; }
    StringList lines = String_Split(output, "\n");
    for (size_t i = 0; i < lines.count; i++) {
      if (Platform_StringContainsCaseInsensitive(lines.items[i],"\"error\"")) {
        log_error("output='%s'", output ? output : "null");
        return false;
      }
    }
    { PROFILE_END_VAR(_pt, "succeded_sway"); return true; }
}

static bool sway_focus_by_con_id(int con_id) {
        PROFILE_START(_pt);
char cmd[128];
    snprintf(cmd, sizeof(cmd), "swaymsg [con_id=%d] focus", con_id);
    char *out = Platform_RunTerminalCommand(cmd, false);
    bool ok = succeded_sway(out);
    free(out);
    { PROFILE_END_VAR(_pt, "sway_focus_by_con_id"); return ok; }
}
static bool sway_json_string(const char *at, char *out, size_t cap, const char *limit);
static bool sway_name_for_pid(const char *json, const char *pid_pos, const char *next_pid, char *out, size_t cap);

bool focus_window_title_sway(char* title) {
        PROFILE_START(_pt);
if (!title || !*title) { log_error("title is NULL"); return false; }
    sway_ensure_sock();
    while (*title && (iscntrl((unsigned char)*title) || isspace((unsigned char)*title))) title++;
    char *end = title + strlen(title);
    while (end > title && (iscntrl((unsigned char)end[-1]) || isspace((unsigned char)end[-1]))) *--end = '\0';
    if (!*title) { log_error("title empty after trim"); return false; }

    char *json = Platform_RunTerminalCommand("swaymsg -t get_tree", false);
    if (json) {
        for (char *p = json; (p = strstr(p, "\"pid\":")); p += 6) {
            char *next = strstr(p + 6, "\"pid\":");
            char name[512] = {0};
            if (!sway_name_for_pid(json, p, next, name, sizeof(name))) continue;
            char *a = name;
            while (*a && (iscntrl((unsigned char)*a) || isspace((unsigned char)*a))) a++;
            char *b = a + strlen(a);
            while (b > a && (iscntrl((unsigned char)b[-1]) || isspace((unsigned char)b[-1]))) *--b = '\0';
            if (!strcmp(a, title) || !strcasecmp(a, title)) {
                char *id_pos = NULL;
                const char *search = p - 4000;
                if (search < json) search = json;
                const char *last_id = NULL;
                const char *cur = search;
                while ((cur = strstr(cur, "\"id\":")) && cur < p) { last_id = cur; cur += 5; }
                if (last_id) {
                    int con_id = atoi(last_id + 5);
                    if (con_id) {
                        free(json);
                        if (sway_focus_by_con_id(con_id)) return true;
                        return false;
                    }
                }
            }
        }
        free(json);
    }

    char escaped[1024];
    char *s = title, *d = escaped;
    while (*s && d - escaped < (int)sizeof(escaped)-2) {
        if (*s == '"' || *s == '\\') { *d++ = '\\'; *d++ = *s++; }
        else *d++ = *s++;
    }
    *d = '\0';
    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "swaymsg '[title=\"%s\"] focus'", escaped);
    if (succeded_sway(Platform_RunTerminalCommand(cmd, false))) { PROFILE_END_VAR(_pt, "focus_window_title_sway"); return true; }
    char low[1024]; strncpy(low, escaped, sizeof(low)-1); low[sizeof(low)-1]='\0';
    if (low[0]) low[0] = tolower((unsigned char)low[0]);
    snprintf(cmd, sizeof(cmd), "swaymsg '[title=\"%s\"] focus'", low);
    if (succeded_sway(Platform_RunTerminalCommand(cmd, false))) { PROFILE_END_VAR(_pt, "focus_window_title_sway"); return true; }
    if (escaped[0]) {
        char up[1024]; strncpy(up, escaped, sizeof(up)-1); up[sizeof(up)-1]='\0';
        up[0] = toupper((unsigned char)up[0]);
        snprintf(cmd, sizeof(cmd), "swaymsg '[title=\"%s\"] focus'", up);
        if (succeded_sway(Platform_RunTerminalCommand(cmd, false))) return true;
    }
    log_error("title='%s'", title ? title : "null");
    { PROFILE_END_VAR(_pt, "focus_window_title_sway"); return false; }
}

static bool sway_json_string(const char *at, char *out, size_t cap, const char *limit) {
        PROFILE_START(_pt);
if (!at || !out || cap == 0) { PROFILE_END_VAR(_pt, "sway_json_string"); return false; }
    const char *s = strchr(at, '"');
    if (!s || (limit && s >= limit)) { PROFILE_END_VAR(_pt, "sway_json_string"); return false; }
    s++;
    const char *e = s;
    while (*e && *e != '"' && (!limit || e < limit)) {
        if (*e == '\\' && *(e+1)) e += 2;
        else e++;
    }
    if (!*e || (limit && e >= limit)) { PROFILE_END_VAR(_pt, "sway_json_string"); return false; }
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
    { PROFILE_END_VAR(_pt, "sway_json_string"); return o > 0; }
}

static bool sway_name_for_pid(const char *json, const char *pid_pos, const char *next_pid, char *out, size_t cap) {
        PROFILE_START(_pt);
const char *search = pid_pos - 3000;
    if (search < json) search = json;
    const char *last = NULL;
    const char *cur = search;
    while ((cur = strstr(cur, "\"name\":")) && cur < pid_pos) {
        last = cur;
        cur += 7;
    }
    if (!last) { PROFILE_END_VAR(_pt, "sway_name_for_pid"); return false; }
    const char *colon = strchr(last, ':');
    if (!colon || (next_pid && colon > next_pid)) { PROFILE_END_VAR(_pt, "sway_name_for_pid"); return false; }
    { PROFILE_END_VAR(_pt, "sway_name_for_pid"); return sway_json_string(colon, out, cap, next_pid); }
}

static bool sway_app_for_pid(const char *pid_pos, const char *next_pid, char *out, size_t cap) {
        PROFILE_START(_pt);
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
    { PROFILE_END_VAR(_pt, "sway_app_for_pid"); return false; }
}

PlWindowList get_window_names_sway() {
        PROFILE_START(_pt);
sway_ensure_sock();
    char *json = Platform_RunTerminalCommand("swaymsg -t get_tree", false);
    PlWindowList list = {0};
    if (!json || !*json) { log_error("output is NULL or empty"); free(json); return list; }
    for (char *p = json; (p = strstr(p, "\"pid\":")); p += 6) {
        char *next = strstr(p + 6, "\"pid\":");
        char buf[256];
        if (!sway_app_for_pid(p, next, buf, sizeof(buf))) continue;
        if (!buf[0] || !strcmp(buf, "null")) continue;
        if (!strcasecmp(buf, "window") || !strcasecmp(buf, "colored")) continue;
        buf[0] = toupper((unsigned char)buf[0]);
        char **tmp = realloc(list.windows, (list.count + 1) * sizeof(char*));
        if (!tmp) break;
        list.windows = tmp;
        list.windows[list.count++] = strdup(buf);
    }
    free(json);
    { PROFILE_END_VAR(_pt, "get_window_names_sway"); return list; }
}

PlWindowList get_window_titles_sway() {
        PROFILE_START(_pt);
sway_ensure_sock();
    char *json = Platform_RunTerminalCommand("swaymsg -t get_tree", false);
    PlWindowList list = {0};
    if (!json || !*json) { log_error("output is NULL or empty"); free(json); return list; }
    for (char *p = json; (p = strstr(p, "\"pid\":")); p += 6) {
        char *next = strstr(p + 6, "\"pid\":");
        char buf[512];
        if (!sway_name_for_pid(json, p, next, buf, sizeof(buf))) continue;
        if (!buf[0] || !strcmp(buf, "null")) continue;
        if (!strcasecmp(buf, "window") || !strcasecmp(buf, "colored")) continue;
        char **tmp = realloc(list.windows, (list.count + 1) * sizeof(char*));
        if (!tmp) break;
        list.windows = tmp;
        list.windows[list.count++] = strdup(buf);
    }
    free(json);
    { PROFILE_END_VAR(_pt, "get_window_titles_sway"); return list; }
}

static int sway_json_depth(const char* base, const char* pos) {
        PROFILE_START(_pt);
int depth = 0;
    int in_str = 0;
    int esc = 0;
    for (const char* s = base; s < pos; s++) {
        if (esc) { esc = 0; continue; }
        if (*s == '\\' && in_str) { esc = 1; continue; }
        if (*s == '"') { in_str = !in_str; continue; }
        if (in_str) continue;
        if (*s == '{' || *s == '[') depth++;
        else if (*s == '}' || *s == ']') depth--;
    }
    { PROFILE_END_VAR(_pt, "sway_json_depth"); return depth; }
}

static char* sway_find_key(const char* base, const char* from, const char* end, const char* key, int want_depth) {
        PROFILE_START(_pt);
size_t kl = strlen(key);
    char* q = (char*)from;
    while (q && (q = strstr(q, key)) != NULL && q < end) {
        if (sway_json_depth(base, q) == want_depth) return q;
        q += kl;
    }
    { PROFILE_END_VAR(_pt, "sway_find_key"); return NULL; }
}

static int sway_key_bool3(const char* base, const char* from, const char* end, const char* key, int depth) {
        PROFILE_START(_pt);
char* k = sway_find_key(base, from, end, key, depth);
    if (!k) { PROFILE_END_VAR(_pt, "sway_key_bool3"); return -1; }
    char* c = strchr(k + strlen(key), ':');
    if (!c || c >= end) { PROFILE_END_VAR(_pt, "sway_key_bool3"); return -1; }
    char* v = c + 1;
    while (v < end && (*v == ' ' || *v == '\t')) v++;
    if (v + 4 <= end && strncmp(v, "true", 4) == 0) { PROFILE_END_VAR(_pt, "sway_key_bool3"); return 1; }
    if (v + 5 <= end && strncmp(v, "false", 5) == 0) { PROFILE_END_VAR(_pt, "sway_key_bool3"); return 0; }
    { PROFILE_END_VAR(_pt, "sway_key_bool3"); return -1; }
}

static int sway_key_int(const char* base, const char* from, const char* end, const char* key, int depth) {
        PROFILE_START(_pt);
char* k = sway_find_key(base, from, end, key, depth);
    if (!k) { PROFILE_END_VAR(_pt, "sway_key_int"); return 0; }
    char* c = strchr(k + strlen(key), ':');
    if (!c || c >= end) { PROFILE_END_VAR(_pt, "sway_key_int"); return 0; }
    { PROFILE_END_VAR(_pt, "sway_key_int"); return atoi(c + 1); }
}

static int sway_key_str(const char* base, const char* from, const char* end, const char* key, int depth, char* out, size_t cap) {
        PROFILE_START(_pt);
char* k = sway_find_key(base, from, end, key, depth);
    if (!k) { PROFILE_END_VAR(_pt, "sway_key_str"); return 0; }
    char* c = strchr(k + strlen(key), ':');
    if (!c || c >= end) { PROFILE_END_VAR(_pt, "sway_key_str"); return 0; }
    char* q = c + 1;
    while (q < end && (*q == ' ' || *q == '\t')) q++;
    if (q >= end || *q != '"') { PROFILE_END_VAR(_pt, "sway_key_str"); return 0; }
    char* e = strchr(q + 1, '"');
    if (!e || e >= end) { PROFILE_END_VAR(_pt, "sway_key_str"); return 0; }
    size_t len = (size_t)(e - (q + 1));
    if (len == 0 || len >= cap) { PROFILE_END_VAR(_pt, "sway_key_str"); return 0; }
    memcpy(out, q + 1, len);
    out[len] = '\0';
    { PROFILE_END_VAR(_pt, "sway_key_str"); return 1; }
}

bool close_active_window_sway(void) {
        PROFILE_START(_pt);
sway_ensure_sock();
    const char* sock = getenv("SWAYSOCK");
    if (!sock || !sock[0]) sock = getenv("I3SOCK");
    if (!sway_sock_live(sock)) { PROFILE_END_VAR(_pt, "close_active_window_sway"); return false; }
    char* out = Platform_RunTerminalCommand("swaymsg kill", false);
    bool ok = succeded_sway(out);
    free(out);
    { PROFILE_END_VAR(_pt, "close_active_window_sway"); return ok; }
}

bool toggle_fullscreen_active_sway(void) {
        PROFILE_START(_pt);
sway_ensure_sock();
    const char* sock = getenv("SWAYSOCK");
    if (!sock || !sock[0]) sock = getenv("I3SOCK");
    if (!sway_sock_live(sock)) { PROFILE_END_VAR(_pt, "toggle_fullscreen_active_sway"); return false; }
    char* out = Platform_RunTerminalCommand("swaymsg fullscreen toggle", false);
    bool ok = succeded_sway(out);
    free(out);
    { PROFILE_END_VAR(_pt, "toggle_fullscreen_active_sway"); return ok; }
}

bool move_active_window_to_monitor_sway(bool right) {
        PROFILE_START(_pt);
sway_ensure_sock();
    const char* sock = getenv("SWAYSOCK");
    if (!sock || !sock[0]) sock = getenv("I3SOCK");
    if (!sway_sock_live(sock)) { PROFILE_END_VAR(_pt, "move_active_window_to_monitor_sway"); return false; }
    char* outs = Platform_RunTerminalCommand("swaymsg -t get_outputs", false);
    if (outs && outs[0]) {
        char* doc_end = outs + strlen(outs);
        char names[16][64];
        int ox[16];
        int oy[16];
        int ow[16];
        int oh[16];
        int usable[16];
        int focused[16];
        int n = 0;
        char* p = outs;
        while (n < 16) {
            char* np = sway_find_key(outs, p, doc_end, "\"name\"", 2);
            if (!np) break;
            char oname[64];
            if (!sway_key_str(outs, np, doc_end, "\"name\"", 2, oname, sizeof(oname))) {
                p = np + 6;
                continue;
            }
            int rx = 0;
            int ry = 0;
            int rw = 0;
            int rh = 0;
            char* rk = sway_find_key(outs, p, doc_end, "\"rect\"", 2);
            if (rk) {
                rx = sway_key_int(outs, rk, doc_end, "\"x\"", 3);
                ry = sway_key_int(outs, rk, doc_end, "\"y\"", 3);
                rw = sway_key_int(outs, rk, doc_end, "\"width\"", 3);
                rh = sway_key_int(outs, rk, doc_end, "\"height\"", 3);
            }
            int is_usable = 1;
            if (sway_key_bool3(outs, np, doc_end, "\"active\"", 2) == 0) is_usable = 0;
            if (sway_key_bool3(outs, np, doc_end, "\"dpms\"", 2) == 0) is_usable = 0;
            if (rw <= 0 || rh <= 0) is_usable = 0;
            memcpy(names[n], oname, sizeof(names[n]));
            ox[n] = rx;
            oy[n] = ry;
            ow[n] = rw;
            oh[n] = rh;
            usable[n] = is_usable;
            focused[n] = (sway_key_bool3(outs, np, doc_end, "\"focused\"", 2) == 1) ? 1 : 0;
            n++;
            p = np + 6;
        }
        if (n >= 2) {
            int cur = -1;
            for (int i = 0; i < n; i++) {
                if (focused[i] && usable[i]) { cur = i; break; }
            }
            if (cur < 0) {
                for (int i = 0; i < n; i++) {
                    if (usable[i]) { cur = i; break; }
                }
            }
            int dst = -1;
            if (cur >= 0) {
                int best_overlap = -1;
                int best_gap = 1 << 30;
                for (int i = 0; i < n; i++) {
                    if (i == cur || !usable[i]) continue;
                    int overlap = 0;
                    int gap = 0;
                    if (right) {
                        if (ox[i] < ox[cur] + ow[cur]) continue;
                        int top = oy[cur] > oy[i] ? oy[cur] : oy[i];
                        int bottom = oy[cur] + oh[cur] < oy[i] + oh[i] ? oy[cur] + oh[cur] : oy[i] + oh[i];
                        overlap = bottom - top;
                        if (overlap < 0) overlap = 0;
                        gap = ox[i] - (ox[cur] + ow[cur]);
                    } else {
                        if (ox[i] + ow[i] > ox[cur]) continue;
                        int top = oy[cur] > oy[i] ? oy[cur] : oy[i];
                        int bottom = oy[cur] + oh[cur] < oy[i] + oh[i] ? oy[cur] + oh[cur] : oy[i] + oh[i];
                        overlap = bottom - top;
                        if (overlap < 0) overlap = 0;
                        gap = ox[cur] - (ox[i] + ow[i]);
                    }
                    if (overlap > best_overlap || (overlap == best_overlap && gap < best_gap)) {
                        best_overlap = overlap;
                        best_gap = gap;
                        dst = i;
                    }
                }
            }
            if (dst >= 0) {
                char cmd[192];
                snprintf(cmd, sizeof(cmd), "swaymsg 'move window to output %s; focus output %s'", names[dst], names[dst]);
                char* out = Platform_RunTerminalCommand(cmd, false);
                bool ok = succeded_sway(out);
                free(out);
                free(outs);
                return ok;
            }
            free(outs);
            return false;
        }
        free(outs);
    } else if (outs) {
        free(outs);
    }
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "swaymsg move window to output %s", right ? "right" : "left");
    char* out = Platform_RunTerminalCommand(cmd, false);
    bool ok = succeded_sway(out);
    free(out);
    { PROFILE_END_VAR(_pt, "move_active_window_to_monitor_sway"); return ok; }
}

bool minimize_active_window_sway(void) {
        PROFILE_START(_pt);
sway_ensure_sock();
    const char* sock = getenv("SWAYSOCK");
    if (!sock || !sock[0]) sock = getenv("I3SOCK");
    if (!sway_sock_live(sock)) { PROFILE_END_VAR(_pt, "minimize_active_window_sway"); return false; }
    char* out = Platform_RunTerminalCommand("swaymsg move scratchpad", false);
    bool ok = succeded_sway(out);
    free(out);
    if (ok) { PROFILE_END_VAR(_pt, "minimize_active_window_sway"); return true; }
    out = Platform_RunTerminalCommand("swaymsg scratchpad show", false);
    ok = succeded_sway(out);
    free(out);
    if (ok) {
        out = Platform_RunTerminalCommand("swaymsg floating disable", false);
        free(out);
    }
    { PROFILE_END_VAR(_pt, "minimize_active_window_sway"); return ok; }
}

bool focus_window_left_sway(void) {
        PROFILE_START(_pt);
sway_ensure_sock();
    const char* sock = getenv("SWAYSOCK");
    if (!sock || !sock[0]) sock = getenv("I3SOCK");
    if (!sway_sock_live(sock)) { PROFILE_END_VAR(_pt, "focus_window_left_sway"); return false; }
    char* out = Platform_RunTerminalCommand("swaymsg focus left", false);
    bool ok = succeded_sway(out);
    free(out);
    { PROFILE_END_VAR(_pt, "focus_window_left_sway"); return ok; }
}

bool focus_window_right_sway(void) {
        PROFILE_START(_pt);
sway_ensure_sock();
    const char* sock = getenv("SWAYSOCK");
    if (!sock || !sock[0]) sock = getenv("I3SOCK");
    if (!sway_sock_live(sock)) { PROFILE_END_VAR(_pt, "focus_window_right_sway"); return false; }
    char* out = Platform_RunTerminalCommand("swaymsg focus right", false);
    bool ok = succeded_sway(out);
    free(out);
    { PROFILE_END_VAR(_pt, "focus_window_right_sway"); return ok; }
}

bool move_active_window_left_sway(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "move_active_window_left_sway"); return move_active_window_to_monitor_sway(false); }
}

bool move_active_window_right_sway(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "move_active_window_right_sway"); return move_active_window_to_monitor_sway(true); }
}
