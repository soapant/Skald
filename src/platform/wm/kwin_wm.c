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
#include <sys/stat.h>

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

static bool kwin_run_script(const char* body) {
    FILE* f = fopen("/tmp/flint_kwin.js", "w");
    if (!f) return false;
    fputs(body, f);
    fclose(f);
    char* pre = kwin_qdbus_call("org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript flint");
    if (pre) free(pre);
    char* out = kwin_qdbus_call("org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript /tmp/flint_kwin.js flint");
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

static char g_kwin_focused_json[4096] = {0};
static long long g_kwin_focused_at = 0;

static const char* kwin_focused_json(void) {
    long long now = Platform_GetWallClockMs();
    if (g_kwin_focused_json[0] && now - g_kwin_focused_at < 500) return g_kwin_focused_json;
    char* out = kwin_qdbus_call("org.kde.KWin /KWin org.kde.KWin.queryWindowInfo");
    if (!out || !out[0] || kwin_has_error(out)) {
        log_error("kwin query failed out='%s'", out ? out : "null");
        if (out) free(out);
        return NULL;
    }
    strncpy(g_kwin_focused_json, out, sizeof(g_kwin_focused_json)-1);
    g_kwin_focused_json[sizeof(g_kwin_focused_json)-1] = '\0';
    free(out);
    g_kwin_focused_at = now;
    return g_kwin_focused_json;
}

static bool kwin_json_line_value(const char* blob, const char* key, char* out, size_t cap) {
    char needle[64];
    snprintf(needle, sizeof(needle), "%s:", key);
    const char* p = strstr(blob, needle);
    if (!p) return false;
    p += strlen(needle);
    while (*p == ' ' || *p == '\t') p++;
    size_t o = 0;
    while (*p && *p != '\n' && *p != '\r' && o + 1 < cap) out[o++] = *p++;
    out[o] = '\0';
    return o > 0;
}

bool get_title_kwin(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    if (!kwin_is_active()) return false;
    return false;
}

bool get_app_id_kwin(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    if (!kwin_is_active()) return false;
    return false;
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
    return list;
}

Fl_WindowList get_window_names_kwin() {
    Fl_WindowList list = {0};
    if (!kwin_is_active()) return list;
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
