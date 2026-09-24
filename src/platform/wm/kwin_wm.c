#include "wm/kwin_wm.h"
#include "deps/log.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_text.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/stat.h>

static pthread_mutex_t g_kwin_mtx = PTHREAD_MUTEX_INITIALIZER;

bool kwin_is_active(void) {
    const char* v = getenv("KDE_SESSION_VERSION");
    if (v && v[0]) return true;
    const char* d = getenv("XDG_CURRENT_DESKTOP");
    if (d && d[0] && strstr(d, "KDE")) return true;
    return false;
}

static bool kwin_has_error(char* output) {
    if (!output) return true;
    return Platform_StringContainsCaseInsensitive(output, "error");
}

static void kwin_ensure_bus(void) {
    const char* cur = getenv("DBUS_SESSION_BUS_ADDRESS");
    if (cur && cur[0]) return;
    const char* rtd = getenv("XDG_RUNTIME_DIR");
    if (!rtd || !rtd[0]) return;
    char sock[512];
    snprintf(sock, sizeof(sock), "%s/bus", rtd);
    struct stat st;
    if (stat(sock, &st) != 0) return;
    char addr[600];
    snprintf(addr, sizeof(addr), "unix:path=%s", sock);
    setenv("DBUS_SESSION_BUS_ADDRESS", addr, 1);
    log_error("kwin bus addr='%s'", addr);
}

static char* kwin_qdbus_call(const char* args) {
    kwin_ensure_bus();
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "QDBUS=$(command -v qdbus6 || command -v qdbus); if [ -z \"$QDBUS\" ]; then echo \"error: no qdbus\"; else \"$QDBUS\" %s; fi", args);
    return Platform_RunTerminalCommand(cmd, false);
}

static bool kwin_run_script_inner(const char* body) {
    FILE* f = fopen("/tmp/flint_kwin_do.js", "w");
    if (!f) return false;
    fputs(body, f);
    fclose(f);
    char* pre = kwin_qdbus_call("org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript flint");
    if (pre) free(pre);
    char* out = kwin_qdbus_call("org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript /tmp/flint_kwin_do.js flint");
    if (!out || kwin_has_error(out)) {
        if (out) free(out);
        log_error("kwin loadScript failed");
        return false;
    }
    int id = atoi(out);
    free(out);
    char args[128];
    snprintf(args, sizeof(args), "org.kde.KWin /Scripting/Script%d org.kde.kwin.Script.run", id);
    char* out2 = kwin_qdbus_call(args);
    bool ok = out2 && !kwin_has_error(out2);
    if (!ok) log_error("kwin run failed id=%d out='%s'", id, out2 ? out2 : "null");
    if (out2) free(out2);
    char* out3 = kwin_qdbus_call("org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript flint");
    if (out3) free(out3);
    return ok;
}

static bool kwin_run_script(const char* body) {
    pthread_mutex_lock(&g_kwin_mtx);
    bool ok = kwin_run_script_inner(body);
    pthread_mutex_unlock(&g_kwin_mtx);
    return ok;
}

static unsigned g_kwin_qid = 0;
static char g_kwin_qexpr[512] = {0};
static char* g_kwin_qpayload = NULL;
static long long g_kwin_qat = 0;

static char* kwin_eval_print_inner(const char* expr) {
    long long now = Platform_GetWallClockMs();
    if (g_kwin_qpayload && now - g_kwin_qat < 500 && !strcmp(g_kwin_qexpr, expr)) return g_kwin_qpayload;
    char marker[32];
    snprintf(marker, sizeof(marker), "FLINTQ%u", ++g_kwin_qid);
    char body[4096];
    snprintf(body, sizeof(body), "print(\"%s \" + (%s));", marker, expr);
    FILE* f = fopen("/tmp/flint_kwin_q.js", "w");
    if (!f) return NULL;
    fputs(body, f);
    fclose(f);
    char* pre = kwin_qdbus_call("org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript flintq");
    if (pre) free(pre);
    char* out = kwin_qdbus_call("org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript /tmp/flint_kwin_q.js flintq");
    if (!out || kwin_has_error(out)) {
        if (out) free(out);
        return NULL;
    }
    int id = atoi(out);
    free(out);
    char args[128];
    snprintf(args, sizeof(args), "org.kde.KWin /Scripting/Script%d org.kde.kwin.Script.run", id);
    char* out2 = kwin_qdbus_call(args);
    if (!out2 || kwin_has_error(out2)) {
        if (out2) free(out2);
        return NULL;
    }
    free(out2);
    char* out3 = kwin_qdbus_call("org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript flintq");
    if (out3) free(out3);
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "journalctl --user -o cat --since=\"-30s\" 2>/dev/null | grep -a \"%s\" | tail -1", marker);
    char* log = NULL;
    for (int attempt = 0; attempt < 8; attempt++) {
        if (attempt > 0) {
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = 100 * 1000 * 1000;
            nanosleep(&ts, NULL);
        }
        char* got = Platform_RunTerminalCommand(cmd, false);
        if (got && got[0] && strstr(got, marker)) {
            log = got;
            break;
        }
        if (got) free(got);
    }
    if (!log || !log[0]) {
        if (log) free(log);
        return NULL;
    }
    char* start = strstr(log, marker);
    if (!start) { free(log); return NULL; }
    start += strlen(marker);
    if (*start == ' ') start++;
    size_t n = strlen(start);
    while (n > 0 && (start[n-1] == '\n' || start[n-1] == '\r')) start[--n] = '\0';
    free(g_kwin_qpayload);
    g_kwin_qpayload = strdup(start);
    free(log);
    if (!g_kwin_qpayload) return NULL;
    strncpy(g_kwin_qexpr, expr, sizeof(g_kwin_qexpr)-1);
    g_kwin_qexpr[sizeof(g_kwin_qexpr)-1] = '\0';
    g_kwin_qat = now;
    return g_kwin_qpayload;
}

static char* kwin_eval_print(const char* expr) {
    pthread_mutex_lock(&g_kwin_mtx);
    char* r = kwin_eval_print_inner(expr);
    char* out = r ? strdup(r) : NULL;
    pthread_mutex_unlock(&g_kwin_mtx);
    return out;
}

static size_t kwin_copy_field(const char* src, char stop, char* dst, size_t cap) {
    size_t o = 0;
    for (const char* p = src; *p && *p != stop && o + 1 < cap; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20) continue;
        dst[o++] = *p;
    }
    dst[o] = '\0';
    return o;
}

bool get_title_kwin(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    if (!kwin_is_active()) return false;
    char* payload = kwin_eval_print("(workspace.activeWindow ? workspace.activeWindow.caption : \"\")");
    if (!payload || !payload[0]) { if (payload) free(payload); return false; }
    kwin_copy_field(payload, 0, buffer, (size_t)buffer_size);
    free(payload);
    return buffer[0] != '\0';
}

bool get_app_id_kwin(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    if (!kwin_is_active()) return false;
    char* payload = kwin_eval_print("(workspace.activeWindow ? workspace.activeWindow.resourceClass : \"\")");
    if (!payload || !payload[0]) { if (payload) free(payload); return false; }
    kwin_copy_field(payload, 0, buffer, (size_t)buffer_size);
    free(payload);
    return buffer[0] != '\0';
}

char* paste_kwin(char* content) {
    if (!content) { log_error("content is NULL"); return NULL; }
    if (!kwin_is_active()) return NULL;
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "wtype %s", content);
    char* out = Platform_RunTerminalCommand(buffer, false);
    if (!out) log_error("content='%s'", content ? content : "null");
    return out;
}

static bool kwin_focus_js_cmd(const char* title, char* out, size_t cap) {
    char esc[1024];
    size_t o = 0;
    for (const char* s = title; *s && o + 2 < sizeof(esc); s++) {
        if (*s == '\\' || *s == '"') {
            esc[o++] = '\\';
            esc[o++] = *s;
        } else if (*s == '\n' || *s == '\r') {
            continue;
        } else {
            esc[o++] = *s;
        }
    }
    esc[o] = '\0';
    if (!esc[0]) return false;
    char low[1024];
    strncpy(low, esc, sizeof(low)-1);
    low[sizeof(low)-1] = '\0';
    if (low[0]) low[0] = tolower((unsigned char)low[0]);
    snprintf(out, cap, "var __t=\"%s\";var __l=\"%s\";var __ws=workspace.stackingOrder;for(var i=0;i<__ws.length;i++){var __c=__ws[i].caption;if(__c==__t||__c.toLowerCase()==__l){workspace.activeWindow=__ws[i];break;}}", esc, low);
    return true;
}

bool focus_window_title_kwin(char* title) {
    if (!title) { log_error("title is NULL"); return false; }
    if (!kwin_is_active()) return false;
    while (*title && (iscntrl((unsigned char)*title) || isspace((unsigned char)*title))) title++;
    char *end = title + strlen(title);
    while (end > title && (iscntrl((unsigned char)end[-1]) || isspace((unsigned char)end[-1]))) *--end = '\0';
    if (!*title) { log_error("title empty after trim"); return false; }
    char body[2300];
    if (!kwin_focus_js_cmd(title, body, sizeof(body))) return false;
    return kwin_run_script(body);
}

Fl_WindowList get_window_titles_kwin() {
    Fl_WindowList list = {0};
    if (!kwin_is_active()) return list;
    char* payload = kwin_eval_print("(function(){var __ws=workspace.stackingOrder;var __o=\"\";for(var i=0;i<__ws.length;i++){__o+=__ws[i].caption+\"\\x1f\";}return __o;})()");
    if (!payload || !payload[0]) { if (payload) free(payload); return list; }
    const char* p = payload;
    while (*p) {
        char buf[1024];
        size_t o = 0;
        while (*p && *p != '\x1f' && o + 1 < sizeof(buf)) {
            unsigned char c = (unsigned char)*p++;
            if (c < 0x20) continue;
            buf[o++] = (char)c;
        }
        if (*p == '\x1f') p++;
        buf[o] = '\0';
        if (!buf[0]) continue;
        if (!strcasecmp(buf, "window") || !strcasecmp(buf, "colored")) continue;
        char** tmp = realloc(list.windows, (size_t)(list.count + 1) * sizeof(char*));
        if (!tmp) break;
        list.windows = tmp;
        list.windows[list.count++] = strdup(buf);
    }
    free(payload);
    return list;
}

Fl_WindowList get_window_names_kwin() {
    Fl_WindowList list = {0};
    if (!kwin_is_active()) return list;
    char* payload = kwin_eval_print("(function(){var __ws=workspace.stackingOrder;var __o=\"\";for(var i=0;i<__ws.length;i++){__o+=__ws[i].resourceClass+\"\\x1f\";}return __o;})()");
    if (!payload || !payload[0]) { if (payload) free(payload); return list; }
    const char* p = payload;
    while (*p) {
        char buf[1024];
        size_t o = 0;
        while (*p && *p != '\x1f' && o + 1 < sizeof(buf)) {
            unsigned char c = (unsigned char)*p++;
            if (c < 0x20) continue;
            buf[o++] = (char)c;
        }
        if (*p == '\x1f') p++;
        buf[o] = '\0';
        if (!buf[0]) continue;
        if (!strcasecmp(buf, "window") || !strcasecmp(buf, "colored")) continue;
        buf[0] = toupper((unsigned char)buf[0]);
        char** tmp = realloc(list.windows, (size_t)(list.count + 1) * sizeof(char*));
        if (!tmp) break;
        list.windows = tmp;
        list.windows[list.count++] = strdup(buf);
    }
    free(payload);
    return list;
}

Fl_Pos get_cursor_position_kwin(void) {
    if (!kwin_is_active()) return (Fl_Pos){0, 0};
    return (Fl_Pos){0, 0};
}

bool close_active_window_kwin(void) {
    if (!kwin_is_active()) return false;
    return kwin_run_script("workspace.slotWindowClose();");
}

bool toggle_fullscreen_active_kwin(void) {
    if (!kwin_is_active()) return false;
    return kwin_run_script("workspace.slotWindowFullScreen();");
}

bool move_active_window_to_monitor_kwin(bool right) {
    if (!kwin_is_active()) return false;
    if (right) return kwin_run_script("workspace.slotWindowToRightScreen();");
    return kwin_run_script("workspace.slotWindowToLeftScreen();");
}

bool move_active_window_left_kwin(void) {
    if (!kwin_is_active()) return false;
    return kwin_run_script("workspace.slotWindowToLeftScreen();");
}

bool move_active_window_right_kwin(void) {
    if (!kwin_is_active()) return false;
    return kwin_run_script("workspace.slotWindowToRightScreen();");
}

bool focus_window_left_kwin(void) {
    if (!kwin_is_active()) return false;
    return kwin_run_script("workspace.slotSwitchWindowLeft();");
}

bool focus_window_right_kwin(void) {
    if (!kwin_is_active()) return false;
    return kwin_run_script("workspace.slotSwitchWindowRight();");
}

bool minimize_active_window_kwin(void) {
    if (!kwin_is_active()) return false;
    return kwin_run_script("workspace.slotWindowMinimize();");
}
