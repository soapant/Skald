#include "helpers/helpers_text.h"
#include "wm/kwin_wm.h"
#include "wm/wm.h"
#include "deps/log.h"
#include "platform/platform.h"
#include "helpers/x11/env_helpers_x11.h"
#include <time.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
static pthread_mutex_t kwin_mtx = PTHREAD_MUTEX_INITIALIZER;

bool kwin_is_active(void) {
  const char *v = getenv("KDE_SESSION_VERSION");
  if (v && v[0])
    return true;
  const char *d = getenv("XDG_CURRENT_DESKTOP");
  if (d && d[0] && strstr(d, "KDE"))
    return true;
  return false;
}

static bool kwin_has_error(char *output) {

  if (!output)
    return true;
  return string_contains_ci(output, "error");
}

static void kwin_ensure_bus(void) {

  const char *cur = getenv("DBUS_SESSION_BUS_ADDRESS");
  if (cur && cur[0]) {
    return;
  }
  const char *rtd = x11_env_xdg_runtime_dir();
  if (!rtd || !rtd[0]) {
    return;
  }
  char sock[512];
  snprintf(sock, sizeof(sock), "%s/bus", rtd);
  struct stat st;
  if (stat(sock, &st) != 0) {
    return;
  }
  char addr[600];
  snprintf(addr, sizeof(addr), "unix:path=%s", sock);
  setenv("DBUS_SESSION_BUS_ADDRESS", addr, 1);
  log_error("kwin bus addr='%s'", addr);
}

static char kwin_qdbus[64] = {0};

static int kwin_make_temp_js(char *path, size_t path_cap) {
  char tmpl[96];
  snprintf(tmpl, sizeof(tmpl), "/tmp/skald_kwin_XXXXXX.js");
  int fd = mkstemp(tmpl);
  if (fd < 0) {
    log_error("kwin temp JS failed: %s", strerror(errno));
    return -1;
  }
  snprintf(path, path_cap, "%s", tmpl);
  return fd;
}

static char *kwin_qdbus_call(const char *args) {

  kwin_ensure_bus();
  if (!kwin_qdbus[0]) {
    const char *c = "/usr/bin/qdbus6";
    if (access(c, X_OK) != 0)
      c = "/usr/bin/qdbus";
    snprintf(kwin_qdbus, sizeof(kwin_qdbus), "%s", c);
  }
  char cmd[2048];
  snprintf(cmd, sizeof(cmd), "%s %s 2>/dev/null || echo error", kwin_qdbus,
           args);
  return Platform_RunTerminalCommand(cmd, false);
}

static bool kwin_write_js(const char *body, char *js_path, size_t path_cap) {
  js_path[0] = '\0';
  int fd = kwin_make_temp_js(js_path, path_cap);
  if (fd < 0)
    return false;
  FILE *f = fdopen(fd, "w");
  if (!f) {
    close(fd);
    unlink(js_path);
    return false;
  }
  fputs(body, f);
  fclose(f);
  return true;
}

static bool kwin_run_script_inner(const char *body) {

  char js_path[96] = {0};
  if (!kwin_write_js(body, js_path, sizeof(js_path)))
    return false;
  char *pre = kwin_qdbus_call(
      "org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript skald");
  if (pre)
    free(pre);
  char load[256];
  snprintf(load, sizeof(load),
           "org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript %s skald",
           js_path);
  char *out = kwin_qdbus_call(load);
  unlink(js_path);
  if (!out || kwin_has_error(out)) {
    if (out)
      free(out);
    log_error("kwin loadScript failed");
    return false;
  }
  int id = atoi(out);
  free(out);
  char args[128];
  snprintf(args, sizeof(args),
           "org.kde.KWin /Scripting/Script%d org.kde.kwin.Script.run", id);
  char *out2 = kwin_qdbus_call(args);
  bool ok = out2 && !kwin_has_error(out2);
  if (!ok)
    log_error("kwin run failed id=%d out='%s'", id, out2 ? out2 : "null");
  if (out2)
    free(out2);
  char *out3 = kwin_qdbus_call(
      "org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript skald");
  if (out3)
    free(out3);
  return ok;
}

static bool kwin_run_script(const char *body) {

  pthread_mutex_lock(&kwin_mtx);
  bool ok = kwin_run_script_inner(body);
  pthread_mutex_unlock(&kwin_mtx);
  return ok;
}

static char *kwin_records = NULL;
static char *kwin_active_title = NULL;
static char *kwin_active_class = NULL;
static long long kwin_snap_at = 0;
static bool kwin_q_loaded = false;
static int kwin_q_id = -1;
static char kwin_q_name[64] = {0};
static unsigned kwin_q_seq = 0;
static bool kwin_q_legacy_cleared = false;
static long long kwin_q_load_tried_at = 0;

#define KWIN_SNAP_TTL_MS 1200

static const char kwin_q_body[] =
    "(function(){"
    "function cf(w){try{var v=w.caption;if(!v)return \"\";return "
    "String(v).replace(/[\\r\\n]+/g,\" \");}catch(e){return \"\";}}"
    "function cc(w){try{var v=w.resourceClass;if(!v)return \"\";return "
    "String(v).replace(/[\\r\\n]+/g,\" \");}catch(e){return \"\";}}"
    "var ws=workspace.stackingOrder;var o=\"\";"
    "for(var "
    "i=0;i<ws.length;i++){if(o){o+=\"\\x1f\";}o+=cf(ws[i])+\"\\x1e\"+cc(ws[i]);"
    "}"
    "print(\"SKALDQR \"+o);"
    "print(\"SKALDQA "
    "\"+(workspace.activeWindow?cf(workspace.activeWindow):\"\"));"
    "print(\"SKALDQC "
    "\"+(workspace.activeWindow?cc(workspace.activeWindow):\"\"));"
    "})();";

static bool kwin_q_load(void) {

  long long now = Platform_GetWallClockMs();
  if (now - kwin_q_load_tried_at < 2000 && kwin_q_load_tried_at != 0)
    return false;
  kwin_q_load_tried_at = now;
  if (!kwin_q_legacy_cleared) {
    kwin_q_legacy_cleared = true;
    char *leg = kwin_qdbus_call(
        "org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript skaldq");
    if (leg)
      free(leg);
  }
  if (kwin_q_name[0]) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript %s",
             kwin_q_name);
    char *pre = kwin_qdbus_call(cmd);
    if (pre)
      free(pre);
    kwin_q_name[0] = '\0';
  }
  char js_path[96] = {0};
  if (!kwin_write_js(kwin_q_body, js_path, sizeof(js_path)))
    return false;
  for (int round = 0; round < 3; round++) {
    snprintf(kwin_q_name, sizeof(kwin_q_name), "skaldq_%d_%u", (int)getpid(),
             ++kwin_q_seq);
    char load[512];
    snprintf(load, sizeof(load),
             "org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "
             "%s %s",
             js_path, kwin_q_name);
    char *out = kwin_qdbus_call(load);
    if (!out || kwin_has_error(out)) {
      log_error("kwin q load failed out='%s'", out ? out : "null");
      if (out)
        free(out);
      continue;
    }
    kwin_q_id = atoi(out);
    free(out);
    if (kwin_q_id < 0) {
      log_error("kwin q load bad id");
      continue;
    }
    kwin_q_loaded = true;
    unlink(js_path);
    return true;
  }
  kwin_q_loaded = false;
  kwin_q_name[0] = '\0';
  unlink(js_path);
  return false;
}

static void kwin_snap_store(const char *blob) {

  char *rec = NULL;
  char *at = NULL;
  char *ac = NULL;
  const char *p = blob;
  while (*p) {
    const char *eol = strchr(p, '\n');
    size_t len = eol ? (size_t)(eol - p) : strlen(p);
    const char *val = NULL;
    size_t vlen = 0;
    char which = 0;
    if (len > 8 && !strncmp(p, "SKALDQR ", 8)) {
      which = 'r';
      val = p + 8;
      vlen = len - 8;
    } else if (len > 8 && !strncmp(p, "SKALDQA ", 8)) {
      which = 'a';
      val = p + 8;
      vlen = len - 8;
    } else if (len > 8 && !strncmp(p, "SKALDQC ", 8)) {
      which = 'c';
      val = p + 8;
      vlen = len - 8;
    }
    if (which) {
      while (vlen > 0 && (val[vlen - 1] == '\r' || val[vlen - 1] == '\n'))
        vlen--;
      char *v = malloc(vlen + 1);
      if (v) {
        memcpy(v, val, vlen);
        v[vlen] = '\0';
        if (which == 'r') {
          free(rec);
          rec = v;
        } else if (which == 'a') {
          free(at);
          at = v;
        } else {
          free(ac);
          ac = v;
        }
      }
    }
    p = eol ? eol + 1 : p + len;
  }
  if (rec) {
    free(kwin_records);
    kwin_records = rec;
  }
  if (at) {
    free(kwin_active_title);
    kwin_active_title = at;
  }
  if (ac) {
    free(kwin_active_class);
    kwin_active_class = ac;
  }
}

static bool kwin_snapshot_locked(void) {

  long long now = Platform_GetWallClockMs();
  if (kwin_records && now - kwin_snap_at < KWIN_SNAP_TTL_MS)
    return true;
  for (int tries = 0; tries < 2; tries++) {
    if (!kwin_q_loaded && !kwin_q_load())
      return false;
    char args[128];
    snprintf(args, sizeof(args),
             "org.kde.KWin /Scripting/Script%d org.kde.kwin.Script.run",
             kwin_q_id);
    char *rout = kwin_qdbus_call(args);
    bool ok = rout && !kwin_has_error(rout) &&
              (rout[0] == '\0' || !strstr(rout, "false"));
    if (!ok)
      log_error("kwin q run failed id=%d out='%s'", kwin_q_id,
                rout ? rout : "null");
    if (rout)
      free(rout);
    if (!ok) {
      kwin_q_loaded = false;
      continue;
    }
    for (int attempt = 0; attempt < 3; attempt++) {
      if (attempt > 0) {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 50 * 1000 * 1000;
        nanosleep(&ts, NULL);
      }
      char *log = Platform_RunTerminalCommand(
          "journalctl --user -o cat --since=\"-5s\" 2>/dev/null | grep -a -e "
          "\"SKALDQR \" -e \"SKALDQA \" -e \"SKALDQC \" | tail -6",
          false);
      if (log && log[0] &&
          (strstr(log, "SKALDQR ") || strstr(log, "SKALDQA "))) {
        kwin_snap_store(log);
        free(log);
        kwin_snap_at = Platform_GetWallClockMs();
        return kwin_records != NULL;
      }
      if (!log || !log[0])
        log_error("kwin journal empty attempt=%d", attempt);
      else
        log_error("kwin journal no marker attempt=%d bytes=%d head='%.80s'",
                  attempt, (int)strlen(log), log);
      if (log)
        free(log);
    }
    kwin_q_loaded = false;
    return kwin_records != NULL;
  }
  return kwin_records != NULL;
}

static bool kwin_skip_caption(const char *cap) {

  if (!cap || !cap[0])
    return true;
  if (!strncmp(cap, "skald-", 6))
    return true;
  if (!strcasecmp(cap, "window") || !strcasecmp(cap, "colored"))
    return true;
  return false;
}

static void kwin_clean_copy(const char *src, size_t n, char *dst, size_t cap) {

  size_t o = 0;
  for (size_t i = 0; i < n && o + 1 < cap; i++) {
    unsigned char c = (unsigned char)src[i];
    if (c < 0x20)
      continue;
    dst[o++] = (char)c;
  }
  dst[o] = '\0';
}

static bool kwin_active_locked_copy(const char *src, char *buffer,
                                    int buffer_size) {
  pthread_mutex_lock(&kwin_mtx);
  bool ok = kwin_snapshot_locked() && src && src[0];
  if (ok)
    kwin_clean_copy(src, strlen(src), buffer, (size_t)buffer_size);
  pthread_mutex_unlock(&kwin_mtx);
  return ok && buffer[0] != '\0';
}

bool get_title_kwin(char *buffer, int buffer_size) {
  if (!buffer || buffer_size <= 0)
    return false;
  buffer[0] = '\0';
  if (!kwin_is_active())
    return false;
  return kwin_active_locked_copy(kwin_active_title, buffer, buffer_size);
}

bool get_app_id_kwin(char *buffer, int buffer_size) {
  if (!buffer || buffer_size <= 0)
    return false;
  buffer[0] = '\0';
  if (!kwin_is_active())
    return false;
  return kwin_active_locked_copy(kwin_active_class, buffer, buffer_size);
}

char *paste_kwin(const char *content) {
  if (!kwin_is_active())
    return NULL;
  return x11_paste_wtype(content);
}

static bool kwin_focus_js_cmd(const char *title, char *out, size_t cap) {

  char esc[1024];
  size_t o = 0;
  for (const char *s = title; *s && o + 2 < sizeof(esc); s++) {
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
  if (!esc[0])
    return false;
  char low[1024];
  safe_copy(low, sizeof(low), esc);
  if (low[0])
    low[0] = tolower((unsigned char)low[0]);
  snprintf(out, cap,
           "var __t=\"%s\";var __l=\"%s\";var "
           "__ws=workspace.stackingOrder;for(var i=0;i<__ws.length;i++){var "
           "__c=__ws[i].caption;if(__c==__t||__c.toLowerCase()==__l){workspace."
           "activeWindow=__ws[i];break;}}",
           esc, low);
  return true;
}

bool focus_window_title_kwin(const char *title) {
  if (!title) {
    log_error("title is NULL");
    return false;
  }
  if (!kwin_is_active())
    return false;
  title = wm_trim_focus_title(title);
  if (!*title) {
    log_error("title empty after trim");
    return false;
  }
  char body[2300];
  if (!kwin_focus_js_cmd(title, body, sizeof(body)))
    return false;
  return kwin_run_script(body);
}

static PlWindowList kwin_list_windows(bool want_names) {
  PlWindowList list = {0};
  if (!kwin_is_active())
    return list;
  pthread_mutex_lock(&kwin_mtx);
  if (!kwin_snapshot_locked() || !kwin_records) {
    pthread_mutex_unlock(&kwin_mtx);
    return list;
  }
  const char *p = kwin_records;
  while (*p) {
    const char *rend = strchr(p, '\x1f');
    size_t rlen = rend ? (size_t)(rend - p) : strlen(p);
    const char *sep = memchr(p, '\x1e', rlen);
    size_t clen = sep ? (size_t)(sep - p) : rlen;
    char cap[1024];
    kwin_clean_copy(p, clen, cap, sizeof(cap));
    if (!kwin_skip_caption(cap)) {
      const char *item = cap;
      char buf[1024];
      if (want_names) {
        if (sep)
          kwin_clean_copy(sep + 1, rlen - clen - 1, buf, sizeof(buf));
        else
          buf[0] = '\0';
        if (buf[0])
          buf[0] = toupper((unsigned char)buf[0]);
        item = buf;
      }
      if (!list_append(&list, item))
        break;
    }
    p = rend ? rend + 1 : p + rlen;
  }
  pthread_mutex_unlock(&kwin_mtx);
  return list;
}

PlWindowList get_window_titles_kwin(void) { return kwin_list_windows(false); }

PlWindowList get_window_names_kwin(void) { return kwin_list_windows(true); }

bool close_active_window_kwin(void) {
  if (!kwin_is_active())
    return false;
  return kwin_run_script("workspace.slotWindowClose();");
}

bool toggle_fullscreen_active_kwin(void) {
  if (!kwin_is_active())
    return false;
  return kwin_run_script("workspace.slotWindowFullScreen();");
}

bool move_active_window_to_monitor_kwin(bool right) {
  if (!kwin_is_active())
    return false;
  if (right) {
    return kwin_run_script("workspace.slotWindowToRightScreen();");
  }
  return kwin_run_script("workspace.slotWindowToLeftScreen();");
}

bool move_active_window_left_kwin(void) {
  if (!kwin_is_active())
    return false;
  return kwin_run_script("workspace.slotWindowToLeftScreen();");
}

bool move_active_window_right_kwin(void) {
  if (!kwin_is_active())
    return false;
  return kwin_run_script("workspace.slotWindowToRightScreen();");
}

bool focus_window_left_kwin(void) {
  if (!kwin_is_active())
    return false;
  return kwin_run_script("workspace.slotSwitchWindowLeft();");
}

bool focus_window_right_kwin(void) {
  if (!kwin_is_active())
    return false;
  return kwin_run_script("workspace.slotSwitchWindowRight();");
}

bool focus_window_up_kwin(void) { return false; }
bool focus_window_down_kwin(void) { return false; }
bool move_active_window_up_kwin(void) { return false; }
bool move_active_window_down_kwin(void) { return false; }

bool inject_key_kwin(int vk, int down) {
  (void)vk;
  (void)down;
  return false;
}

bool minimize_active_window_kwin(void) {
  if (!kwin_is_active())
    return false;
  return kwin_run_script("workspace.slotWindowMinimize();");
}

bool get_mouse_pos_kwin(int *x, int *y) {
  (void)x;
  (void)y;
  return false;
}

bool toggle_maximize_active_kwin(void) {
  if (!kwin_is_active())
    return false;
  return kwin_run_script("workspace.slotWindowMaximize();");
}

bool toggle_always_on_top_active_kwin(void) {
  if (!kwin_is_active())
    return false;
  return kwin_run_script("workspace.slotWindowAbove();");
}

bool raise_active_window_kwin(void) {
  if (!kwin_is_active())
    return false;
  return kwin_run_script("workspace.slotWindowRaise();");
}

bool lower_active_window_kwin(void) {
  if (!kwin_is_active())
    return false;
  return kwin_run_script("workspace.slotWindowLower();");
}

bool center_active_window_kwin(void) {
  if (!kwin_is_active())
    return false;
  return kwin_run_script("workspace.slotWindowCenter();");
}

bool snap_active_window_kwin(SnapPreset preset) {
  if (!kwin_is_active())
    return false;
  static const char *exprs[] = {
    "x: g.x, y: g.y, width: Math.round(g.width / 2), height: g.height",
    "x: g.x + Math.round(g.width / 2), y: g.y, width: g.width - Math.round(g.width / 2), height: g.height",
    "x: g.x, y: g.y, width: g.width, height: Math.round(g.height / 2)",
    "x: g.x, y: g.y + Math.round(g.height / 2), width: g.width, height: g.height - Math.round(g.height / 2)",
    "x: g.x, y: g.y, width: Math.round(g.width / 2), height: Math.round(g.height / 2)",
    "x: g.x + Math.round(g.width / 2), y: g.y, width: g.width - Math.round(g.width / 2), height: Math.round(g.height / 2)",
    "x: g.x, y: g.y + Math.round(g.height / 2), width: Math.round(g.width / 2), height: g.height - Math.round(g.height / 2)",
    "x: g.x + Math.round(g.width / 2), y: g.y + Math.round(g.height / 2), width: g.width - Math.round(g.width / 2), height: g.height - Math.round(g.height / 2)",
  };
  if (preset < 0 || preset > SNAP_BOTTOM_RIGHT)
    return false;
  char body[2048];
  snprintf(body, sizeof(body),
           "var a = workspace.activeWindow;\n"
           "if (!a) { throw 'skald: no active window'; }\n"
           "var g = a.output ? a.output.geometry : null;\n"
           "if (!g) { throw 'skald: no output geometry'; }\n"
           "a.frameGeometry = { %s };\n",
           exprs[preset]);
  return kwin_run_script(body);
}

long get_pid_kwin(void) {
  log_error("kwin pid not implemented (frameGeometry/pid binding untested)");
  return 0;
}
