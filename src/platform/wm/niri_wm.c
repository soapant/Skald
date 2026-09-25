#include "wm/niri_wm.h"
#include "deps/log.h"
#include "core/platform.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <glob.h>
#include <time.h>
#include <sys/stat.h>

static bool niri_sock_live(const char* sock) {
    if (!sock || !sock[0]) return false;
    struct stat st;
    return stat(sock, &st) == 0;
}

bool niri_is_active(void) {
    const char* cur = getenv("NIRI_SOCKET");
    if (niri_sock_live(cur)) return true;
    if (cur && cur[0]) unsetenv("NIRI_SOCKET");
    const char* rtd = getenv("XDG_RUNTIME_DIR");
    if (!rtd || !rtd[0]) return false;
    char pat[512];
    snprintf(pat, sizeof(pat), "%s/niri*.sock", rtd);
    glob_t g;
    memset(&g, 0, sizeof(g));
    if (glob(pat, 0, NULL, &g) != 0 || g.gl_pathc == 0) {
        globfree(&g);
        return false;
    }
    const char* best = NULL;
    time_t best_mtime = 0;
    for (size_t i = 0; i < g.gl_pathc; i++) {
        struct stat st;
        if (stat(g.gl_pathv[i], &st) != 0) continue;
        if (!best || st.st_mtime >= best_mtime) {
            best = g.gl_pathv[i];
            best_mtime = st.st_mtime;
        }
    }
    if (best) {
        setenv("NIRI_SOCKET", best, 1);
        log_error("niri detected sock='%s'", best);
        globfree(&g);
        return true;
    }
    globfree(&g);
    return false;
}

static bool niri_succeded(char* output) {
    if (!output) { log_error("output is NULL"); return false; }
    if (Platform_StringContainsCaseInsensitive(output, "error")) {
        log_error("niri output='%s'", output);
        return false;
    }
    return true;
}

static size_t niri_copy_json_string(const char* s, char* out, size_t cap) {
    size_t o = 0;
    for (const char* p = s; *p && o + 1 < cap; p++) {
        if (*p == '\\' && *(p+1)) { p++; out[o++] = *p; continue; }
        if (*p == '"') break;
        out[o++] = *p;
    }
    out[o] = '\0';
    return o;
}

bool get_title_niri(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    if (!niri_is_active()) return false;
    char* json = Platform_RunTerminalCommand("niri msg --json focused-window", false);
    if (!json || !json[0]) { free(json); return false; }
    const char* tp = strstr(json, "\"title\":\"");
    bool ok = false;
    if (tp) {
        niri_copy_json_string(tp + 9, buffer, (size_t)buffer_size);
        ok = buffer[0] != '\0';
    }
    free(json);
    return ok;
}

bool get_app_id_niri(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    if (!niri_is_active()) return false;
    char* json = Platform_RunTerminalCommand("niri msg --json focused-window", false);
    if (!json || !json[0]) { free(json); return false; }
    const char* tp = strstr(json, "\"app_id\":\"");
    bool ok = false;
    if (tp) {
        niri_copy_json_string(tp + 10, buffer, (size_t)buffer_size);
        ok = buffer[0] != '\0';
    }
    free(json);
    return ok;
}

char* paste_niri(char* content) {
    if (!content) { log_error("content is NULL"); return NULL; }
    if (!niri_is_active()) return NULL;
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "wtype %s", content);
    char* out = Platform_RunTerminalCommand(buffer, false);
    if (!out) log_error("content='%s'", content ? content : "null");
    return out;
}

bool focus_window_title_niri(char* title) {
    if (!title || !*title) return false;
    if (!niri_is_active()) return false;
    while (*title && (iscntrl((unsigned char)*title) || isspace((unsigned char)*title))) title++;
    char *end = title + strlen(title);
    while (end > title && (iscntrl((unsigned char)end[-1]) || isspace((unsigned char)end[-1]))) *--end = '\0';
    if (!*title) return false;
    char* json = Platform_RunTerminalCommand("niri msg --json windows", false);
    if (!json || !json[0]) { free(json); return false; }
    int target = -1;
    const char* p = json;
    while (1) {
        const char* idpos = strstr(p, "\"id\":");
        if (!idpos) break;
        int id = atoi(idpos + 5);
        const char* tpos = strstr(idpos, "\"title\":\"");
        const char* nextid = strstr(idpos + 5, "\"id\":");
        if (!tpos || (nextid && tpos > nextid)) { p = idpos + 5; continue; }
        char name[1024];
        niri_copy_json_string(tpos + 9, name, sizeof(name));
        if (name[0] && (!strcmp(name, title) || !strcasecmp(name, title))) { target = id; break; }
        p = tpos + 9;
    }
    free(json);
    if (target < 0) return false;
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "niri msg action focus-window --id %d", target);
    char* out = Platform_RunTerminalCommand(cmd, false);
    bool ok = niri_succeded(out);
    free(out);
    return ok;
}

Fl_WindowList get_window_titles_niri() {
    Fl_WindowList list = {0};
    if (!niri_is_active()) return list;
    char* json = Platform_RunTerminalCommand("niri msg --json windows", false);
    if (!json || !json[0]) { free(json); return list; }
    const char* p = json;
    while (1) {
        const char* tpos = strstr(p, "\"title\":\"");
        if (!tpos) break;
        char buf[1024];
        niri_copy_json_string(tpos + 9, buf, sizeof(buf));
        p = tpos + 9;
        if (!buf[0]) continue;
        if (!strcasecmp(buf, "window") || !strcasecmp(buf, "colored")) continue;
        char** tmp = realloc(list.windows, (size_t)(list.count + 1) * sizeof(char*));
        if (!tmp) break;
        list.windows = tmp;
        list.windows[list.count++] = strdup(buf);
    }
    free(json);
    return list;
}

Fl_WindowList get_window_names_niri() {
    Fl_WindowList list = {0};
    if (!niri_is_active()) return list;
    char* json = Platform_RunTerminalCommand("niri msg --json windows", false);
    if (!json || !json[0]) { free(json); return list; }
    const char* p = json;
    while (1) {
        const char* tpos = strstr(p, "\"app_id\":\"");
        if (!tpos) break;
        char buf[1024];
        niri_copy_json_string(tpos + 10, buf, sizeof(buf));
        p = tpos + 10;
        if (!buf[0]) continue;
        if (!strcasecmp(buf, "window") || !strcasecmp(buf, "colored")) continue;
        buf[0] = toupper((unsigned char)buf[0]);
        char** tmp = realloc(list.windows, (size_t)(list.count + 1) * sizeof(char*));
        if (!tmp) break;
        list.windows = tmp;
        list.windows[list.count++] = strdup(buf);
    }
    free(json);
    return list;
}

Fl_Pos get_cursor_position_niri(void) {
    char cmd[200];
    snprintf(cmd, sizeof(cmd), "%s/external/slurp -p -d ", Platform_GetWorkingDir());
    
    char* output = Platform_RunCommandAndGetOutput(cmd);

    char* comma = strchr(output, ',');
    
    int x = atoi(output);     
    int y = atoi(comma + 1); 
    
    free(output);
    
    return (Fl_Pos){x, y}; 
}

bool close_active_window_niri(void) {
    if (!niri_is_active()) return false;
    char* out = Platform_RunTerminalCommand("niri msg action close-window", false);
    bool ok = niri_succeded(out);
    free(out);
    return ok;
}

bool toggle_fullscreen_active_niri(void) {
    if (!niri_is_active()) return false;
    char* out = Platform_RunTerminalCommand("niri msg action fullscreen-window", false);
    bool ok = niri_succeded(out);
    free(out);
    return ok;
}

bool move_active_window_to_monitor_niri(bool right) {
    if (!niri_is_active()) return false;
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "niri msg action move-window-to-monitor-%s", right ? "right" : "left");
    char* out = Platform_RunTerminalCommand(cmd, false);
    bool ok = niri_succeded(out);
    free(out);
    return ok;
}

bool move_active_window_left_niri(void) {
    if (!niri_is_active()) return false;
    char* out = Platform_RunTerminalCommand("niri msg action move-window-to-monitor-left", false);
    bool ok = niri_succeded(out);
    free(out);
    return ok;
}

bool move_active_window_right_niri(void) {
    if (!niri_is_active()) return false;
    char* out = Platform_RunTerminalCommand("niri msg action move-window-to-monitor-right", false);
    bool ok = niri_succeded(out);
    free(out);
    return ok;
}

bool focus_window_left_niri(void) {
    if (!niri_is_active()) return false;
    char* out = Platform_RunTerminalCommand("niri msg action focus-column-or-monitor-left", false);
    bool ok = niri_succeded(out);
    free(out);
    return ok;
}

bool focus_window_right_niri(void) {
    if (!niri_is_active()) return false;
    char* out = Platform_RunTerminalCommand("niri msg action focus-column-or-monitor-right", false);
    bool ok = niri_succeded(out);
    free(out);
    return ok;
}

bool minimize_active_window_niri(void) {
    if (!niri_is_active()) return false;
    return false;
}
