#include "wm/kwin_wm.h"
#include "deps/log.h"
#include "platform/platform.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

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

static char g_kwin_qdbus[64] = {0};

static char* kwin_qdbus_call(const char* args) {
    kwin_ensure_bus();
    if (!g_kwin_qdbus[0]) {
        const char* c = "/usr/bin/qdbus6";
        if (access(c, X_OK) != 0) c = "/usr/bin/qdbus";
        snprintf(g_kwin_qdbus, sizeof(g_kwin_qdbus), "%s", c);
    }
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%s %s 2>/dev/null || echo error", g_kwin_qdbus, args);
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

static char* g_kwin_records = NULL;
static char* g_kwin_active_title = NULL;
static char* g_kwin_active_class = NULL;
static long long g_kwin_snap_at = 0;
static bool g_kwin_q_loaded = false;
static int g_kwin_q_id = -1;
static char g_kwin_q_name[64] = {0};
static unsigned g_kwin_q_seq = 0;
static bool g_kwin_q_legacy_cleared = false;
static long long g_kwin_q_load_tried_at = 0;

/* How long a window snapshot stays usable. The query costs a Script.run plus
   a journalctl scrape, so a short TTL meant every hotkey paid for a fresh
   one. Window filters only need the title to be roughly current. */
#define KWIN_SNAP_TTL_MS 1200

static const char g_kwin_q_body[] =
    "(function(){"
    "function cf(w){try{var v=w.caption;if(!v)return \"\";return String(v).replace(/[\\r\\n]+/g,\" \");}catch(e){return \"\";}}"
    "function cc(w){try{var v=w.resourceClass;if(!v)return \"\";return String(v).replace(/[\\r\\n]+/g,\" \");}catch(e){return \"\";}}"
    "var ws=workspace.stackingOrder;var o=\"\";"
    "for(var i=0;i<ws.length;i++){if(o){o+=\"\\x1f\";}o+=cf(ws[i])+\"\\x1e\"+cc(ws[i]);}"
    "print(\"FLINTQR \"+o);"
    "print(\"FLINTQA \"+(workspace.activeWindow?cf(workspace.activeWindow):\"\"));"
    "print(\"FLINTQC \"+(workspace.activeWindow?cc(workspace.activeWindow):\"\"));"
    "})();";

static bool kwin_q_load(void) {
    long long now = Platform_GetWallClockMs();
    if (now - g_kwin_q_load_tried_at < 2000 && g_kwin_q_load_tried_at != 0) return false;
    g_kwin_q_load_tried_at = now;
    if (!g_kwin_q_legacy_cleared) {
        g_kwin_q_legacy_cleared = true;
        char* leg = kwin_qdbus_call("org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript flintq");
        if (leg) free(leg);
    }
    if (g_kwin_q_name[0]) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript %s", g_kwin_q_name);
        char* pre = kwin_qdbus_call(cmd);
        if (pre) free(pre);
        g_kwin_q_name[0] = '\0';
    }
    FILE* f = fopen("/tmp/flint_kwin_q.js", "w");
    if (!f) return false;
    fputs(g_kwin_q_body, f);
    fclose(f);
    for (int round = 0; round < 3; round++) {
        snprintf(g_kwin_q_name, sizeof(g_kwin_q_name), "flintq_%d_%u", (int)getpid(), ++g_kwin_q_seq);
        char load[256];
        snprintf(load, sizeof(load), "org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript /tmp/flint_kwin_q.js %s", g_kwin_q_name);
        char* out = kwin_qdbus_call(load);
        if (!out || kwin_has_error(out)) {
            log_error("kwin q load failed out='%s'", out ? out : "null");
            if (out) free(out);
            continue;
        }
        g_kwin_q_id = atoi(out);
        free(out);
        if (g_kwin_q_id < 0) {
            log_error("kwin q load bad id");
            continue;
        }
        /* loadScript returning a valid id is enough. Verifying here meant a
           Script.run plus a 150ms sleep plus a journalctl scrape, and then
           kwin_snapshot_locked immediately did the same run and scrape again.
           If the script is actually broken the snapshot's own journal retry
           fails and clears g_kwin_q_loaded, so this self-corrects. */
        g_kwin_q_loaded = true;
        return true;
    }
    g_kwin_q_loaded = false;
    g_kwin_q_name[0] = '\0';
    return false;
}

static void kwin_snap_store(const char* blob) {
    char* rec = NULL;
    char* at = NULL;
    char* ac = NULL;
    const char* p = blob;
    while (*p) {
        const char* eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        const char* val = NULL;
        size_t vlen = 0;
        char which = 0;
        if (len > 8 && !strncmp(p, "FLINTQR ", 8)) { which = 'r'; val = p + 8; vlen = len - 8; }
        else if (len > 8 && !strncmp(p, "FLINTQA ", 8)) { which = 'a'; val = p + 8; vlen = len - 8; }
        else if (len > 8 && !strncmp(p, "FLINTQC ", 8)) { which = 'c'; val = p + 8; vlen = len - 8; }
        if (which) {
            while (vlen > 0 && (val[vlen - 1] == '\r' || val[vlen - 1] == '\n')) vlen--;
            char* v = malloc(vlen + 1);
            if (v) {
                memcpy(v, val, vlen);
                v[vlen] = '\0';
                if (which == 'r') { free(rec); rec = v; }
                else if (which == 'a') { free(at); at = v; }
                else { free(ac); ac = v; }
            }
        }
        p = eol ? eol + 1 : p + len;
    }
    if (rec) { free(g_kwin_records); g_kwin_records = rec; }
    if (at) { free(g_kwin_active_title); g_kwin_active_title = at; }
    if (ac) { free(g_kwin_active_class); g_kwin_active_class = ac; }
}

static bool kwin_snapshot_locked(void) {
    long long now = Platform_GetWallClockMs();
    if (g_kwin_records && now - g_kwin_snap_at < KWIN_SNAP_TTL_MS) return true;
    for (int tries = 0; tries < 2; tries++) {
        if (!g_kwin_q_loaded && !kwin_q_load()) return false;
        char args[128];
        snprintf(args, sizeof(args), "org.kde.KWin /Scripting/Script%d org.kde.kwin.Script.run", g_kwin_q_id);
        char* rout = kwin_qdbus_call(args);
        bool ok = rout && !kwin_has_error(rout) && (rout[0] == '\0' || !strstr(rout, "false"));
        if (!ok) log_error("kwin q run failed id=%d out='%s'", g_kwin_q_id, rout ? rout : "null");
        if (rout) free(rout);
        if (!ok) {
            g_kwin_q_loaded = false;
            continue;
        }
        for (int attempt = 0; attempt < 3; attempt++) {
            if (attempt > 0) {
                struct timespec ts;
                ts.tv_sec = 0;
                ts.tv_nsec = 50 * 1000 * 1000;
                nanosleep(&ts, NULL);
            }
            char* log = Platform_RunTerminalCommand("journalctl --user -o cat --since=\"-5s\" 2>/dev/null | grep -a -e \"FLINTQR \" -e \"FLINTQA \" -e \"FLINTQC \" | tail -6", false);
            if (log && log[0] && (strstr(log, "FLINTQR ") || strstr(log, "FLINTQA "))) {
                kwin_snap_store(log);
                free(log);
                g_kwin_snap_at = Platform_GetWallClockMs();
                return g_kwin_records != NULL;
            }
            if (!log || !log[0]) log_error("kwin journal empty attempt=%d", attempt);
            else log_error("kwin journal no marker attempt=%d bytes=%d head='%.80s'", attempt, (int)strlen(log), log);
            if (log) free(log);
        }
        g_kwin_q_loaded = false;
        return g_kwin_records != NULL;
    }
    return g_kwin_records != NULL;
}

static bool kwin_skip_caption(const char* cap) {
    if (!cap || !cap[0]) return true;
    if (!strncmp(cap, "flint-", 6)) return true;
    if (!strcasecmp(cap, "window") || !strcasecmp(cap, "colored")) return true;
    return false;
}

static void kwin_clean_copy(const char* src, size_t n, char* dst, size_t cap) {
    size_t o = 0;
    for (size_t i = 0; i < n && o + 1 < cap; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x20) continue;
        dst[o++] = (char)c;
    }
    dst[o] = '\0';
}

bool get_title_kwin(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    if (!kwin_is_active()) return false;
    pthread_mutex_lock(&g_kwin_mtx);
    bool ok = kwin_snapshot_locked() && g_kwin_active_title && g_kwin_active_title[0];
    if (ok) kwin_clean_copy(g_kwin_active_title, strlen(g_kwin_active_title), buffer, (size_t)buffer_size);
    pthread_mutex_unlock(&g_kwin_mtx);
    return ok && buffer[0] != '\0';
}

bool get_app_id_kwin(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    if (!kwin_is_active()) return false;
    pthread_mutex_lock(&g_kwin_mtx);
    bool ok = kwin_snapshot_locked() && g_kwin_active_class && g_kwin_active_class[0];
    if (ok) kwin_clean_copy(g_kwin_active_class, strlen(g_kwin_active_class), buffer, (size_t)buffer_size);
    pthread_mutex_unlock(&g_kwin_mtx);
    return ok && buffer[0] != '\0';
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

PlWindowList get_window_titles_kwin() {
    PlWindowList list = {0};
    if (!kwin_is_active()) return list;
    pthread_mutex_lock(&g_kwin_mtx);
    if (!kwin_snapshot_locked() || !g_kwin_records) {
        pthread_mutex_unlock(&g_kwin_mtx);
        return list;
    }
    const char* p = g_kwin_records;
    while (*p) {
        const char* rend = strchr(p, '\x1f');
        size_t rlen = rend ? (size_t)(rend - p) : strlen(p);
        const char* sep = memchr(p, '\x1e', rlen);
        size_t clen = sep ? (size_t)(sep - p) : rlen;
        char cap[1024];
        kwin_clean_copy(p, clen, cap, sizeof(cap));
        if (!kwin_skip_caption(cap)) {
            char** tmp = realloc(list.windows, (size_t)(list.count + 1) * sizeof(char*));
            if (!tmp) break;
            list.windows = tmp;
            list.windows[list.count++] = strdup(cap);
        }
        p = rend ? rend + 1 : p + rlen;
    }
    pthread_mutex_unlock(&g_kwin_mtx);
    return list;
}

PlWindowList get_window_names_kwin() {
    PlWindowList list = {0};
    if (!kwin_is_active()) return list;
    pthread_mutex_lock(&g_kwin_mtx);
    if (!kwin_snapshot_locked() || !g_kwin_records) {
        pthread_mutex_unlock(&g_kwin_mtx);
        return list;
    }
    const char* p = g_kwin_records;
    while (*p) {
        const char* rend = strchr(p, '\x1f');
        size_t rlen = rend ? (size_t)(rend - p) : strlen(p);
        const char* sep = memchr(p, '\x1e', rlen);
        size_t clen = sep ? (size_t)(sep - p) : rlen;
        char cap[1024];
        kwin_clean_copy(p, clen, cap, sizeof(cap));
        if (!kwin_skip_caption(cap)) {
            char buf[1024];
            if (sep) kwin_clean_copy(sep + 1, rlen - clen - 1, buf, sizeof(buf));
            else buf[0] = '\0';
            if (buf[0]) buf[0] = toupper((unsigned char)buf[0]);
            char** tmp = realloc(list.windows, (size_t)(list.count + 1) * sizeof(char*));
            if (!tmp) break;
            list.windows = tmp;
            list.windows[list.count++] = strdup(buf);
        }
        p = rend ? rend + 1 : p + rlen;
    }
    pthread_mutex_unlock(&g_kwin_mtx);
    return list;
}

PlPos get_cursor_position_kwin(void) {
    if (!kwin_is_active()) return (PlPos){0, 0};

    char* dbus_cmd = "qdbus org.kde.KWin /Scripting org.kde.KWin.Scripting.loadScript "
                     "\"\" \"print('CURSOR_POS: ' + workspace.cursorPos.x + ',' + workspace.cursorPos.y);\"";
    
    char* script_id_str = Platform_RunTerminalCommand(dbus_cmd, false);
    if (script_id_str == NULL) return (PlPos){0, 0};
    
    int script_id = atoi(script_id_str);
    free(script_id_str);

    if (script_id <= 0) return (PlPos){0, 0};

    char run_cmd[128];
    snprintf(run_cmd, sizeof(run_cmd), "qdbus org.kde.KWin /Scripting/Script%d org.kde.KWin.Script.run", script_id);
    Platform_RunTerminalCommand(run_cmd, false);

    char unload_cmd[128];
    snprintf(unload_cmd, sizeof(unload_cmd), "qdbus org.kde.KWin /Scripting/Script%d org.kde.KWin.Script.stop", script_id);
    Platform_RunTerminalCommand(unload_cmd, false);

    char* journal_output = Platform_RunTerminalCommand("journalctl --user -b 0 -u plasma-kwin_wayland --grep='CURSOR_POS:' -n 1 -q --no-pager", false);
    
    int x_val = 0;
    int y_val = 0;

    if (journal_output != NULL) {
        char* match = strstr(journal_output, "CURSOR_POS: ");
        if (match != NULL) {
            sscanf(match, "CURSOR_POS: %d,%d", &x_val, &y_val);
        }
        free(journal_output);
    }
    free(unload_cmd);

    return (PlPos){x_val, y_val};
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

bool screenshot_copy_area_kwin(void) {
    if (!kwin_is_active()) return false;
    free(Platform_RunTerminalCommand("spectacle -b -n -r -c", false));
    return true;
}

bool screenshot_copy_kwin(void) {
    if (!kwin_is_active()) return false;
    free(Platform_RunTerminalCommand("spectacle -b -n -c", false));
    return true;
}

bool screenshot_save_area_kwin(char* save_path) {
    if (!kwin_is_active() || !save_path || !save_path[0]) return false;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "spectacle -b -n -r -o \"%s\"", save_path);
    free(Platform_RunTerminalCommand(cmd, false));
    return true;
}

bool screenshot_save_kwin(char* save_path) {
    if (!kwin_is_active() || !save_path || !save_path[0]) return false;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "spectacle -b -n -o \"%s\"", save_path);
    free(Platform_RunTerminalCommand(cmd, false));
    return true;
}

char* ocr_area_kwin(void) {
    if (!kwin_is_active()) return NULL;
    char path[128];
    snprintf(path, sizeof(path), "/tmp/flint_ocr_%d.png", (int)getpid());
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "spectacle -b -n -r -o \"%s\" && tesseract \"%s\" stdout -l eng; rm -f \"%s\"", path, path, path);
    char* out = Platform_RunTerminalCommand(cmd, false);
    if (!out || !out[0]) { free(out); return NULL; }
    return out;
}
