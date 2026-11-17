#include "wm/sway_wm.h"
#include "wm/wm.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <glob.h>
#include <strings.h>
#include <sys/stat.h>

#include "deps/log.h"
#include "platform/platform.h"
#include "helpers/x11/env_helpers_x11.h"
#include "helpers/helpers_text.h"

static bool sway_sock_live(const char *path) {

  struct stat st;
  return path && path[0] && stat(path, &st) == 0;
}

static void sway_ensure_sock(void) {

  const char *cur = getenv("SWAYSOCK");
  if (!cur || !cur[0])
    cur = getenv("I3SOCK");
  if (sway_sock_live(cur)) {
    return;
  }
  if (cur)
    unsetenv(cur == getenv("SWAYSOCK") ? "SWAYSOCK" : "I3SOCK");
  const char *rtd = x11_env_xdg_runtime_dir();
  if (!rtd || !rtd[0]) {
    return;
  }
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

static bool sway_bus(void) {
  sway_ensure_sock();
  const char *sock = getenv("SWAYSOCK");
  if (!sock || !sock[0])
    sock = getenv("I3SOCK");
  return sway_sock_live(sock);
}

static bool sway_tree_field(char *buffer, int buffer_size, const char *key) {
  if (!buffer || buffer_size <= 0) {
    log_error("buffer='%s' buffer_size=%d", buffer ? buffer : "null",
              buffer_size);
    return false;
  }
  buffer[0] = '\0';
  sway_ensure_sock();
  if (!getenv("SWAYSOCK") && !getenv("I3SOCK")) {
    log_error("SWAYSOCK and I3SOCK not set buffer='%s' buffer_size=%d",
              buffer ? buffer : "null", buffer_size);
    return false;
  }
  char *command_output = Platform_RunTerminalCommand(
      "swaymsg -t get_tree --raw | grep -A 40 '\"focused\": true'", false);
  if (!command_output) {
    log_error("command_output is NULL buffer='%s' buffer_size=%d",
              buffer ? buffer : "null", buffer_size);
    return false;
  }
  if (command_output[0] == '\0') {
    log_error("command_output empty buffer='%s' buffer_size=%d",
              buffer ? buffer : "null", buffer_size);
    free(command_output);
    return false;
  }
  StringList lines = String_Split(command_output, "\n");
  bool ok = false;
  for (size_t i = 0; i < lines.count; i++) {
    if (!lines.items[i])
      continue;
    const char *p = strstr(lines.items[i], key);
    if (!p)
      continue;
    p = strchr(p, ':');
    if (!p)
      continue;
    p++;
    while (*p && (isspace((unsigned char)*p) || *p == '"'))
      p++;
    size_t n = 0;
    while (p[n] && p[n] != '"' && p[n] != ',' && n < (size_t)(buffer_size - 1))
      n++;
    if (n == 0 || (n == 4 && strncmp(p, "null", 4) == 0))
      continue;
    memcpy(buffer, p, n);
    buffer[n] = '\0';
    ok = true;
    break;
  }
  string_list_free(&lines);
  free(command_output);
  return ok;
}

bool get_title_sway(char *buffer, int buffer_size) {
  return sway_tree_field(buffer, buffer_size, "\"name\"");
}

bool get_id_sway(char *buffer, int buffer_size) {
  return sway_tree_field(buffer, buffer_size, "\"app_id\"");
}

char *paste_sway(const char *content) { return x11_paste_wtype(content); }
static bool succeded_sway(char *output) {
  if (!output) {
    log_error("output is NULL");
    return false;
  }
  StringList lines = String_Split(output, "\n");
  for (size_t i = 0; i < lines.count; i++) {
    if (string_contains_ci(lines.items[i], "\"error\"")) {
      log_error("output='%s'", output ? output : "null");
      return false;
    }
  }
  return true;
}

static bool sway_exec(const char *cmd) {
  if (!sway_bus())
    return false;
  char *out = Platform_RunTerminalCommand(cmd, false);
  bool ok = succeded_sway(out);
  free(out);
  return ok;
}

static bool sway_focus_by_con_id(int con_id) {

  char cmd[128];
  snprintf(cmd, sizeof(cmd), "swaymsg [con_id=%d] focus", con_id);
  char *out = Platform_RunTerminalCommand(cmd, false);
  bool ok = succeded_sway(out);
  free(out);
  return ok;
}
static bool sway_json_string(const char *at, char *out, size_t cap,
                             const char *limit);
static bool sway_name_for_pid(const char *json, const char *pid_pos,
                              const char *next_pid, char *out, size_t cap);

bool focus_window_title_sway(const char *title) {
  if (!title || !*title) {
    log_error("title is NULL");
    return false;
  }
  sway_ensure_sock();
  title = wm_trim_focus_title(title);
  if (!*title) {
    log_error("title empty after trim");
    return false;
  }

  char *json = Platform_RunTerminalCommand("swaymsg -t get_tree", false);
  if (json) {
    for (char *p = json; (p = strstr(p, "\"pid\":")); p += 6) {
      const char *next = strstr(p + 6, "\"pid\":");
      char name[512] = {0};
      if (!sway_name_for_pid(json, p, next, name, sizeof(name)))
        continue;
      char *a = name;
      while (*a && (iscntrl((unsigned char)*a) || isspace((unsigned char)*a)))
        a++;
      char *b = a + strlen(a);
      while (b > a &&
             (iscntrl((unsigned char)b[-1]) || isspace((unsigned char)b[-1])))
        *--b = '\0';
      if (!strcmp(a, title) || !strcasecmp(a, title)) {
        const char *search = p - 4000;
        if (search < json)
          search = json;
        const char *last_id = NULL;
        const char *cur = search;
        while ((cur = strstr(cur, "\"id\":")) && cur < p) {
          last_id = cur;
          cur += 5;
        }
        if (last_id) {
          int con_id = atoi(last_id + 5);
          if (con_id) {
            free(json);
            if (sway_focus_by_con_id(con_id))
              return true;
            return false;
          }
        }
      }
    }
    free(json);
  }

  char escaped[1024];
  const char *s = title;
  char *d = escaped;
  while (d - escaped < (int)sizeof(escaped) - 2) {
    if (*s == '"' || *s == '\\') {
      *d++ = '\\';
      *d++ = *s++;
    } else
      *d++ = *s++;
  }
  *d = '\0';
  char low[1024];
  safe_copy(low, sizeof(low), escaped);
  if (low[0])
    low[0] = tolower((unsigned char)low[0]);
  char up[1024];
  safe_copy(up, sizeof(up), escaped);
  if (up[0])
    up[0] = toupper((unsigned char)up[0]);
  const char *targets[3] = {escaped, NULL, NULL};
  if (low[0])
    targets[1] = low;
  if (up[0])
    targets[2] = up;
  for (int i = 0; i < 3; i++) {
    if (!targets[i] || !targets[i][0])
      continue;
    char criteria[1200];
    snprintf(criteria, sizeof(criteria), "[title=\"%s\"] focus", targets[i]);
    const char *argv[] = {"swaymsg", criteria, NULL};
    if (succeded_sway(Platform_RunArgvCapture(argv, NULL, NULL)))
      return true;
  }
  log_warn("sway focus: no title match");
  return false;
}

static bool sway_json_string(const char *at, char *out, size_t cap,
                             const char *limit) {

  if (!at || !out || cap == 0)
    return false;
  const char *s = strchr(at, '"');
  if (!s || (limit && s >= limit))
    return false;
  s++;
  const char *e = s;
  while (*e && *e != '"' && (!limit || e < limit)) {
    if (*e == '\\' && *(e + 1))
      e += 2;
    else
      e++;
  }
  if (!*e || (limit && e >= limit))
    return false;
  size_t len = e - s;
  if (len >= cap)
    len = cap - 1;
  size_t o = 0;
  for (size_t i = 0; i < len && o + 1 < cap; i++) {
    if (s[i] == '\\' && i + 1 < len) {
      if (s[i + 1] == '/' || s[i + 1] == '"') {
        out[o++] = s[i + 1];
        i++;
      } else
        out[o++] = s[i];
    } else
      out[o++] = s[i];
  }
  out[o] = '\0';
  return o > 0;
}

static bool sway_name_for_pid(const char *json, const char *pid_pos,
                              const char *next_pid, char *out, size_t cap) {

  const char *search = pid_pos - 3000;
  if (search < json)
    search = json;
  const char *last = NULL;
  const char *cur = search;
  while ((cur = strstr(cur, "\"name\":")) && cur < pid_pos) {
    last = cur;
    cur += 7;
  }
  if (!last)
    return false;
  const char *colon = strchr(last, ':');
  if (!colon || (next_pid && colon > next_pid))
    return false;
  return sway_json_string(colon, out, cap, next_pid);
}

static bool sway_class_for_pid(const char *pid_pos, const char *next_pid,
                               char *out, size_t cap) {
  const char *cls = strstr(pid_pos, "\"class\":");
  if (!cls || (next_pid && cls >= next_pid))
    return false;
  const char *c2 = strchr(cls, ':');
  return c2 ? sway_json_string(c2, out, cap, next_pid) : false;
}

static bool sway_app_for_pid(const char *json, const char *pid_pos,
                             const char *next_pid, char *out, size_t cap) {
  (void)json;
  const char *app = strstr(pid_pos, "\"app_id\":");
  if (app && (!next_pid || app < next_pid)) {
    if (strncmp(app, "\"app_id\": \"", 11) == 0) {
      const char *colon = strchr(app, ':');
      return colon ? sway_json_string(colon, out, cap, next_pid) : false;
    }
    if (strncmp(app, "\"app_id\": null", 14) == 0)
      return sway_class_for_pid(pid_pos, next_pid, out, cap);
  }
  return sway_class_for_pid(pid_pos, next_pid, out, cap);
}

static WindowList sway_list_pid_fields(bool (*extract)(const char *json,
                                                         const char *pid_pos,
                                                         const char *next_pid,
                                                         char *out, size_t cap),
                                         bool toupper_first) {
  sway_ensure_sock();
  char *json = Platform_RunTerminalCommand("swaymsg -t get_tree", false);
  WindowList list = {0};
  if (!json || !*json) {
    log_error("output is NULL or empty");
    free(json);
    return list;
  }
  for (char *p = json; (p = strstr(p, "\"pid\":")); p += 6) {
    char *next = strstr(p + 6, "\"pid\":");
    char buf[512];
    if (!extract(json, p, next, buf, sizeof(buf)))
      continue;
    if (!buf[0] || !strcmp(buf, "null"))
      continue;
    if (!wm_append_window_name(&list, buf, toupper_first))
      break;
  }
  free(json);
  return list;
}

WindowList get_window_ids_sway(void) {
  return sway_list_pid_fields(sway_app_for_pid, true);
}

WindowList get_window_titles_sway(void) {
  return sway_list_pid_fields(sway_name_for_pid, false);
}

static int sway_json_depth(const char *base, const char *pos) {

  int depth = 0;
  int in_str = 0;
  int esc = 0;
  for (const char *s = base; s < pos; s++) {
    if (esc) {
      esc = 0;
      continue;
    }
    if (*s == '\\' && in_str) {
      esc = 1;
      continue;
    }
    if (*s == '"') {
      in_str = !in_str;
      continue;
    }
    if (in_str)
      continue;
    if (*s == '{' || *s == '[')
      depth++;
    else if (*s == '}' || *s == ']')
      depth--;
  }
  return depth;
}

static char *sway_find_key(const char *base, const char *from, const char *end,
                           const char *key, int want_depth) {

  size_t kl = strlen(key);
  char *q = (char *)from;
  while (q && (q = strstr(q, key)) != NULL && q < end) {
    if (sway_json_depth(base, q) == want_depth)
      return q;
    q += kl;
  }
  return NULL;
}

static int sway_key_bool3(const char *base, const char *from, const char *end,
                          const char *key, int depth) {

  char *k = sway_find_key(base, from, end, key, depth);
  if (!k)
    return -1;
  char *c = strchr(k + strlen(key), ':');
  if (!c || c >= end)
    return -1;
  char *v = c + 1;
  while (v < end && (*v == ' ' || *v == '\t'))
    v++;
  if (v + 4 <= end && strncmp(v, "true", 4) == 0)
    return 1;
  if (v + 5 <= end && strncmp(v, "false", 5) == 0)
    return 0;
  return -1;
}

static char *sway_find_colon(const char *base, const char *from,
                             const char *end, const char *key, int depth) {
  char *k = sway_find_key(base, from, end, key, depth);
  if (!k)
    return NULL;
  char *c = strchr(k + strlen(key), ':');
  if (!c || c >= end)
    return NULL;
  return c;
}

static int sway_key_int(const char *base, const char *from, const char *end,
                        const char *key, int depth) {

  const char *c = sway_find_colon(base, from, end, key, depth);
  return c ? atoi(c + 1) : 0;
}

static int sway_key_str(const char *base, const char *from, const char *end,
                        const char *key, int depth, char *out, size_t cap) {

  char *c = sway_find_colon(base, from, end, key, depth);
  if (!c)
    return 0;
  char *q = c + 1;
  while (q < end && (*q == ' ' || *q == '\t'))
    q++;
  if (q >= end || *q != '"')
    return 0;
  const char *e = strchr(q + 1, '"');
  if (!e || e >= end)
    return 0;
  size_t len = (size_t)(e - (q + 1));
  if (len == 0 || len >= cap)
    return 0;
  memcpy(out, q + 1, len);
  out[len] = '\0';
  return 1;
}

bool close_active_window_sway(void) { return sway_exec("swaymsg kill"); }

bool toggle_fullscreen_active_sway(void) {
  return sway_exec("swaymsg fullscreen toggle");
}

bool move_active_window_to_monitor_sway(bool right) {
  if (!sway_bus())
    return false;
  char *outs = Platform_RunTerminalCommand("swaymsg -t get_outputs", false);
  if (outs && outs[0]) {
    const char *doc_end = outs + strlen(outs);
    char names[16][64];
    int ox[16];
    int oy[16];
    int ow[16];
    int oh[16];
    int usable[16];
    int focused[16];
    int n = 0;
    const char *p = outs;
    while (n < 16) {
      char *np = sway_find_key(outs, p, doc_end, "\"name\"", 2);
      if (!np)
        break;
      char oname[64];
      if (!sway_key_str(outs, np, doc_end, "\"name\"", 2, oname,
                        sizeof(oname))) {
        p = np + 6;
        continue;
      }
      int rx = 0;
      int ry = 0;
      int rw = 0;
      int rh = 0;
      const char *rk = sway_find_key(outs, p, doc_end, "\"rect\"", 2);
      if (rk) {
        rx = sway_key_int(outs, rk, doc_end, "\"x\"", 3);
        ry = sway_key_int(outs, rk, doc_end, "\"y\"", 3);
        rw = sway_key_int(outs, rk, doc_end, "\"width\"", 3);
        rh = sway_key_int(outs, rk, doc_end, "\"height\"", 3);
      }
      int is_usable = 1;
      if (sway_key_bool3(outs, np, doc_end, "\"active\"", 2) == 0)
        is_usable = 0;
      if (sway_key_bool3(outs, np, doc_end, "\"dpms\"", 2) == 0)
        is_usable = 0;
      if (rw <= 0 || rh <= 0)
        is_usable = 0;
      memcpy(names[n], oname, sizeof(names[n]));
      ox[n] = rx;
      oy[n] = ry;
      ow[n] = rw;
      oh[n] = rh;
      usable[n] = is_usable;
      focused[n] =
          (sway_key_bool3(outs, np, doc_end, "\"focused\"", 2) == 1) ? 1 : 0;
      n++;
      p = np + 6;
    }
    if (n >= 2) {
      int cur = -1;
      for (int i = 0; i < n; i++) {
        if (focused[i] && usable[i]) {
          cur = i;
          break;
        }
      }
      if (cur < 0) {
        for (int i = 0; i < n; i++) {
          if (usable[i]) {
            cur = i;
            break;
          }
        }
      }
      int dst = -1;
      if (cur >= 0) {
        int best_overlap = -1;
        int best_gap = 1 << 30;
        for (int i = 0; i < n; i++) {
          if (i == cur || !usable[i])
            continue;
          if (right && ox[i] < ox[cur] + ow[cur])
            continue;
          if (!right && ox[i] + ow[i] > ox[cur])
            continue;
          int top = oy[cur] > oy[i] ? oy[cur] : oy[i];
          int bottom = oy[cur] + oh[cur] < oy[i] + oh[i] ? oy[cur] + oh[cur]
                                                         : oy[i] + oh[i];
          int overlap = bottom - top;
          if (overlap < 0)
            overlap = 0;
          int gap =
              right ? ox[i] - (ox[cur] + ow[cur]) : ox[cur] - (ox[i] + ow[i]);
          if (overlap > best_overlap ||
              (overlap == best_overlap && gap < best_gap)) {
            best_overlap = overlap;
            best_gap = gap;
            dst = i;
          }
        }
      }
      if (dst >= 0) {
        char cmd[192];
        snprintf(cmd, sizeof(cmd),
                 "swaymsg 'move window to output %s; focus output %s'",
                 names[dst], names[dst]);
        char *out = Platform_RunTerminalCommand(cmd, false);
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
  snprintf(cmd, sizeof(cmd), "swaymsg move window to output %s",
           right ? "right" : "left");
  char *out = Platform_RunTerminalCommand(cmd, false);
  bool ok = succeded_sway(out);
  free(out);
  return ok;
}

bool focus_window_up_sway(void) { return sway_exec("swaymsg focus up"); }

bool focus_window_down_sway(void) { return sway_exec("swaymsg focus down"); }

bool move_active_window_up_sway(void) { return sway_exec("swaymsg move up"); }

bool move_active_window_down_sway(void) {
  return sway_exec("swaymsg move down");
}

bool inject_key_sway(int vk, int down) {
  (void)vk;
  (void)down;
  return false;
}

bool minimize_active_window_sway(void) {
  if (!sway_bus())
    return false;
  char *out = Platform_RunTerminalCommand("swaymsg move scratchpad", false);
  bool ok = succeded_sway(out);
  free(out);
  if (ok)
    return true;
  out = Platform_RunTerminalCommand("swaymsg scratchpad show", false);
  ok = succeded_sway(out);
  free(out);
  if (ok) {
    out = Platform_RunTerminalCommand("swaymsg floating disable", false);
    free(out);
  }
  return ok;
}

bool focus_window_left_sway(void) { return sway_exec("swaymsg focus left"); }

bool focus_window_right_sway(void) { return sway_exec("swaymsg focus right"); }

bool move_active_window_left_sway(void) {
  return move_active_window_to_monitor_sway(false);
}

bool move_active_window_right_sway(void) {
  return move_active_window_to_monitor_sway(true);
}

bool get_mouse_pos_sway(int *x, int *y) {
  (void)x;
  (void)y;
  return false;
}

static bool sway_focused_output_geom(int *ox, int *oy, int *ow, int *oh) {
  char *out = Platform_RunTerminalCommand("swaymsg -t get_outputs", false);
  if (!out || !out[0]) {
    log_error("sway get_outputs failed or empty");
    free(out);
    return false;
  }
  const char *focus = strstr(out, "\"focused\": true");
  if (!focus) {
    log_error("sway get_outputs no focused output");
    free(out);
    return false;
  }
  size_t flen = strlen(out);
  size_t fpos = (size_t)(focus - out);
  size_t start = 0, end = 0;
  int depth = 0;
  bool got = false;
  for (size_t i = 0; i < flen; i++) {
    char c = out[i];
    if (c == '{') {
      if (depth == 0)
        start = i;
      depth++;
    } else if (c == '}') {
      depth--;
      if (depth == 0 && start <= fpos && fpos <= i) {
        end = i;
        got = true;
        break;
      }
    }
  }
  if (!got) {
    log_error("sway get_outputs focused object not found");
    free(out);
    return false;
  }
  size_t n = end - start + 1;
  char *block = malloc(n + 1);
  if (!block) {
    free(out);
    return false;
  }
  memcpy(block, out + start, n);
  block[n] = '\0';
  free(out);
  const char *rect = strstr(block, "\"rect\":");
  long x = 0, y = 0, w = 0, h = 0;
  bool ok = rect && wm_json_num_at(rect, "\"x\"", &x) &&
            wm_json_num_at(rect, "\"y\"", &y) &&
            wm_json_num_at(rect, "\"width\"", &w) &&
            wm_json_num_at(rect, "\"height\"", &h);
  free(block);
  if (!ok) {
    log_error("sway get_outputs no rect for focused output");
    return false;
  }
  *ox = (int)x;
  *oy = (int)y;
  *ow = (int)w;
  *oh = (int)h;
  return true;
}

bool toggle_maximize_active_sway(void) {
  log_error("sway maximize is tiling; not supported (use fullscreen)");
  return false;
}

bool toggle_always_on_top_active_sway(void) {
  return sway_exec("swaymsg sticky toggle");
}

bool raise_active_window_sway(void) {
  log_error("sway has no z-order raise command");
  return false;
}

bool lower_active_window_sway(void) {
  log_error("sway has no z-order lower command");
  return false;
}

bool center_active_window_sway(void) {
  return sway_exec("swaymsg move position center");
}

bool snap_active_window_sway(SnapPreset preset) {
  int mx = 0, my = 0, mw = 0, mh = 0;
  if (!sway_focused_output_geom(&mx, &my, &mw, &mh))
    return false;
  SnapRect r = wm_snap_rect(mx, my, mw, mh, preset);
  int w = r.w, h = r.h, x = r.x, y = r.y;
  char cmd[512];
  snprintf(cmd, sizeof(cmd),
           "swaymsg resize set width %d px height %d px && "
           "swaymsg move position %d px %d px",
           w, h, x, y);
  return sway_exec(cmd);
}

long get_pid_sway(void) {
  char buf[64];
  if (!sway_tree_field(buf, sizeof(buf), "\"pid\""))
    return 0;
  return strtol(buf, NULL, 10);
}

static void sway_json_extract_field(const char *json, const char *at,
                                    const char *limit, const char *key,
                                    char *out, size_t cap) {
  size_t n = (size_t)(limit - at);
  const char *end = at + n;
  const char *p = strstr(at, key);
  out[0] = '\0';
  if (!p || p >= end)
    return;
  p += strlen(key);
  while (p < end && (*p == ' ' || *p == ':'))
    p++;
  if (*p == '"') {
    p++;
    const char *e = p;
    while (e < end && *e && *e != '"')
      e++;
    size_t m = (size_t)(e - p);
    if (m >= cap)
      m = cap - 1;
    memcpy(out, p, m);
    out[m] = '\0';
    return;
  }
  /* null / number */
  const char *e = p;
  while (e < end && *e &&
         (*e != ',' && *e != '}' && *e != ']' && !iscntrl((unsigned char)*e)))
    e++;
  {
    size_t m = (size_t)(e - p);
    if (m >= cap)
      m = cap - 1;
    memcpy(out, p, m);
    out[m] = '\0';
  }
  char *t = out + strlen(out);
  while (t > out && isspace((unsigned char)t[-1]))
    *--t = '\0';
}

static bool sway_foreach_window(const char *match, bool by_id, int *closed) {
  if (match == NULL || match[0] == '\0')
    return false;
  char *json = Platform_RunTerminalCommand("swaymsg -t get_tree --raw", false);
  if (!json || !json[0]) {
    free(json);
    return false;
  }
  int count = 0;
  if (closed)
    *closed = 0;
  for (char *p = json; (p = strstr(p, "\"id\":")); p += 5) {
    char *next = strstr(p + 5, "\"id\":");
    const char *limit = next ? next : json + strlen(json);
    long con_id = strtol(p + 6, NULL, 10);
    if (con_id == 0)
      continue;
    char name[1024] = {0}, app_id[1024] = {0};
    sway_json_extract_field(json, p, limit, "\"name\":", name, sizeof(name));
    sway_json_extract_field(json, p, limit, "\"app_id\":", app_id,
                            sizeof(app_id));
    if (strncmp(name, "null", 4) == 0)
      name[0] = '\0';
    if (strncmp(app_id, "null", 4) == 0)
      app_id[0] = '\0';
    const char *v = by_id ? app_id : name;
    if (!v || v[0] == '\0' || !string_contains_ci(v, match))
      continue;
    char crit[256];
    snprintf(crit, sizeof(crit), "swaymsg '[con_id=%ld] kill'", con_id);
    if (sway_exec(crit))
      count++;
  }
  free(json);
  if (closed)
    *closed = count;
  return count > 0;
}

bool close_windows_matching_title_sway(const char *needle) {
  int n = 0;
  return sway_foreach_window(needle, false, &n);
}

bool close_windows_matching_id_sway(const char *needle) {
  int n = 0;
  return sway_foreach_window(needle, true, &n);
}

bool switch_to_workspace_sway(int index) {
  if (index < 1)
    return false;
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "swaymsg workspace %d", index);
  return sway_exec(cmd);
}

bool move_active_window_to_workspace_sway(int index) {
  if (index < 1)
    return false;
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "swaymsg move container to workspace %d", index);
  return sway_exec(cmd);
}

bool set_active_window_opacity_sway(int percent) {
  if (percent < 1)
    percent = 1;
  if (percent > 100)
    percent = 100;
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "swaymsg opacity %.2f", (double)percent / 100.0);
  return sway_exec(cmd);
}
