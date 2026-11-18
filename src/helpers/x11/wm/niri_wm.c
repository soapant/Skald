#include "helpers/helpers_text.h"
#include "wm/niri_wm.h"
#include "wm/wm.h"

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <glob.h>
#include <time.h>
#include <sys/stat.h>

#include "deps/log.h"
#include "platform/platform.h"
#include "helpers/x11/env_helpers_x11.h"

static bool niri_sock_live(const char *sock) {

  if (!sock || !sock[0])
    return false;
  struct stat st;
  return stat(sock, &st) == 0;
}

bool niri_is_active(void) {
  const char *cur = getenv("NIRI_SOCKET");
  if (niri_sock_live(cur))
    return true;
  if (cur && cur[0])
    unsetenv("NIRI_SOCKET");
  const char *rtd = x11_env_xdg_runtime_dir();
  if (!rtd || !rtd[0])
    return false;
  char pat[512];
  snprintf(pat, sizeof(pat), "%s/niri*.sock", rtd);
  glob_t g;
  memset(&g, 0, sizeof(g));
  if (glob(pat, 0, NULL, &g) != 0 || g.gl_pathc == 0) {
    globfree(&g);
    return false;
  }
  const char *best = NULL;
  time_t best_mtime = 0;
  for (size_t i = 0; i < g.gl_pathc; i++) {
    struct stat st;
    if (stat(g.gl_pathv[i], &st) != 0)
      continue;
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

static bool niri_succeded(char *output) {

  if (!output) {
    log_error("output is NULL");
    return false;
  }
  if (string_contains_ci(output, "error")) {
    log_error("niri output='%s'", output);
    return false;
  }
  return true;
}

static bool niri_exec(const char *cmd) {
  if (!niri_is_active())
    return false;
  char *out = Platform_RunTerminalCommand(cmd, false);
  bool ok = niri_succeded(out);
  free(out);
  return ok;
}

static size_t niri_copy_json_string(const char *s, char *out, size_t cap) {

  size_t o = 0;
  for (const char *p = s; *p && o + 1 < cap; p++) {
    if (*p == '\\' && *(p + 1)) {
      p++;
      out[o++] = *p;
      continue;
    }
    if (*p == '"')
      break;
    out[o++] = *p;
  }
  out[o] = '\0';
  return o;
}

static bool niri_focused_field(const char *key, size_t key_len, char *buffer,
                               int buffer_size) {
  if (!buffer || buffer_size <= 0)
    return false;
  buffer[0] = '\0';
  if (!niri_is_active())
    return false;
  char *json =
      Platform_RunTerminalCommand("niri msg --json focused-window", false);
  if (!json || !json[0]) {
    free(json);
    return false;
  }
  const char *tp = strstr(json, key);
  bool ok = false;
  if (tp) {
    niri_copy_json_string(tp + key_len, buffer, (size_t)buffer_size);
    ok = buffer[0] != '\0';
  }
  free(json);
  return ok;
}

bool get_title_niri(char *buffer, int buffer_size) {
  return niri_focused_field("\"title\":\"", 9, buffer, buffer_size);
}

bool get_id_niri(char *buffer, int buffer_size) {
  return niri_focused_field("\"app_id\":\"", 10, buffer, buffer_size);
}

char *paste_niri(const char *content) {
  if (!niri_is_active())
    return NULL;
  return x11_paste_wtype(content);
}

bool focus_window_title_niri(const char *title) {
  if (!title || !*title)
    return false;
  if (!niri_is_active())
    return false;
  title = wm_trim_focus_title(title);
  if (!*title)
    return false;
  char *json = Platform_RunTerminalCommand("niri msg --json windows", false);
  if (!json || !json[0]) {
    free(json);
    return false;
  }
  int target = -1;
  const char *p = json;
  while (1) {
    const char *idpos = strstr(p, "\"id\":");
    if (!idpos)
      break;
    int id = atoi(idpos + 5);
    const char *tpos = strstr(idpos, "\"title\":\"");
    const char *nextid = strstr(idpos + 5, "\"id\":");
    if (!tpos || (nextid && tpos > nextid)) {
      p = idpos + 5;
      continue;
    }
    char name[1024];
    niri_copy_json_string(tpos + 9, name, sizeof(name));
    if (name[0] && (!strcmp(name, title) || !strcasecmp(name, title))) {
      target = id;
      break;
    }
    p = tpos + 9;
  }
  free(json);
  if (target < 0)
    return false;
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "niri msg action focus-window --id %d", target);
  return niri_exec(cmd);
}

static WindowList niri_list_fields(const char *key, size_t key_len,
                                   bool toupper_first) {
  WindowList list = {0};
  if (!niri_is_active())
    return list;
  char *json = Platform_RunTerminalCommand("niri msg --json windows", false);
  if (!json || !json[0]) {
    free(json);
    return list;
  }
  const char *p = json;
  while (1) {
    const char *tpos = strstr(p, key);
    if (!tpos)
      break;
    char buf[1024];
    niri_copy_json_string(tpos + key_len, buf, sizeof(buf));
    p = tpos + key_len;
    if (!buf[0])
      continue;
    if (!wm_append_window_name(&list, buf, toupper_first))
      break;
  }
  free(json);
  return list;
}

WindowList get_window_titles_niri(void) {
  return niri_list_fields("\"title\":\"", 9, false);
}

WindowList get_window_ids_niri(void) {
  return niri_list_fields("\"app_id\":\"", 10, true);
}

bool close_active_window_niri(void) {
  return niri_exec("niri msg action close-window");
}

bool toggle_fullscreen_active_niri(void) {
  return niri_exec("niri msg action fullscreen-window");
}

bool move_active_window_to_monitor_niri(bool right) {
  if (!niri_is_active())
    return false;
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "niri msg action move-window-to-monitor-%s",
           right ? "right" : "left");
  return niri_exec(cmd);
}

bool move_active_window_left_niri(void) {
  return niri_exec("niri msg action move-window-to-monitor-left");
}

bool move_active_window_right_niri(void) {
  return niri_exec("niri msg action move-window-to-monitor-right");
}

bool focus_window_left_niri(void) {
  return niri_exec("niri msg action focus-column-or-monitor-left");
}

bool focus_window_right_niri(void) {
  return niri_exec("niri msg action focus-column-or-monitor-right");
}

bool focus_window_up_niri(void) { return false; }
bool focus_window_down_niri(void) { return false; }
bool move_active_window_up_niri(void) { return false; }
bool move_active_window_down_niri(void) { return false; }

bool inject_key_niri(int vk, int down) {
  (void)vk;
  (void)down;
  return false;
}

bool minimize_active_window_niri(void) {
  if (!niri_is_active())
    return false;
  return false;
}

bool get_mouse_pos_niri(int *x, int *y) {
  (void)x;
  (void)y;
  return false;
}

bool toggle_maximize_active_niri(void) {
  if (!niri_is_active())
    return false;
  return niri_exec("niri msg action maximize-window-to-edges");
}

bool toggle_always_on_top_active_niri(void) {
  log_error("niri has no always-on-top concept");
  return false;
}

bool raise_active_window_niri(void) {
  log_error("niri has no z-order raise command");
  return false;
}

bool lower_active_window_niri(void) {
  log_error("niri has no z-order lower command");
  return false;
}

bool center_active_window_niri(void) {
  if (!niri_is_active())
    return false;
  return niri_exec("niri msg action center-window");
}

bool snap_active_window_niri(SnapPreset preset) {
  (void)preset;
  log_error("niri has no absolute window positioning (scrollable layout)");
  return false;
}

long get_pid_niri(void) {
  if (!niri_is_active())
    return 0;
  char *json =
      Platform_RunTerminalCommand("niri msg --json focused-window", false);
  if (!json)
    return 0;
  const char *p = strstr(json, "\"pid\":");
  long pid = 0;
  if (p)
    pid = strtol(p + 6, NULL, 10);
  free(json);
  return pid;
}

bool switch_to_workspace_niri(int index) {
  if (index < 1)
    return false;
  if (!niri_is_active())
    return false;
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "niri msg action focus-workspace %d", index);
  return niri_exec(cmd);
}

bool move_active_window_to_workspace_niri(int index) {
  if (index < 1)
    return false;
  if (!niri_is_active())
    return false;
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "niri msg action move-window-to-workspace %d",
           index);
  return niri_exec(cmd);
}

bool set_active_window_opacity_niri(int percent) {
  (void)percent;
  log_error(
      "niri has no absolute opacity setter (toggle-window-rule-opacity only)");
  return false;
}

static bool niri_windows_field(const char *obj, const char *key, char *out,
                               size_t cap) {
  const char *p = strstr(obj, key);
  out[0] = '\0';
  if (!p)
    return false;
  p += strlen(key);
  while (*p == ' ' || *p == ':' || *p == '"')
    p++;
  const char *e = p;
  while (*e && *e != '"' && *e != ',' && *e != '}' &&
         !isspace((unsigned char)*e))
    e++;
  size_t m = (size_t)(e - p);
  if (m == 0)
    return false;
  if (m >= cap)
    m = cap - 1;
  memcpy(out, p, m);
  out[m] = '\0';
  return true;
}

static bool niri_close_windows(const char *needle, bool by_id) {
  if (!needle || !needle[0])
    return false;
  if (!niri_is_active())
    return false;
  char *json = Platform_RunTerminalCommand("niri msg --json windows", false);
  if (!json || !json[0]) {
    free(json);
    return false;
  }
  int closed = 0;
  for (char *p = json; (p = strstr(p, "\"id\":")); p += 5) {
    char *next = strstr(p + 5, "\"id\":");
    char *limit = next ? next : json + strlen(json);
    /* bound the scan to this window object */
    size_t save_len = (size_t)(limit - p);
    char *obj = malloc(save_len + 1);
    if (!obj)
      continue;
    memcpy(obj, p, save_len);
    obj[save_len] = '\0';
    long id = strtol(p + 5, NULL, 10);
    if (id == 0) {
      free(obj);
      continue;
    }
    char title[1024] = {0}, app_id[1024] = {0};
    niri_windows_field(obj, "\"title\":", title, sizeof(title));
    niri_windows_field(obj, "\"app_id\":", app_id, sizeof(app_id));
    if (strncmp(app_id, "null", 4) == 0)
      app_id[0] = '\0';
    if (strncmp(title, "null", 4) == 0)
      title[0] = '\0';
    const char *v = by_id ? app_id : title;
    bool match = v[0] != '\0' && string_contains_ci(v, needle);
    free(obj);
    if (!match)
      continue;
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "niri msg action close-window --id %ld", id);
    if (niri_exec(cmd))
      closed++;
  }
  free(json);
  return closed > 0;
}

bool close_windows_matching_title_niri(const char *needle) {
  return niri_close_windows(needle, false);
}

bool close_windows_matching_id_niri(const char *needle) {
  return niri_close_windows(needle, true);
}
