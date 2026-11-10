#include <string.h>
#include "helpers/helpers_fs.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <fontconfig/fontconfig.h>
#include <stdbool.h>

#include "deps/log.h"
#include "platform/platform.h"
#include "helpers/helpers_text.h"
#include "platform/draw.h"
#include "helpers/helpers_ui.h"
#include "helpers/x11/env_helpers_x11.h"
#include "platform/x11/drawing_x11.h"
#include "helpers/helpers_ui.h"
#include "core/prompt_geometry.h"
#include "core/listbox_hover.h"
#include "helpers/helpers_ui.h"
#include "core/editbox.h"
#include "core/window_manager.h"
#include "platform/wm_platform.h"
#include "config_values.h"
#include "core/types.h"
#include "core/types/skapi_types.h"
#include "core/types/keys.h"
#include "core/types/window.h"
#include "core/thread_registry.h"
#include "helpers/helpers_owner.h"

static bool x11_desktop_parse(const char *desktop_path, char exec_line[1024],
                          char workdir[1024], bool *is_terminal) {

  if (exec_line)
    exec_line[0] = '\0';
  if (workdir)
    workdir[0] = '\0';
  if (is_terminal)
    *is_terminal = false;
  if (!desktop_path)
    return false;
  static char content[16384];
  if (File_ReadAll(desktop_path, content, (long)sizeof(content)) <= 0)
    return false;
  char *cur = content;
  while (*cur) {
    char *nl = strchr(cur, '\n');
    if (nl)
      *nl = '\0';
    char *line = cur;
    cur = nl ? nl + 1 : cur + strlen(cur);
    char *p = line;
    while (*p == ' ' || *p == '\t')
      p++;
    if (String_StartsWithCI(p, "Terminal=")) {
      char *v = p + 9;
      while (*v == ' ' || *v == '\t')
        v++;
      v[strcspn(v, "\r\n")] = '\0';
      if (is_terminal && (String_ContainsCI(v, "true") ||
                          strcmp(v, "1") == 0))
        *is_terminal = true;
    } else if (String_StartsWithCI(p, "Exec=")) {
      char *v = p + 5;
      while (*v == ' ' || *v == '\t')
        v++;
      v[strcspn(v, "\r\n")] = '\0';
      if (exec_line)
        strncpy(exec_line, v, 1024 - 1);
    } else if (String_StartsWithCI(p, "Path=")) {
      char *v = p + 5;
      while (*v == ' ' || *v == '\t')
        v++;
      v[strcspn(v, "\r\n")] = '\0';
      if (workdir)
        strncpy(workdir, v, 1024 - 1);
    }
  }
  return true;
}

static void x11_desktop_clean_exec(char exec_line[1024]) {
  if (!exec_line || !exec_line[0]) {
    return;
  }
  char cleaned[1024] = {0};
  size_t o = 0;
  for (size_t i = 0; exec_line[i] && o < sizeof(cleaned) - 1; i++) {
    if (exec_line[i] == '%' && exec_line[i + 1]) {
      char c = exec_line[i + 1];
      if (c == 'f' || c == 'F' || c == 'u' || c == 'U' || c == 'i' ||
          c == 'c' || c == 'k' || c == 'v' || c == 'm') {
        i++;
        while (exec_line[i + 1] == ' ')
          i++;
        continue;
      }
    }
    cleaned[o++] = exec_line[i];
  }
  cleaned[o] = '\0';
  char *s = cleaned;
  while (*s == ' ' || *s == '\t')
    s++;
  char *e = s + strlen(s) - 1;
  while (e > s && (*e == ' ' || *e == '\t'))
    *e-- = '\0';
  strncpy(exec_line, s, 1024 - 1);
}

static bool platform_exe_in_path(const char *name) {
  if (!name || !name[0])
    return false;
  const char *path = getenv("PATH");
  if (!path || !path[0])
    return false;
  const char *p = path;
  while (*p) {
    const char *sep = strchr(p, ':');
    size_t len = sep ? (size_t)(sep - p) : strlen(p);
    char cand[1024];
    if (len == 0)
      snprintf(cand, sizeof(cand), "./%s", name);
    else
      snprintf(cand, sizeof(cand), "%.*s/%s", (int)len, p, name);
    if (access(cand, X_OK) == 0)
      return true;
    if (!sep)
      break;
    p = sep + 1;
  }
  return false;
}

static bool platform_resolve_desktop_exec(const char *exec_line,
                                          const char *desktop_dir,
                                          const char *workdir, char *out,
                                          size_t out_size) {
  if (!exec_line || !out || out_size == 0)
    return false;
  const char *p = exec_line;
  while (*p == ' ' || *p == '\t')
    p++;

  char token[1024];
  size_t t = 0;
  if (*p == '"') {
    p++;
    while (*p) {
      if (*p == '\\' && (p[1] == '"' || p[1] == '\\'))
        p++;
      else if (*p == '"') {
        p++;
        break;
      }
      if (t + 1 < sizeof(token))
        token[t++] = *p;
      p++;
    }
  } else {
    while (*p && *p != ' ' && *p != '\t') {
      if (t + 1 < sizeof(token))
        token[t++] = *p;
      p++;
    }
  }
  token[t] = '\0';
  while (*p == ' ' || *p == '\t')
    p++;
  if (!token[0])
    return false;

  const char *base = (workdir && workdir[0]) ? workdir : desktop_dir;
  char prog[1024];
  if (strchr(token, '/')) {
    if (token[0] == '/')
      snprintf(prog, sizeof(prog), "%s", token);
    else
      snprintf(prog, sizeof(prog), "%s/%s", base, token);
  } else if (platform_exe_in_path(token)) {
    snprintf(prog, sizeof(prog), "%s", token);
  } else {
    snprintf(prog, sizeof(prog), "%s/%s", desktop_dir, token);
  }

  if (strchr(prog, '/')) {
    if (access(prog, X_OK) != 0) {
      log_info("desktop exec resolve: '%s' not executable", prog);
      return false;
    }
  } else if (!platform_exe_in_path(prog)) {
    return false;
  }

  char q[2200];
  size_t n = 0;
  q[n++] = '\'';
  for (const char *s = prog; *s && n + 5 < sizeof(q); s++) {
    if (*s == '\'') {
      memcpy(q + n, "'\\''", 4);
      n += 4;
    } else
      q[n++] = *s;
  }
  q[n++] = '\'';
  q[n] = '\0';

  snprintf(out, out_size, "%s%s%s", q, p[0] ? " " : "", p);
  return true;
}

static int platform_run_status(char *const argv[], const X11SessionEnv *env,
                               bool apply_env) {
  pid_t pid = fork();
  if (pid < 0)
    return -1;
  if (pid == 0) {
    if (apply_env && env)
      x11_env_apply_to_current_process(env);
    execvp(argv[0], argv);
    _exit(127);
  }
  int st = 0;
  if (waitpid(pid, &st, 0) < 0)
    return -1;
  if (!WIFEXITED(st))
    return -1;
  return WEXITSTATUS(st);
}

bool sk_launch_with_args_platform(const char *path, const char *args) {
  log_info("sk_launch path='%s' args='%s'", path ? path : "null",
           args ? args : "null");
  if (!path)
    return false;
  char expanded[1024];
  if (!ExpandPath(path, expanded, sizeof(expanded)))
    return false;
  log_info("sk_launch expanded='%s'", expanded);
  path = expanded;
  if (strstr(path, ".desktop") != NULL) {
    log_info("sk_launch desktop detected expanded='%s'", expanded);
    bool is_terminal = false;
    char exec_line[1024] = {0};
    char workdir[1024] = {0};
    x11_desktop_parse(path, exec_line, workdir, &is_terminal);
    log_info("x11_desktop_parse exec='%s' workdir='%s' is_terminal=%d", exec_line,
             workdir, is_terminal);
    x11_desktop_clean_exec(exec_line);
    log_info("x11_desktop_clean_exec exec='%s'", exec_line);
    if (is_terminal && exec_line[0]) {
      const char *wd = workdir[0] ? workdir : NULL;
      char extra[2048];
      if (args && args[0])
        snprintf(extra, sizeof(extra), "-e sh -c '%s %s; exec %s'", exec_line,
                 args, UsedShell);
      else
        snprintf(extra, sizeof(extra), "-e sh -c '%s; exec %s'", exec_line,
                 UsedShell);
      if (wd)
        return Platform_LaunchTerminalAtPath(wd, extra);
      else
        return Platform_LaunchTerminalAtPath(Platform_GetRealHome(), extra);
    }
    if (exec_line[0] && args && args[0]) {
      char combined[2048];
      snprintf(combined, sizeof(combined), "%s %s", exec_line, args);
      strncpy(exec_line, combined, sizeof(exec_line) - 1);
      exec_line[sizeof(exec_line) - 1] = '\0';
    }
    if (!exec_line[0]) {
      log_warn("desktop '%s' has no usable Exec line", expanded);
      return false;
    }

    char desktop_dir[1024] = {0};
    snprintf(desktop_dir, sizeof(desktop_dir), "%s", expanded);
    char *dslash = strrchr(desktop_dir, '/');
    if (dslash)
      *dslash = '\0';
    else
      snprintf(desktop_dir, sizeof(desktop_dir), ".");
    const char *run_dir = workdir[0] ? workdir : desktop_dir;

    X11SessionEnv env;
    x11_env_get_session_env(&env);
    char disp_env[256], xauth_env[512], runtime_env[512], wayland_env[512];
    x11_env_build_env_strings(&env, disp_env, xauth_env, runtime_env, wayland_env);
    bool as_root = x11_env_is_running_as_root();
    log_info("desktop '%s' exec='%s' workdir='%s' run_dir='%s' as_root=%d",
             expanded, exec_line, workdir, run_dir, as_root);

    int gio_st;
    if (as_root) {
      char *const av_gio[] = {"sudo",      "-u",        (char *)env.real_user,
                              "env",       disp_env,    xauth_env,
                              runtime_env, wayland_env, "gio",
                              "launch",    expanded,    NULL};
      gio_st = platform_run_status(av_gio, NULL, false);
    } else {
      char *const av_gio2[] = {"gio", "launch", expanded, NULL};
      gio_st = platform_run_status(av_gio2, &env, true);
    }
    log_info("gio launch '%s' exit=%d", expanded, gio_st);
    if (gio_st == 0) {
      return true;
    }

    char cmd[4096];
    if (!platform_resolve_desktop_exec(exec_line, desktop_dir, workdir, cmd,
                                       sizeof(cmd))) {
      log_warn(
          "desktop '%s': cannot resolve Exec '%s' via PATH or desktop dir '%s'",
          expanded, exec_line, desktop_dir);
      return false;
    }
    char launch_cmd[4608];
    snprintf(launch_cmd, sizeof(launch_cmd), "cd '%s' && %s", run_dir, cmd);
    log_info("desktop resolved launch_cmd='%s'", launch_cmd);

    if (as_root) {
      char *const av_run[] = {"sudo",      "-u",        (char *)env.real_user,
                              "env",       disp_env,    xauth_env,
                              runtime_env, wayland_env, "sh",
                              "-c",        launch_cmd,  NULL};
      if (!x11_env_daemon_exec(av_run)) {
        return false;
      }
    } else {
      X11SessionEnv env2 = env;
      char *const av_run2[] = {"sh", "-c", launch_cmd, NULL};
      if (!x11_env_daemon_exec_with_env(av_run2, &env2)) {
        return false;
      }
    }
    return true;
  } else {
    X11SessionEnv env;
    x11_env_get_session_env(&env);
    char disp_env[256], xauth_env[512], runtime_env[512];
    x11_env_build_env_strings(&env, disp_env, xauth_env, runtime_env, NULL);
    struct stat pst;
    bool is_regular_file = (stat(path, &pst) == 0 && S_ISREG(pst.st_mode));
    bool is_executable = (access(path, X_OK) == 0);
    if (is_regular_file && !is_executable) {
      char *const av_open2[] = {"gio", "open", path, NULL};
      if (!x11_env_daemon_exec_with_env(av_open2, &env))
        return false;
      return true;
    }
    char cmd[2048];
    if (args && args[0] && strcmp(args, "0") != 0)
      snprintf(cmd, sizeof(cmd), "\"%s\" %s", path, args);
    else
      snprintf(cmd, sizeof(cmd), "\"%s\"", path);
    if (x11_env_is_running_as_root()) {
      char *const argv[] = {"sudo",      "-u",     (char *)env.real_user,
                            "env",       disp_env, xauth_env,
                            runtime_env, "sh",     "-c",
                            cmd,         NULL};
      if (!x11_env_daemon_exec(argv))
        return false;
      return true;
    } else {
      X11SessionEnv env2 = env;
      pid_t pid = fork();
      if (pid < 0)
        return false;
      if (pid == 0) {
        setsid();
        pid_t pid2 = fork();
        if (pid2 < 0)
          _exit(1);
        if (pid2 > 0)
          _exit(0);
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
          dup2(fd, 0);
          dup2(fd, 1);
          dup2(fd, 2);
          if (fd > 2)
            close(fd);
        }
        x11_env_apply_to_current_process(&env2);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
      }
      int st = 0;
      waitpid(pid, &st, 0);
      return true;
    }
  }
}

bool sk_launch_with_args_as_admin_platform(const char *path, const char *args) {
  if (!path)
    return false;
  char expanded[MAX_PATH];
  if (!ExpandPath(path, expanded, sizeof(expanded)))
    return false;
  char cmd[2048];
  if (args && args[0])
    snprintf(cmd, sizeof(cmd), "%s %s", expanded, args);
  else
    snprintf(cmd, sizeof(cmd), "%s", expanded);
  pid_t pid = fork();
  if (pid < 0)
    return false;
  if (pid == 0) {
    char *argv1[] = {"pkexec", "sh", "-c", cmd, NULL};
    execvp("pkexec", argv1);
    char *argv2[] = {"sudo", "sh", "-c", cmd, NULL};
    execvp("sudo", argv2);
    _exit(1);
  }
  return true;
}

char *sk_search_file_in_dir_platform(const char *directory,
                                     const char *search_string,
                                     char *output_path) {
  if (!directory || !search_string || strlen(search_string) == 0)
    return NULL;
  DIR *dir = opendir(directory);
  if (!dir)
    return NULL;
  static char found_path[MAX_PATH];
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] == '.' &&
        (entry->d_name[1] == '\0' ||
         (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
      continue;
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", directory, entry->d_name);
    struct stat st;
    if (stat(full_path, &st) != 0)
      continue;
    if (S_ISDIR(st.st_mode))
      continue;
    if (String_ContainsCI(entry->d_name, search_string)) {
      snprintf(found_path, sizeof(found_path), "%s", full_path);
      closedir(dir);
      return found_path;
    }
  }
  closedir(dir);
  return NULL;
}

bool sk_place_editbox_platform(const SkEditSpec *spec) {
  if (!spec)
    return false;
  Display *dpy;
  XftDraw *xd;
  Visual *vis;
  Colormap cmap;
  x11_get_draw_context(&dpy, &xd, &vis, &cmap);
  if (!dpy || !xd)
    return false;
  WindowManager *wm = WindowManager_Get();
  PromptState *state = NULL;
  if (wm) {
    Popup *top = WindowManager_GetTopPopup(wm);
    if (top && top->user_data && top->type == POPUP_PROMPT)
      state = (PromptState *)top->user_data;
  }
  EditboxPlace place;
  Editbox_ResolvePlace(spec->buffer, spec->x, spec->y, spec->w, spec->h,
                       spec->fontsize, state, &place);
  int fs = place.fontsize;
  const char *buf = place.buf;
  int x = place.x;
  int y_raw = place.y_raw;
  int w = place.w;
  int h = place.h;
  int text_len = place.text_len;
  int cursor = place.cursor;
  FontHandle fh = spec->font;
  bool needDestroy = false;
  if (!FH_TO_XFT(fh)) {
    fh = x11_create_font_on_display(dpy, fs, UsedFont);
    needDestroy = true;
  }
  XftFont *xf = FH_TO_XFT(fh);
  x11_set_draw_context(dpy, xd, vis, cmap);

  EditboxX11Metrics metrics = {dpy, xf};
  int cursor_x = 0;
  int text_w = 0;
  if (xf) {
    XGlyphInfo ce;
    XftTextExtentsUtf8(dpy, xf, (FcChar8 *)buf, cursor, &ce);
    cursor_x = ce.xOff;
    XGlyphInfo te;
    XftTextExtentsUtf8(dpy, xf, (FcChar8 *)buf, text_len, &te);
    text_w = te.xOff;
  }


  EditboxInit ebi;
  EditboxInitFromSpec init_args = {.state = state,
                                   .buf = buf,
                                   .text_len = text_len,
                                   .cursor = cursor,
                                   .x = x,
                                   .y = y_raw,
                                   .w = w,
                                   .h = h,
                                   .cursor_x = cursor_x,
                                   .text_w = text_w,
                                   .ascent = xf ? xf->ascent : 0,
                                   .descent = xf ? xf->descent : 0,
                                   .fontsize = fs,
                                   .font = fh,
                                   .text_color = spec->color_text,
                                   .bg_color = spec->color_bg,
                                   .placeholder = spec->placeholder,
                                   .caret_height = spec->caret_height,
                                   .caret_width = spec->caret_width,
                                   .caret_x = spec->caret_x,
                                   .caret_y = spec->caret_y,
                                   .caret_pos = spec->caret_pos};
  Editbox_InitFromSpec(&ebi, &init_args);

  EditboxSpec eb;
  Editbox_BuildSpec(&eb, &ebi);
  eb.measure = x11_editbox_measure;
  eb.measure_user = &metrics;

  Editbox_Render(&eb, NULL);
  if (state)
    state->eb->scroll_offset = eb.scroll;

  if (state) {
    if (WindowTitleText && state->window_title) {
      unsigned long title_col = rgb_to_hex(COLOR_TEXT);
      x11_set_draw_context(dpy, xd, vis, cmap);
      Platform_DrawTextRaw(
          (PlDrawTextRawSpec){.x = PROMPT_FOOTER_X,
                              .y = state->height - PROMPT_FOOTER_Y_FROM_BOTTOM,
                              .text = state->window_title,
                              .color = title_col,
                              .font = fh,
                              .align = "LEFT",
                              .pad = PROMPT_FOOTER_PAD,
                              .fontsize = FontSize});
    }
  }
  if (needDestroy)
    Platform_DestroyFont(fh);
  return true;
}


static void *x11_timer_thread(void *arg) {

  SkRunAfterPayload *payload = (SkRunAfterPayload *)arg;
  if (!payload)
    return NULL;

  useconds_t micros = (useconds_t)(payload->seconds * 1000000.0);
  usleep(micros);

  if (payload->callback) {
    payload->callback();
  }
  ThreadRegistry_Finish(payload);
  free(payload);
  return NULL;
}

bool sk_run_after_time_arm_platform(SkRunAfterPayload *payload) {

  pthread_t thread;
  if (pthread_create(&thread, NULL, x11_timer_thread, payload) != 0)
    return false;
  pthread_detach(thread);
  return true;
}

void sk_sleep_platform(double seconds) {
  if (seconds <= 0)
    return;
  struct timespec req;
  req.tv_sec = (time_t)seconds;
  req.tv_nsec = (long)((seconds - (double)req.tv_sec) * 1000000000.0);
  nanosleep(&req, NULL);
}

int sk_get_active_interface_platform(char out[], size_t max_len) {
  static char content[16384];
  if (File_ReadAll("/proc/net/route", content, (long)sizeof(content)) <= 0)
    return 0;
  int found = 0;
  char *cur = content;
  if (*cur) {
    char *nl = strchr(cur, '\n');
    if (nl)
      *nl = '\0';
    cur = nl ? nl + 1 : cur + strlen(cur);
  }
  while (*cur) {
    char *nl = strchr(cur, '\n');
    if (nl)
      *nl = '\0';
    char *line = cur;
    cur = nl ? nl + 1 : cur + strlen(cur);
    char iface[64];
    unsigned long dest, mask;
    if (sscanf(line, "%63s %lx %*s %*s %*s %*s %*s %lx", iface, &dest, &mask) ==
        3) {
      if (dest == 0 && mask == 0) {
        strncpy(out, iface, max_len - 1);
        out[max_len - 1] = '\0';
        found = 1;
        break;
      }
    }
  }
  return found;
}
unsigned long long sk_net_rx_bytes_platform() {
  char want[64];
  if (!sk_get_active_interface_platform(want, sizeof(want)) || !want[0])
    return 0;
  FILE *f = fopen("/proc/net/dev", "r");
  if (!f)
    return 0;
  char buf[512];
  unsigned long long rx_bytes = 0;

  if (fgets(buf, sizeof(buf), f) && fgets(buf, sizeof(buf), f)) {
    while (fgets(buf, sizeof(buf), f)) {
      char name[64];
      unsigned long long bytes = 0;
      if (sscanf(buf, " %63[^:]:%llu", name, &bytes) == 2) {
        char *trimmed = name;
        while (*trimmed == ' ' || *trimmed == '\t')
          trimmed++;
        char *tail = trimmed + strlen(trimmed);
        while (tail > trimmed && (tail[-1] == ' ' || tail[-1] == '\t' ||
                                  tail[-1] == '\n' || tail[-1] == '\r'))
          *--tail = '\0';
        if (strcmp(trimmed, want) == 0) {
          rx_bytes = bytes;
          break;
        }
      }
    }
  }
  fclose(f);
  return rx_bytes;
}

SkNetStatus sk_net_status_platform(void) {
  char iface[32] = {0};
  int found = sk_get_active_interface_platform(iface, sizeof(iface));

  if (!found)
    return SK_NET_OFFLINE;
  char sysfs_path[128];
  snprintf(sysfs_path, sizeof(sysfs_path), "/sys/class/net/%s/carrier", iface);

  FILE *f = fopen(sysfs_path, "r");
  if (!f)
    return SK_NET_OFFLINE;

  char carrier = (char)fgetc(f);
  fclose(f);

  if (carrier != '1')
    return SK_NET_OFFLINE;

  if (strncmp(iface, "en", 2) == 0 || strncmp(iface, "eth", 3) == 0) {
    return SK_NET_ETHERNET;
  }
  if (strncmp(iface, "wl", 2) == 0) {
    return SK_NET_WIFI;
  }
  return SK_NET_OFFLINE;
}

bool sk_dismiss_message_platform(const char *name) {
  Platform_DismissIndefiniteMessage();
  return Platform_DismissMessage(name);
}

int sk_get_pid_platform() { return (int)getpid(); }
