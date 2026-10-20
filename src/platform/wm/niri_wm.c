#include "wm/niri_wm.h"
#include "deps/log.h"
#include "platform/platform.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <glob.h>
#include <time.h>
#include <sys/stat.h>

#include "core/timer.h"
static bool niri_sock_live(const char* sock) {
        PROFILE_START(_pt);
if (!sock || !sock[0]) { PROFILE_END_VAR(_pt, "niri_sock_live"); return false; }
    struct stat st;
    { PROFILE_END_VAR(_pt, "niri_sock_live"); return stat(sock, &st) == 0; }
}

bool niri_is_active(void) {
        PROFILE_START(_pt);
const char* cur = getenv("NIRI_SOCKET");
    if (niri_sock_live(cur)) { PROFILE_END_VAR(_pt, "niri_is_active"); return true; }
    if (cur && cur[0]) unsetenv("NIRI_SOCKET");
    const char* rtd = getenv("XDG_RUNTIME_DIR");
    if (!rtd || !rtd[0]) { PROFILE_END_VAR(_pt, "niri_is_active"); return false; }
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
    { PROFILE_END_VAR(_pt, "niri_is_active"); return false; }
}

static bool niri_succeded(char* output) {
        PROFILE_START(_pt);
if (!output) { log_error("output is NULL"); return false; }
    if (Platform_StringContainsCaseInsensitive(output, "error")) {
        log_error("niri output='%s'", output);
        return false;
    }
    { PROFILE_END_VAR(_pt, "niri_succeded"); return true; }
}

static size_t niri_copy_json_string(const char* s, char* out, size_t cap) {
        PROFILE_START(_pt);
size_t o = 0;
    for (const char* p = s; *p && o + 1 < cap; p++) {
        if (*p == '\\' && *(p+1)) { p++; out[o++] = *p; continue; }
        if (*p == '"') break;
        out[o++] = *p;
    }
    out[o] = '\0';
    { PROFILE_END_VAR(_pt, "niri_copy_json_string"); return o; }
}

bool get_title_niri(char *buffer, int buffer_size) {
        PROFILE_START(_pt);
if (!buffer || buffer_size <= 0) { PROFILE_END_VAR(_pt, "get_title_niri"); return false; }
    buffer[0] = '\0';
    if (!niri_is_active()) { PROFILE_END_VAR(_pt, "get_title_niri"); return false; }
    char* json = Platform_RunTerminalCommand("niri msg --json focused-window", false);
    if (!json || !json[0]) { free(json); return false; }
    const char* tp = strstr(json, "\"title\":\"");
    bool ok = false;
    if (tp) {
        niri_copy_json_string(tp + 9, buffer, (size_t)buffer_size);
        ok = buffer[0] != '\0';
    }
    free(json);
    { PROFILE_END_VAR(_pt, "get_title_niri"); return ok; }
}

bool get_app_id_niri(char *buffer, int buffer_size) {
        PROFILE_START(_pt);
if (!buffer || buffer_size <= 0) { PROFILE_END_VAR(_pt, "get_app_id_niri"); return false; }
    buffer[0] = '\0';
    if (!niri_is_active()) { PROFILE_END_VAR(_pt, "get_app_id_niri"); return false; }
    char* json = Platform_RunTerminalCommand("niri msg --json focused-window", false);
    if (!json || !json[0]) { free(json); return false; }
    const char* tp = strstr(json, "\"app_id\":\"");
    bool ok = false;
    if (tp) {
        niri_copy_json_string(tp + 10, buffer, (size_t)buffer_size);
        ok = buffer[0] != '\0';
    }
    free(json);
    { PROFILE_END_VAR(_pt, "get_app_id_niri"); return ok; }
}

char* paste_niri(char* content) {
        PROFILE_START(_pt);
if (!content) { log_error("content is NULL"); return NULL; }
    if (!niri_is_active()) { PROFILE_END_VAR(_pt, "paste_niri"); return NULL; }
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "wtype %s", content);
    char* out = Platform_RunTerminalCommand(buffer, false);
    if (!out) log_error("content='%s'", content ? content : "null");
    { PROFILE_END_VAR(_pt, "paste_niri"); return out; }
}

bool focus_window_title_niri(char* title) {
        PROFILE_START(_pt);
if (!title || !*title) { PROFILE_END_VAR(_pt, "focus_window_title_niri"); return false; }
    if (!niri_is_active()) { PROFILE_END_VAR(_pt, "focus_window_title_niri"); return false; }
    while (*title && (iscntrl((unsigned char)*title) || isspace((unsigned char)*title))) title++;
    char *end = title + strlen(title);
    while (end > title && (iscntrl((unsigned char)end[-1]) || isspace((unsigned char)end[-1]))) *--end = '\0';
    if (!*title) { PROFILE_END_VAR(_pt, "focus_window_title_niri"); return false; }
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
    if (target < 0) { PROFILE_END_VAR(_pt, "focus_window_title_niri"); return false; }
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "niri msg action focus-window --id %d", target);
    char* out = Platform_RunTerminalCommand(cmd, false);
    bool ok = niri_succeded(out);
    free(out);
    { PROFILE_END_VAR(_pt, "focus_window_title_niri"); return ok; }
}

PlWindowList get_window_titles_niri() {
        PROFILE_START(_pt);
PlWindowList list = {0};
    if (!niri_is_active()) { PROFILE_END_VAR(_pt, "get_window_titles_niri"); return list; }
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
    { PROFILE_END_VAR(_pt, "get_window_titles_niri"); return list; }
}

PlWindowList get_window_names_niri() {
        PROFILE_START(_pt);
PlWindowList list = {0};
    if (!niri_is_active()) { PROFILE_END_VAR(_pt, "get_window_names_niri"); return list; }
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
    { PROFILE_END_VAR(_pt, "get_window_names_niri"); return list; }
}


bool close_active_window_niri(void) {
        PROFILE_START(_pt);
if (!niri_is_active()) { PROFILE_END_VAR(_pt, "close_active_window_niri"); return false; }
    char* out = Platform_RunTerminalCommand("niri msg action close-window", false);
    bool ok = niri_succeded(out);
    free(out);
    { PROFILE_END_VAR(_pt, "close_active_window_niri"); return ok; }
}

bool toggle_fullscreen_active_niri(void) {
        PROFILE_START(_pt);
if (!niri_is_active()) { PROFILE_END_VAR(_pt, "toggle_fullscreen_active_niri"); return false; }
    char* out = Platform_RunTerminalCommand("niri msg action fullscreen-window", false);
    bool ok = niri_succeded(out);
    free(out);
    { PROFILE_END_VAR(_pt, "toggle_fullscreen_active_niri"); return ok; }
}

bool move_active_window_to_monitor_niri(bool right) {
        PROFILE_START(_pt);
if (!niri_is_active()) { PROFILE_END_VAR(_pt, "move_active_window_to_monitor_niri"); return false; }
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "niri msg action move-window-to-monitor-%s", right ? "right" : "left");
    char* out = Platform_RunTerminalCommand(cmd, false);
    bool ok = niri_succeded(out);
    free(out);
    { PROFILE_END_VAR(_pt, "move_active_window_to_monitor_niri"); return ok; }
}

bool move_active_window_left_niri(void) {
        PROFILE_START(_pt);
if (!niri_is_active()) { PROFILE_END_VAR(_pt, "move_active_window_left_niri"); return false; }
    char* out = Platform_RunTerminalCommand("niri msg action move-window-to-monitor-left", false);
    bool ok = niri_succeded(out);
    free(out);
    { PROFILE_END_VAR(_pt, "move_active_window_left_niri"); return ok; }
}

bool move_active_window_right_niri(void) {
        PROFILE_START(_pt);
if (!niri_is_active()) { PROFILE_END_VAR(_pt, "move_active_window_right_niri"); return false; }
    char* out = Platform_RunTerminalCommand("niri msg action move-window-to-monitor-right", false);
    bool ok = niri_succeded(out);
    free(out);
    { PROFILE_END_VAR(_pt, "move_active_window_right_niri"); return ok; }
}

bool focus_window_left_niri(void) {
        PROFILE_START(_pt);
if (!niri_is_active()) { PROFILE_END_VAR(_pt, "focus_window_left_niri"); return false; }
    char* out = Platform_RunTerminalCommand("niri msg action focus-column-or-monitor-left", false);
    bool ok = niri_succeded(out);
    free(out);
    { PROFILE_END_VAR(_pt, "focus_window_left_niri"); return ok; }
}

bool focus_window_right_niri(void) {
        PROFILE_START(_pt);
if (!niri_is_active()) { PROFILE_END_VAR(_pt, "focus_window_right_niri"); return false; }
    char* out = Platform_RunTerminalCommand("niri msg action focus-column-or-monitor-right", false);
    bool ok = niri_succeded(out);
    free(out);
    { PROFILE_END_VAR(_pt, "focus_window_right_niri"); return ok; }
}

bool minimize_active_window_niri(void) {
        PROFILE_START(_pt);
if (!niri_is_active()) { PROFILE_END_VAR(_pt, "minimize_active_window_niri"); return false; }
    { PROFILE_END_VAR(_pt, "minimize_active_window_niri"); return false; }
}
