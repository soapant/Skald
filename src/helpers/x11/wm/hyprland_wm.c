#include "wm/hyprland_wm.h"
#include "wm/wm.h"

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <glob.h>
#include <strings.h>
#include <time.h>
#include <sys/stat.h>

#include "deps/log.h"
#include "platform/platform.h"
#include "helpers/x11/env_helpers_x11.h"
#include "helpers/helpers_text.h"

static bool hypr_numeric_activewindow_field(const char *field, char *buffer,
                                            int buffer_size);

static bool hypr_sock_live(const char *sig) {

  if (!sig || !sig[0])
    return false;
  const char *rtd = x11_env_xdg_runtime_dir();
  if (!rtd || !rtd[0])
    return false;
  char path[512];
  snprintf(path, sizeof(path), "%s/hypr/%s/.socket.sock", rtd, sig);
  struct stat st;
  return stat(path, &st) == 0;
}

static void hypr_ensure_sig(void) {

  const char *cur = getenv("HYPRLAND_INSTANCE_SIGNATURE");
  if (hypr_sock_live(cur)) {
    return;
  }
  if (cur && cur[0])
    unsetenv("HYPRLAND_INSTANCE_SIGNATURE");
  const char *rtd = x11_env_xdg_runtime_dir();
  if (!rtd || !rtd[0]) {
    log_error("hypr discovery no XDG_RUNTIME_DIR");
    return;
  }
  char pat[512];
  snprintf(pat, sizeof(pat), "%s/hypr/*/.socket.sock", rtd);
  glob_t g;
  memset(&g, 0, sizeof(g));
  if (glob(pat, 0, NULL, &g) != 0 || g.gl_pathc == 0) {
    log_error("hypr discovery no match pat='%s'", pat);
    globfree(&g);
    return;
  }
  const char *best = NULL;
  const char *best_p = NULL;
  size_t best_n = 0;
  time_t best_mtime = 0;
  for (size_t i = 0; i < g.gl_pathc; i++) {
    const char *cand = g.gl_pathv[i];
    const char *end = strrchr(cand, '/');
    if (!end || end <= cand)
      continue;
    const char *p = end - 1;
    while (p > cand && *(p - 1) != '/')
      p--;
    size_t n = (size_t)(end - p);
    if (n == 0 || n >= 128)
      continue;
    struct stat st;
    if (stat(cand, &st) != 0)
      continue;
    if (!best || st.st_mtime >= best_mtime) {
      best = cand;
      best_p = p;
      best_n = n;
      best_mtime = st.st_mtime;
    }
  }
  if (best && best_p) {
    char signame[128];
    memcpy(signame, best_p, best_n);
    signame[best_n] = '\0';
    setenv("HYPRLAND_INSTANCE_SIGNATURE", signame, 1);
    log_error("hypr discovery sig='%s' sock='%s'", signame, best);
  } else {
    log_error("hypr discovery no live sock pat='%s' count=%d", pat,
              (int)g.gl_pathc);
  }
  globfree(&g);
}
static bool hypr_parse_activewindow_field(const char *field, char *buffer,
                                          int buffer_size) {
  if (!field || !buffer || buffer_size <= 0)
    return false;
  buffer[0] = '\0';
  hypr_ensure_sig();
  char *output = Platform_RunTerminalCommand("hyprctl activewindow -j", false);
  if (!output || output[0] == '\0') {
    log_error("hypr activewindow empty for field='%s'", field);
    free(output);
    return false;
  }
  StringList lines = String_Split(output, "\n");
  bool ok = false;
  char needle[64];
  snprintf(needle, sizeof(needle), "\"%s\"", field);
  for (size_t i = 0; i < lines.count && !ok; i++) {
    if (!lines.items[i])
      continue;
    const char *match = strstr(lines.items[i], needle);
    if (!match)
      continue;
    char tmp[1024];
    if (sscanf(match, "\"%63[^\"]\": \"%1023[^\"]\"", tmp, buffer) != 2) {
      if (sscanf(match, "\"%63[^\"]\":\"%1023[^\"]\"", tmp, buffer) != 2)
        continue;
    }
    if (buffer[0] == '\0')
      continue;
    if (strcmp(buffer, "null") == 0)
      continue;
    buffer[buffer_size - 1] = '\0';
    ok = true;
  }
  if (!ok) {
    log_error("hypr activewindow no '%s' in output='%s'", field, output);
  } else {
    log_info("hypr activewindow %s='%s'", field, buffer);
  }
  string_list_free(&lines);
  free(output);
  return ok;
}

bool get_title_hyprland(char *buffer, int buffer_size) {
  return hypr_parse_activewindow_field("title", buffer, buffer_size);
}

bool get_id_hyprland(char *buffer, int buffer_size) {
  return hypr_parse_activewindow_field("class", buffer, buffer_size);
}

char *paste_hyprland(const char *content) { return x11_paste_wtype(content); }

static bool hypr_bus(void) {
  hypr_ensure_sig();
  const char *sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
  return sig && sig[0];
}

static bool succeded_hypr(char *output) {
  if (!output) {
    log_error("output is NULL");
    return false;
  }
  if (!output[0]) {
    log_error("hyprctl empty output");
    return false;
  }
  StringList lines = String_Split(output, "\n");
  for (size_t i = 0; i < lines.count; i++) {
    if (string_contains_ci(lines.items[i], "running")) {
      log_error("hyprctl output='%s'", output);
      return false;
    }
  }
  if (!string_contains_ci(output, "ok")) {
    log_error("hyprctl no ok output='%s'", output);
    return false;
  }
  return true;
}

static bool hypr_exec(const char *cmd) {
  if (!hypr_bus())
    return false;
  char *out = Platform_RunTerminalCommand(cmd, false);
  bool ok = succeded_hypr(out);
  free(out);
  return ok;
}

static bool hypr_focus_lua_cmd(const char *title, char *out, size_t cap) {

  char esc[1024];
  size_t o = 0;
  for (const char *s = title; *s && o + 4 < sizeof(esc); s++) {
    if (*s == '\\' || *s == '"') {
      esc[o++] = '\\';
      esc[o++] = *s;
    } else if (*s == '\'') {
      esc[o++] = '\'';
      esc[o++] = '\\';
      esc[o++] = '\'';
      esc[o++] = '\'';
    } else if (*s == '\n' || *s == '\r') {
      continue;
    } else {
      esc[o++] = *s;
    }
  }
  esc[o] = '\0';
  snprintf(out, cap,
           "hyprctl dispatch 'hl.dsp.focus({ window = \"title:%s\" })'", esc);
  return esc[0] != '\0';
}

bool focus_window_title_hyprland(const char *title) {
  if (!title) {
    log_error("title is NULL");
    return false;
  }
  hypr_ensure_sig();
  char cmd[1280];
  if (!hypr_focus_lua_cmd(title, cmd, sizeof(cmd))) {
    log_error("title='%s'", title ? title : "null");
    return false;
  }
  char *out = Platform_RunTerminalCommand(cmd, false);
  if (succeded_hypr(out)) {
    free(out);
    return true;
  }
  free(out);
  if (title[0] != '\0') {
    char alt[1024];
    safe_copy(alt, sizeof(alt), title);
    alt[0] = toupper((unsigned char)alt[0]);
    if (hypr_focus_lua_cmd(alt, cmd, sizeof(cmd))) {
      out = Platform_RunTerminalCommand(cmd, false);
      if (succeded_hypr(out)) {
        free(out);
        return true;
      }
      free(out);
    }
  }
  log_error("title='%s'", title ? title : "null");
  return false;
}

static WindowList hypr_client_list(const char *field, bool toupper_first) {
  hypr_ensure_sig();
  char *output = Platform_RunTerminalCommand("hyprctl clients -j", false);
  WindowList list = {.windows = NULL, .count = 0};
  char buffer[512];
  char match_pat[32];
  char fmt[64];
  if (output == NULL || strlen(output) == 0) {
    log_error("output is NULL or empty");
    return list;
  }
  snprintf(match_pat, sizeof(match_pat), "\"%s\"", field);
  snprintf(fmt, sizeof(fmt), "\"%s\": \"%%511[^\"]\"", field);
  StringList lines = String_Split(output, "\n");
  for (size_t i = 0; i < lines.count; i++) {
    const char *match = strstr(lines.items[i], match_pat);
    if (match) {
      if (sscanf(match, fmt, buffer) == 1) {
        if (strlen(buffer) == 0)
          continue;
        if (!strcasecmp(buffer, "window") || !strcasecmp(buffer, "colored"))
          continue;
        if (toupper_first)
          buffer[0] = toupper((unsigned char)buffer[0]);
        char **temp = realloc(list.windows, (list.count + 1) * sizeof(char *));
        if (temp == NULL)
          break;
        list.windows = temp;
        list.windows[list.count] = strdup(buffer);
        list.count++;
      }
    }
  }
  string_list_free(&lines);
  free(output);
  return list;
}
WindowList get_window_ids_hyprland(void) {
  return hypr_client_list("class", true);
}
WindowList get_window_titles_hyprland(void) {
  return hypr_client_list("title", false);
}

bool close_active_window_hyprland(void) {
  return hypr_exec("hyprctl dispatch 'hl.dsp.window.close()'");
}

bool toggle_fullscreen_active_hyprland(void) {
  return hypr_exec(
      "hyprctl dispatch 'hl.dsp.window.fullscreen({ mode = \"fullscreen\", "
      "action = \"toggle\" })'");
}

bool move_active_window_to_monitor_hyprland(bool right) {
  if (!hypr_bus())
    return false;
  char cmd[96];
  snprintf(cmd, sizeof(cmd),
           "hyprctl dispatch 'hl.dsp.window.move({ monitor = \"%c\" })'",
           right ? 'r' : 'l');
  char *out = Platform_RunTerminalCommand(cmd, false);
  bool ok = succeded_hypr(out);
  free(out);
  return ok;
}

bool focus_window_up_hyprland(void) {
  return hypr_exec("hyprctl dispatch movefocus u");
}

bool focus_window_down_hyprland(void) {
  return hypr_exec("hyprctl dispatch movefocus d");
}

bool move_active_window_up_hyprland(void) {
  return hypr_exec("hyprctl dispatch movewindow u");
}

bool move_active_window_down_hyprland(void) {
  return hypr_exec("hyprctl dispatch movewindow d");
}

bool inject_key_hyprland(int vk, int down) {
  if (!down)
    return true;
  hypr_ensure_sig();
  const char *sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
  const char *runtime = x11_env_xdg_runtime_dir();
  if (!sig || !sig[0] || !runtime || !runtime[0])
    return false;

  char keyname[32];
  const char *name = NULL;
  if (vk >= 'A' && vk <= 'Z') {
    keyname[0] = (char)(vk + 32);
    keyname[1] = '\0';
    name = keyname;
  } else if (vk >= '0' && vk <= '9') {
    keyname[0] = (char)vk;
    keyname[1] = '\0';
    name = keyname;
  } else {
    switch (vk) {
    case VK_RETURN:
      name = "Return";
      break;
    case VK_SPACE:
      name = "space";
      break;
    case VK_TAB:
      name = "Tab";
      break;
    case VK_ESCAPE:
      name = "Escape";
      break;
    case VK_UP:
      name = "Up";
      break;
    case VK_DOWN:
      name = "Down";
      break;
    case VK_LEFT:
      name = "Left";
      break;
    case VK_RIGHT:
      name = "Right";
      break;
    default:
      name = NULL;
    }
  }
  if (!name)
    return false;

  char path[512];
  snprintf(path, sizeof(path), "%s/hypr/%s/.socket.sock", runtime, sig);

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    return false;

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  safe_copy(addr.sun_path, sizeof(addr.sun_path), path);

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(fd);
    return false;
  }

  char cmd[256];
  snprintf(cmd, sizeof(cmd), "dispatch sendshortcut ,%s,active", name);

  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 100000;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  if (write(fd, cmd, strlen(cmd)) < 0) {
    close(fd);
    return false;
  }
  write(fd, "\n", 1);

  char buf[256];
  recv(fd, buf, sizeof(buf) - 1, 0);
  close(fd);
  return true;
}

bool minimize_active_window_hyprland(void) {
  if (!hypr_bus())
    return false;
  char *out =
      Platform_RunTerminalCommand("hyprctl dispatch 'hl.dsp.window.move({ "
                                  "workspace = \"special\", follow = false })'",
                                  false);
  bool ok = succeded_hypr(out);
  free(out);
  if (ok)
    return true;
  out = Platform_RunTerminalCommand(
      "hyprctl dispatch 'hl.dsp.workspace.toggle_special()'", false);
  ok = succeded_hypr(out);
  free(out);
  return ok;
}

bool focus_window_left_hyprland(void) {
  return hypr_exec("hyprctl dispatch 'hl.dsp.focus({ direction = \"l\" })'");
}

bool focus_window_right_hyprland(void) {
  return hypr_exec("hyprctl dispatch 'hl.dsp.focus({ direction = \"r\" })'");
}

bool move_active_window_left_hyprland(void) {
  return move_active_window_to_monitor_hyprland(false);
}

bool move_active_window_right_hyprland(void) {
  return move_active_window_to_monitor_hyprland(true);
}

bool get_mouse_pos_hyprland(int *x, int *y) {
  const char *rtd = x11_env_xdg_runtime_dir();
  const char *sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
  if (!rtd || !sig)
    return false;
  char path[512];
  snprintf(path, sizeof(path), "%s/hypr/%s/.socket.sock", rtd, sig);
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    return false;
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  if (strlen(path) >= sizeof(addr.sun_path)) {
    close(fd);
    return false;
  }
  memcpy(addr.sun_path, path, strlen(path) + 1);
  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    close(fd);
    return false;
  }
  if (send(fd, "cursorpos", 9, 0) < 0) {
    close(fd);
    return false;
  }
  shutdown(fd, SHUT_WR);
  char buf[64] = {0};
  int total = 0;
  for (;;) {
    int r = recv(fd, buf + total, sizeof(buf) - 1 - total, 0);
    if (r <= 0)
      break;
    total += r;
    if (total >= (int)sizeof(buf) - 1)
      break;
  }
  close(fd);
  buf[total] = '\0';
  int px = 0, py = 0;
  if (sscanf(buf, "%d, %d", &px, &py) == 2) {
    if (x)
      *x = px;
    if (y)
      *y = py;
    return true;
  }
  return false;
}

static bool hypr_focused_monitor_geom(int *out_x, int *out_y, int *out_w,
                                      int *out_h) {
  char *out = Platform_RunTerminalCommand("hyprctl monitors -j", false);
  if (!out || !out[0]) {
    log_error("hypr monitors failed or empty");
    free(out);
    return false;
  }
  const char *focus = strstr(out, "\"focused\": true");
  if (!focus) {
    log_error("hypr monitors no focused monitor");
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
    log_error("hypr monitors focused object not found");
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
  long x = 0, y = 0, w = 0, h = 0;
  bool ok = wm_json_num_at(block, "\"x\"", &x) &&
            wm_json_num_at(block, "\"y\"", &y) &&
            wm_json_num_at(block, "\"width\"", &w) &&
            wm_json_num_at(block, "\"height\"", &h);
  const char *res = strstr(block, "\"reserved\":");
  if (ok && res) {
    long rl = 0, rt = 0, rr = 0, rb = 0;
    const char *br = strchr(res, '[');
    if (br && sscanf(br, "[%ld,%ld,%ld,%ld]", &rl, &rt, &rr, &rb) == 4) {
      x += rl;
      y += rt;
      w -= rl + rr;
      h -= rt + rb;
    }
  }
  free(block);
  if (!ok) {
    log_error("hypr monitors no geometry for focused monitor");
    return false;
  }
  *out_x = (int)x;
  *out_y = (int)y;
  *out_w = (int)w;
  *out_h = (int)h;
  return true;
}

static bool hypr_numeric_activewindow_field(const char *field, char *buffer,
                                            int buffer_size) {
  if (!field || !buffer || buffer_size <= 0)
    return false;
  buffer[0] = '\0';
  hypr_ensure_sig();
  char *output = Platform_RunTerminalCommand("hyprctl activewindow -j", false);
  if (!output || output[0] == '\0') {
    log_error("hypr activewindow empty for field='%s'", field);
    free(output);
    return false;
  }
  char needle[64];
  snprintf(needle, sizeof(needle), "\"%s\":", field);
  const char *p = strstr(output, needle);
  bool ok = false;
  if (p) {
    p += strlen(needle);
    while (*p == ' ' || *p == ':')
      p++;
    char *end = NULL;
    long v = strtol(p, &end, 10);
    if (end != p) {
      snprintf(buffer, (size_t)buffer_size, "%ld", v);
      ok = buffer[0] != '\0';
    }
  }
  free(output);
  return ok;
}

bool toggle_maximize_active_hyprland(void) {
  if (!hypr_bus())
    return false;
  return hypr_exec(
      "hyprctl dispatch 'hl.dsp.window.fullscreen({ mode = \"maximized\", "
      "action = \"toggle\" })'");
}

bool toggle_always_on_top_active_hyprland(void) {
  if (!hypr_bus())
    return false;
  return hypr_exec("hyprctl dispatch 'hl.dsp.window.pin({ action = "
                   "\"toggle\" })'");
}

bool raise_active_window_hyprland(void) {
  if (!hypr_bus())
    return false;
  return hypr_exec(
      "hyprctl dispatch 'hl.dsp.window.alter_zorder({ mode = \"top\" })'");
}

bool lower_active_window_hyprland(void) {
  if (!hypr_bus())
    return false;
  return hypr_exec(
      "hyprctl dispatch 'hl.dsp.window.alter_zorder({ mode = \"bottom\" })'");
}

bool center_active_window_hyprland(void) {
  if (!hypr_bus())
    return false;
  return hypr_exec("hyprctl dispatch 'hl.dsp.window.center({})'");
}

bool snap_active_window_hyprland(SnapPreset preset) {
  if (!hypr_bus())
    return false;
  int mx = 0, my = 0, mw = 0, mh = 0;
  if (!hypr_focused_monitor_geom(&mx, &my, &mw, &mh))
    return false;
  int x = 0, y = 0, w = 0, h = 0;
  wm_snap_rect(mx, my, mw, mh, preset, &x, &y, &w, &h);
  char cmd[512];
  snprintf(cmd, sizeof(cmd),
           "hyprctl dispatch 'hl.dsp.window.resize({ x = %d, y = %d })' && "
           "hyprctl dispatch 'hl.dsp.window.move({ x = %d, y = %d })'",
           w, h, x, y);
  return hypr_exec(cmd);
}

long get_pid_hyprland(void) {
  if (!hypr_bus())
    return 0;
  char buf[64];
  if (!hypr_numeric_activewindow_field("pid", buf, sizeof(buf)))
    return 0;
  return strtol(buf, NULL, 10);
}

bool switch_to_workspace_hyprland(int index) {
  if (index < 1)
    return false;
  if (!hypr_bus())
    return false;
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "hyprctl dispatch workspace %d", index);
  char *out = Platform_RunTerminalCommand(cmd, false);
  bool ok = succeded_hypr(out);
  free(out);
  return ok;
}

bool move_active_window_to_workspace_hyprland(int index) {
  if (index < 1)
    return false;
  if (!hypr_bus())
    return false;
  char cmd[256];
  snprintf(cmd, sizeof(cmd),
           "hyprctl dispatch 'hl.dsp.window.move({ workspace = \"%d\" })'",
           index);
  return hypr_exec(cmd);
}

bool set_active_window_opacity_hyprland(int percent) {
  if (percent < 1)
    percent = 1;
  if (percent > 100)
    percent = 100;
  if (!hypr_bus())
    return false;
  char cmd[256];
  snprintf(cmd, sizeof(cmd),
           "hyprctl dispatch 'hl.dsp.window.set_prop({ prop = \"opacity\", "
           "value = \"%.2f\" })'",
           (double)percent / 100.0);
  return hypr_exec(cmd);
}

static void hypr_json_field(const char *json, const char *limit,
                            const char *key, char *out, size_t cap) {
  out[0] = '\0';
  const char *p = strstr(json, key);
  if (!p || p >= limit)
    return;
  p += strlen(key);
  while (p < limit && (*p == ' ' || *p == ':'))
    p++;
  if (*p != '"')
    return;
  p++;
  const char *e = p;
  while (e < limit && *e && *e != '"')
    e++;
  size_t m = (size_t)(e - p);
  if (m >= cap)
    m = cap - 1;
  memcpy(out, p, m);
  out[m] = '\0';
}

static bool hypr_close_windows(const char *needle, bool by_id) {
  if (!needle || !needle[0])
    return false;
  if (!hypr_bus())
    return false;
  char *out = Platform_RunTerminalCommand("hyprctl clients -j", false);
  if (!out || out[0] == '\0') {
    free(out);
    return false;
  }
  int closed = 0;
  for (char *p = out; (p = strstr(p, "\"address\":")); p += 10) {
    char *next = strstr(p + 10, "\"address\":");
    char *limit = next ? next : out + strlen(out);
    char address[64] = {0}, title[512] = {0}, clazz[512] = {0};
    hypr_json_field(p, limit, "\"address\":", address, sizeof(address));
    hypr_json_field(p, limit, "\"title\":", title, sizeof(title));
    hypr_json_field(p, limit, "\"class\":", clazz, sizeof(clazz));
    const char *v = by_id ? clazz : title;
    if (!v || v[0] == '\0' || !string_contains_ci(v, needle))
      continue;
    char cmd[256];
    snprintf(
        cmd, sizeof(cmd),
        "hyprctl dispatch 'hl.dsp.window.close({ window = \"address:%s\" })'",
        address);
    if (hypr_exec(cmd))
      closed++;
  }
  free(out);
  return closed > 0;
}

bool close_windows_matching_title_hyprland(const char *needle) {
  return hypr_close_windows(needle, false);
}

bool close_windows_matching_id_hyprland(const char *needle) {
  return hypr_close_windows(needle, true);
}
