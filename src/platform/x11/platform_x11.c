#include <stdarg.h>
#include "wm/x11_wm.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_text.h"
#include "helpers/helpers_fs.h"
#include "helpers/x11/raw_platform_x11.h"
#define _GNU_SOURCE
#define _XOPEN_SOURCE 500

#if __has_include(<X11/extensions/XTest.h>)

#include <X11/extensions/XTest.h>

#endif

bool Platform_LaunchTerminalAtPath(const char *working_dir,
                                   const char *extra_args) {
  if (!terminal_guard(working_dir, extra_args))
    return false;

  X11SessionEnv env;
  x11_env_get_session_env(&env);
  char disp_env[256], xauth_env[512], runtime_env[512], wayland_env[512];
  x11_env_build_env_strings(&env, disp_env, xauth_env, runtime_env,
                            wayland_env);

  pid_t pid = fork();
  if (pid < 0) {
    log_error("fork failed: %s", strerror(errno));
    return false;
  }

  if (pid == 0) {
    setsid();
    pid_t pid2 = fork();
    if (pid2 < 0)
      _exit(1);
    if (pid2 > 0)
      _exit(0);

    if (x11_env_is_running_as_root()) {
      if (extra_args && extra_args[0]) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "%s --working-directory=\"%s\" %s",
                 UsedTerminal, working_dir, extra_args);
        char *const argv[] = {"sudo",      "-u",        (char *)env.real_user,
                              "env",       disp_env,    xauth_env,
                              runtime_env, wayland_env, "sh",
                              "-c",        cmd,         NULL};
        execvp(argv[0], argv);
      } else {
        char *const argv[] = {"sudo",
                              "-u",
                              (char *)env.real_user,
                              "env",
                              disp_env,
                              xauth_env,
                              runtime_env,
                              wayland_env,
                              (char *)UsedTerminal,
                              "--working-directory",
                              (char *)working_dir,
                              NULL};
        execvp(argv[0], argv);
        char *const argv2[] = {"sudo",      "-u",        (char *)env.real_user,
                               "env",       disp_env,    xauth_env,
                               runtime_env, wayland_env, (char *)UsedTerminal,
                               NULL};
        execvp(argv2[0], argv2);
      }
    } else {
      x11_env_apply_to_current_process(&env);
      if (extra_args && extra_args[0]) {
        char cmd[2048];
        if (strncmp(extra_args, "-e", 2) == 0) {
          snprintf(cmd, sizeof(cmd), "%s --working-directory=\"%s\" %s",
                   UsedTerminal, working_dir, extra_args);
        } else {
          snprintf(cmd, sizeof(cmd), "%s --working-directory=\"%s\" -e %s",
                   UsedTerminal, working_dir, extra_args);
        }
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        if (strncmp(extra_args, "-e", 2) == 0) {
          snprintf(cmd, sizeof(cmd), "%s %s", UsedTerminal, extra_args);
        } else {
          snprintf(cmd, sizeof(cmd), "%s -e %s", UsedTerminal, extra_args);
        }
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
      } else {
        execlp(UsedTerminal, UsedTerminal, "--working-directory", working_dir,
               (char *)NULL);
      }
    }
    _exit(127);
  }

  int st = 0;
  waitpid(pid, &st, 0);
  if (WIFEXITED(st) && WEXITSTATUS(st) == 127) {
    log_error("failed to exec terminal '%s' (extra_args='%s', dir='%s') - "
              "check UsedTerminal config and PATH",
              UsedTerminal, extra_args ? extra_args : "", working_dir);
  } else if (WIFEXITED(st) && WEXITSTATUS(st) != 0) {
    log_warn("terminal '%s' exited with status %d", UsedTerminal,
             WEXITSTATUS(st));
  }
  log_info("Platform_LaunchTerminalAtPath At Path %s", working_dir);
  return true;
}

char *Platform_RunCommandAndGetOutput(const char *command) {
  FILE *fp;
  char buffer[8192];
  size_t cap = 8192;
  size_t total_size = 0;
  char *output = malloc(cap);

  if (output == NULL) {
    log_error("malloc failed command='%s'", command ? command : "null");
    return NULL;
  }

  fp = popen(command, "r");
  if (fp == NULL) {
    log_error("popen failed cmd='%s' err=%d", command ? command : "null",
              errno);
    free(output);
    return NULL;
  }

  size_t len;
  while ((len = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
    while (total_size + len + 1 > cap) {
      cap *= 2;
      char *new_output = realloc(output, cap);
      if (!new_output) {
        free(output);
        pclose(fp);
        return NULL;
      }
      output = new_output;
    }
    memcpy(output + total_size, buffer, len);
    total_size += len;
  }

  pclose(fp);
  output[total_size] = '\0';
  if (total_size == 0)
    log_error("popen empty cmd='%s'", command ? command : "");
  log_info("Platform_RunCommandAndGetOutput command='%s'",
           command ? command : "");
  ;
  return output;
}

static char *x11_run_command_detached_capture(const char *command) {

  if (!command || !*command) {
    log_error("capture: null/empty command");
    return NULL;
  }

  const char *real_user = Platform_GetRealUser();
  const char *real_home = Platform_GetRealHome();

  log_info("capture: original cmd='%s'", command);
  log_info("capture: real_user='%s' real_home='%s' is_root=%d",
           real_user ? real_user : "(null)", real_home ? real_home : "(null)",
           Platform_IsRunningAsRoot());

  char expanded[4096];
  const char *final_cmd = command;

  if (command[0] == '~' && (command[1] == '/' || command[1] == '\0') &&
      real_home && real_home[0]) {
    snprintf(expanded, sizeof(expanded), "%s%s", real_home, command + 1);
    final_cmd = expanded;
    log_info("capture: expanded cmd='%s'", final_cmd);
  }

  {
    char test_path[4096];
    const char *space = strchr(final_cmd, ' ');
    size_t len = space ? (size_t)(space - final_cmd) : strlen(final_cmd);
    if (len >= sizeof(test_path))
      len = sizeof(test_path) - 1;
    memcpy(test_path, final_cmd, len);
    test_path[len] = '\0';

    if (access(test_path, X_OK) != 0) {
      log_error(
          "capture: script not executable or not found: '%s' (errno=%d %s)",
          test_path, errno, strerror(errno));
    } else {
      log_info("capture: script exists and is executable: '%s'", test_path);
    }
  }

  int pfd[2];
  if (pipe(pfd) != 0) {
    log_error("capture: pipe failed cmd='%s' err=%d %s", final_cmd, errno,
              strerror(errno));
    return NULL;
  }

  pid_t pid = fork();
  if (pid < 0) {
    log_error("capture: fork failed cmd='%s' err=%d %s", final_cmd, errno,
              strerror(errno));
    close(pfd[0]);
    close(pfd[1]);
    return NULL;
  }

  if (pid == 0) {
    if (real_home)
      setenv("HOME", real_home, 1);
    if (real_user) {
      setenv("USER", real_user, 1);
      setenv("LOGNAME", real_user, 1);
    }

    close(pfd[0]);
    signal(SIGPIPE, SIG_IGN);
    dup2(pfd[1], STDOUT_FILENO);
    dup2(pfd[1], STDERR_FILENO);
    if (pfd[1] > 2)
      close(pfd[1]);

    int fd = open("/dev/null", O_RDONLY);
    if (fd >= 0) {
      dup2(fd, STDIN_FILENO);
      if (fd > 2)
        close(fd);
    }

    execl("/bin/sh", "sh", "-c", final_cmd, (char *)NULL);

    const char *msg = "execl /bin/sh failed\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    _exit(127);
  }

  close(pfd[1]);
  log_info("capture: forked child pid=%d for cmd='%s'", (int)pid, final_cmd);

  int flags = fcntl(pfd[0], F_GETFL, 0);
  if (flags >= 0)
    fcntl(pfd[0], F_SETFL, flags | O_NONBLOCK);

  size_t cap = 8192;
  size_t total = 0;
  char *out = malloc(cap);
  if (!out) {
    log_error("capture: malloc failed");
    close(pfd[0]);
    waitpid(pid, NULL, 0);
    return NULL;
  }

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);

  const long drain_ms = 8000;
  const long start_ms = 30000;
  long deadline = start_ms;
  bool got_data = false;
  bool eof = false;
  char buf[16096];
  int poll_count = 0;

  while (1) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long elapsed = (now.tv_sec - start.tv_sec) * 1000L +
                   (now.tv_nsec - start.tv_nsec) / 1000000L;
    long remain = deadline - elapsed;
    if (remain <= 0) {
      log_info("capture: deadline reached (elapsed=%ld ms, got_data=%d)",
               elapsed, got_data);
      break;
    }

    struct pollfd p = {.fd = pfd[0], .events = POLLIN | POLLHUP | POLLERR};
    int r = poll(&p, 1, (int)remain);
    poll_count++;

    if (r < 0) {
      if (errno == EINTR)
        continue;
      log_error("capture: poll error err=%d %s", errno, strerror(errno));
      break;
    }
    if (r == 0) {
      log_info("capture: poll timeout (elapsed=%ld)", elapsed);
      break;
    }

    log_info("capture: poll ok revents=0x%x elapsed=%ld", p.revents, elapsed);

    ssize_t n;
    bool read_any = false;
    while ((n = read(pfd[0], buf, sizeof(buf))) > 0) {
      read_any = true;
      if (!got_data) {
        got_data = true;
        deadline = elapsed + drain_ms;
        log_info("capture: first data received (%zd bytes), new deadline=%ld",
                 n, deadline);
      }
      while (total + (size_t)n + 1 > cap) {
        cap *= 2;
        char *no = realloc(out, cap);
        if (!no) {
          log_error("capture: realloc failed");
          free(out);
          close(pfd[0]);
          waitpid(pid, NULL, 0);
          return NULL;
        }
        out = no;
      }
      memcpy(out + total, buf, (size_t)n);
      total += (size_t)n;

      clock_gettime(CLOCK_MONOTONIC, &now);
      elapsed = (now.tv_sec - start.tv_sec) * 1000L +
                (now.tv_nsec - start.tv_nsec) / 1000000L;
      if (got_data)
        deadline = elapsed + drain_ms;
    }

    if (n == 0) {
      log_info("capture: read returned 0 (EOF)");
      eof = true;
      break;
    }
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      log_error("capture: read error n=%zd err=%d %s", n, errno,
                strerror(errno));
      if (p.revents & POLLHUP)
        eof = true;
      break;
    }
    if (!read_any && (p.revents & POLLHUP)) {
      log_info("capture: POLLHUP with no data");
      eof = true;
      break;
    }
  }

  close(pfd[0]);

  int status = 0;
  pid_t waited = waitpid(pid, &status, 0);
  log_info("capture: waitpid returned %d, status=0x%x", (int)waited, status);

  if (WIFEXITED(status)) {
    log_info("capture: child exited normally with code %d",
             WEXITSTATUS(status));
  } else if (WIFSIGNALED(status)) {
    log_error("capture: child killed by signal %d", WTERMSIG(status));
  } else {
    log_error("capture: child ended with unexpected status 0x%x", status);
  }

  if (total + 1 > cap) {
    char *no = realloc(out, total + 1);
    if (!no) {
      free(out);
      return NULL;
    }
    out = no;
  }
  out[total] = '\0';

  log_info("capture: finished total_bytes=%zu eof=%d poll_count=%d", total, eof,
           poll_count);
  if (total == 0) {
    log_error("capture empty cmd='%s' eof=%d status=0x%x", final_cmd, eof,
              status);
  } else {
    char preview[2001];
    size_t plen = total < 2000 ? total : 2000;
    memcpy(preview, out, plen);
    preview[plen] = '\0';
    log_info("capture: output preview: '%s'", preview);
  }
  return out;
}

bool Platform_LaunchDetached(const char *command) {
  if (!command || !command[0]) {
    log_error("LaunchDetached: null or empty command");
    return false;
  }

  log_info("LaunchDetached: starting cmd='%s'", command);

  if (x11_env_detach_fork() != 0)
    return true;
  const char *real_home = Platform_GetRealHome();
  if (real_home)
    setenv("HOME", real_home, 1);

  const char *display = getenv("DISPLAY");
  if (display)
    setenv("DISPLAY", display, 1);

  const char *xauth = getenv("XAUTHORITY");
  if (xauth)
    setenv("XAUTHORITY", xauth, 1);

  const char *dbus = getenv("DBUS_SESSION_BUS_ADDRESS");
  if (dbus)
    setenv("DBUS_SESSION_BUS_ADDRESS", dbus, 1);

  execl("/bin/sh", "sh", "-c", command, (char *)NULL);
  _exit(127);
  return false;
}

char *Platform_RunTerminalCommand(const char *command, bool open_terminal) {
  char timer_text[300];
  snprintf(timer_text, sizeof(timer_text),
           "platform_x11:Platform_RunTerminalCommand Command:%s", command);

  const char *real_user = Platform_GetRealUser();
  const char *real_home = Platform_GetRealHome();

  if (!open_terminal) {
    return x11_run_command_detached_capture(command);
  }

  char temp_file[MAX_PATH];
  char cache_dir[MAX_PATH];
  snprintf(cache_dir, sizeof(cache_dir), "%s/.cache", real_home);
  snprintf(temp_file, sizeof(temp_file), "%s/.cache/temp_cmd.sh", real_home);

  FILE *f = fopen(temp_file, "w");
  if (f) {
    fprintf(f, "#!/bin/%s\n", UsedShell);
    fprintf(f, "%s\n", command);
    fclose(f);
    if (Platform_IsRunningAsRoot()) {
      char chown_cmd[2048];
      snprintf(chown_cmd, sizeof(chown_cmd),
               "chown %.255s:%.255s \"%.511s\" && chmod 755 \"%.511s\"",
               real_user, real_user, temp_file, temp_file);
      Platform_RunTerminalCommand(chown_cmd, false);
    } else {
      chmod(temp_file, 0755);
    }
  }

  char runtime_dir[256];
  snprintf(runtime_dir, sizeof(runtime_dir), "%s", x11_get_runtime_dir());
  char extra[1024];

  snprintf(extra, sizeof(extra), "%s -c \"%s '%s'; exec %s\"", UsedShell,
           UsedShell, temp_file, UsedShell);

  Platform_LaunchTerminalAtPath(real_home, extra);
  log_info("ran terminal command '%s' open_terminal=%d", command,
           open_terminal);
  return NULL;
}

pthread_mutex_t font_mtx = PTHREAD_MUTEX_INITIALIZER;

FontHandle Platform_CreateFont(int size, const char *face_name) {
  pthread_mutex_lock(&font_mtx);

  Display *dpy = x11_get_display();
  if (!dpy) {
    pthread_mutex_unlock(&font_mtx);
    return FH_NULL;
  }

  FontHandle fh = x11_create_font_on_display(dpy, size, face_name);

  pthread_mutex_unlock(&font_mtx);
  log_info("Platform_CreateFont size=%d face_name='%s'", size,
           face_name ? face_name : "");
  ;
  return fh;
}

void Platform_DestroyFont(FontHandle font) {
  XftFont *xf = FH_TO_XFT(font);
  if (!xf) {
    return;
  }
  pthread_mutex_lock(&g_font_cache_mtx);
  bool cached = x11_font_is_cached(FH_TO_DPY(font), xf);
  bool retired = cached ? false : x11_font_was_retired(FH_TO_DPY(font), xf);
  pthread_mutex_unlock(&g_font_cache_mtx);
  if (cached || retired) {
    return;
  }
  XftFontClose(FH_TO_DPY(font), xf);
}

static int xft_text_width(Display *dpy, XftFont *xf, const char *text,
                          int len) {

  XGlyphInfo ext;
  XftTextExtentsUtf8(dpy, xf, (FcChar8 *)text, len, &ext);
  return ext.xOff;
}

void Platform_GetTextExtents(FontHandle font, const char *text, int maxWidth,
                             int *outWidth, int *outHeight) {

  if (!FH_TO_XFT(font) || !text) {
    *outWidth = 0;
    *outHeight = 0;
    log_info("Platform_GetTextExtents text='%s' maxWidth=%d", text ? text : "",
             maxWidth);
    return;
  }
  Display *dpy = FH_TO_DPY(font);
  XftFont *xf = FH_TO_XFT(font);
  int lineHeight = xf->ascent + xf->descent;
  int spaceWidth = xft_text_width(dpy, xf, " ", 1);

  int lineWidth = 0;
  int totalHeight = lineHeight;
  const char *p = text;
  while (*p) {
    const char *word_start = p;
    while (*p && *p != ' ')
      p++;
    int wordWidth = xft_text_width(dpy, xf, word_start, p - word_start);
    int add = wordWidth + (lineWidth > 0 ? spaceWidth : 0);
    if (lineWidth > 0 && lineWidth + add > maxWidth) {
      totalHeight += lineHeight;
      lineWidth = wordWidth;
    } else {
      lineWidth += add;
    }
    while (*p == ' ')
      p++;
  }
  *outWidth = lineWidth;
  *outHeight = totalHeight;
}

bool Platform_FocusWindow(const char *title) { return wm_focus_by_title(title); }

WindowHandle Platform_GetWindowByTitle(const char *title) {
  WindowHandle h = WH_NULL;
  unsigned long w = x11_find_window_by_title(title);
  if (w != 0)
    h.xid = w;
  return h;
}

WindowHandle Platform_GetWindowByRootAtom(const char *atom) {
  WindowHandle h = WH_NULL;
  if (!atom || !atom[0])
    return h;
  Display *dpy = x11_get_display();
  if (!dpy)
    return h;
  Atom a = XInternAtom(dpy, atom, False);
  Atom type = None;
  int fmt = 0;
  unsigned long nitems = 0, after = 0;
  unsigned char *data = NULL;
  if (XGetWindowProperty(dpy, DefaultRootWindow(dpy), a, 0, 1, False, XA_WINDOW,
                         &type, &fmt, &nitems, &after, &data) != Success ||
      !data)
    return h;
  if (type == XA_WINDOW && fmt == 32 && nitems >= 1)
    h.xid = (Window)((long *)data)[0];
  XFree(data);
  return h;
}

bool Platform_SetWindowStringProperty(WindowHandle window, const char *atom,
                                      const char *value) {
  Display *dpy = x11_get_display();
  if (!dpy || window.xid == 0 || !atom || !value)
    return false;
  XChangeProperty(dpy, (Window)window.xid, XInternAtom(dpy, atom, False),
                  XA_STRING, 8, PropModeReplace, (const unsigned char *)value,
                  (int)strlen(value));
  XFlush(dpy);
  return true;
}

bool Platform_SetWindowLongProperty(WindowHandle window, const char *atom,
                                    long value) {
  Display *dpy = x11_get_display();
  if (!dpy || window.xid == 0 || !atom)
    return false;
  XChangeProperty(dpy, (Window)window.xid, XInternAtom(dpy, atom, False),
                  XA_INTEGER, 32, PropModeReplace,
                  (const unsigned char *)&value, 1);
  XFlush(dpy);
  return true;
}

bool Platform_SendWindowMessage(WindowHandle window, const char *atom,
                                long data) {
  Display *dpy = x11_get_display();
  if (!dpy || window.xid == 0 || !atom)
    return false;
  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.xclient.type = ClientMessage;
  ev.xclient.window = (Window)window.xid;
  ev.xclient.message_type = XInternAtom(dpy, atom, False);
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = data;
  if (XSendEvent(dpy, (Window)window.xid, False, NoEventMask, &ev) == 0)
    return false;
  XFlush(dpy);
  return true;
}

bool Platform_CloseActiveWindow(void) { return wm_close_active_window(); }

bool Platform_ToggleFullscreenActive(void) {
  return wm_toggle_fullscreen_active();
}

bool Platform_MoveActiveWindowToMonitor(bool right) {
  return wm_move_active_window_to_monitor(right);
}

bool Platform_MoveActiveWindowLeft(void) { return wm_move_active_window_left(); }

bool Platform_MoveActiveWindowRight(void) { return wm_move_active_window_right(); }

bool Platform_MoveActiveWindowUp(void) { return wm_move_active_window_up(); }

bool Platform_MoveActiveWindowDown(void) { return wm_move_active_window_down(); }

bool Platform_FocusWindowLeft(void) { return wm_focus_window_left(); }

bool Platform_FocusWindowRight(void) { return wm_focus_window_right(); }

bool Platform_FocusWindowUp(void) { return wm_focus_window_up(); }

bool Platform_FocusWindowDown(void) { return wm_focus_window_down(); }

bool Platform_MinimizeActiveWindow(void) { return wm_minimize_active_window(); }

char *Platform_GetActiveWindowTitle(void) { return wm_get_active_window_title(); }

int Platform_GetIconSize(char *icon_path) {
  if (strstr(icon_path, "32x32")) {
    return 32;
  }
  if (strstr(icon_path, "48x48")) {
    return 48;
  }
  if (strstr(icon_path, "64x64")) {
    return 64;
  }
  if (strstr(icon_path, "128x128")) {
    return 128;
  }
  if (strstr(icon_path, "256x256")) {
    return 256;
  }
  return 32;
}

#define MAX_ICON_CACHE 1024

static struct {
  char key[512];
  char value[1024];
} icon_cache[MAX_ICON_CACHE];

static int icon_cache_count = 0;

static bool icon_cache_loaded = false;

static bool icon_cache_path(char *out, size_t n) {
  return feature_path_get("core", "icon_cache.txt", out, n);
}

static void icon_cache_load(void) {

  if (icon_cache_loaded) {
    return;
  }
  icon_cache_loaded = true;
  char cf[1024];
  static char content[MAX_ICON_CACHE * 1600];
  if (!icon_cache_path(cf, sizeof(cf))) {
    return;
  }
  if (file_read_all(cf, content, (long)sizeof(content)) <= 0) {
    return;
  }
  char *cur = content;
  while (*cur && icon_cache_count < MAX_ICON_CACHE) {
    char *nl = strchr(cur, '\n');
    if (nl)
      *nl = '\0';
    char *line = cur;
    cur = nl ? nl + 1 : cur + strlen(cur);
    line[strcspn(line, "\r\n")] = '\0';
    char *eq = strchr(line, '=');
    if (!eq)
      continue;
    *eq = '\0';
    safe_copy(icon_cache[icon_cache_count].key, sizeof(icon_cache[0].key), line);
    safe_copy(icon_cache[icon_cache_count].value, sizeof(icon_cache[0].value),
             eq + 1);
    if (access(icon_cache[icon_cache_count].value, F_OK) == 0)
      icon_cache_count++;
  }
}

static const char *icon_cache_get(const char *key) {

  icon_cache_load();
  for (int i = 0; i < icon_cache_count; i++)
    if (strcmp(icon_cache[i].key, key) == 0) {
      return icon_cache[i].value;
    }
  return NULL;
}

static void icon_cache_set(const char *key, const char *value) {

  icon_cache_load();
  for (int i = 0; i < icon_cache_count; i++)
    if (strcmp(icon_cache[i].key, key) == 0) {
      return;
    }
  if (icon_cache_count >= MAX_ICON_CACHE) {
    return;
  }
  safe_copy(icon_cache[icon_cache_count].key, sizeof(icon_cache[0].key), key);
  safe_copy(icon_cache[icon_cache_count].value, sizeof(icon_cache[0].value),
           value);
  icon_cache_count++;
  char cf[1024];
  FILE *f = NULL;
  if (icon_cache_path(cf, sizeof(cf)))
    f = fopen(cf, "a");
  if (!f) {
    return;
  }
  fprintf(f, "%s=%s\n", key, value);
  fclose(f);
}

static char *icon_probe(const char *path, const char *bare) {
  if (access(path, F_OK) != 0)
    return NULL;
  icon_cache_set(bare, path);
  return strdup(path);
}

static char *icon_try(const char *bare, const char *fmt, ...) {
  char path[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(path, sizeof(path), fmt, ap);
  va_end(ap);
  return icon_probe(path, bare);
}
static char *x11_icon_try_sizes(const char *bare, const char *dir,
                                const char *const *sizes,
                                const char *const *fmts, int n_fmts) {
  for (int s = 0; sizes[s]; s++) {
    const char *sz = sizes[s];
    while (*sz == ' ')
      sz++;
    for (int f = 0; f < n_fmts; f++) {
      char *hit = icon_try(bare, fmts[f], dir, sz, bare);
      if (hit)
        return hit;
    }
  }
  return NULL;
}

char *Platform_GetActiveWindowAppId(void) { return wm_get_active_window_app_id(); }

bool Platform_PrewarmImage(char *path) {
  extern bool x11_prewarm_image(Display *dpy, const char *path);
  Display *dpy = x11_get_display();
  if (!dpy || !path || !path[0])
    return false;
  return x11_prewarm_image(dpy, path);
}

char *Platform_GetIconPath(char *app_path) {
  if (!app_path) {
    log_error("operation failed, returning error app_path='%s'",
              app_path ? app_path : "null");
    return NULL;
  }
  if (app_path[0] == '/' && strchr(app_path, '.') &&
      access(app_path, F_OK) == 0) {
    if (strstr(app_path, ".png") || strstr(app_path, ".svg") ||
        strstr(app_path, ".xpm"))
      return strdup(app_path);
  }
  char icon_name[512] = {0};
  if (strstr(app_path, ".desktop")) {
    FILE *f = fopen(app_path, "r");
    if (f) {
      char line[1024];
      while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t')
          p++;
        if (string_contains_ci(p, "Icon=")) {
          p = strchr(p, '=');
          if (p) {
            p++;
            while (*p == ' ' || *p == '\t')
              p++;
          }
          char *end = strpbrk(p, " \r\n#");
          if (end)
            *end = '\0';
          safe_copy(icon_name, sizeof(icon_name), p);
          break;
        }
      }
      fclose(f);
    }
  } else {
    safe_copy(icon_name, sizeof(icon_name), app_path);
  }
  if (icon_name[0] == '/' && access(icon_name, F_OK) == 0) {
    if (strstr(icon_name, ".png") || strstr(icon_name, ".svg") ||
        strstr(icon_name, ".xpm"))
      return strdup(icon_name);
  }
  char bare[512];
  safe_copy(bare, sizeof(bare), icon_name);
  char *dot = strrchr(bare, '.');
  if (dot && (strcmp(dot, ".png") == 0 || strcmp(dot, ".svg") == 0 ||
              strcmp(dot, ".xpm") == 0))
    *dot = '\0';
  if (bare[0]) {
    const char *cached = icon_cache_get(bare);
    if (cached && access(cached, F_OK) == 0)
      return strdup(cached);
  }
  const char *sizes[] = {"32x32",   "48x48",    "64x64", "128x128",
                         "256x256", "scalable", NULL};
  const char *bases[] = {"/usr/share/icons/hicolor", "/usr/share/pixmaps",
                         "/usr/share/icons/Adwaita", "/usr/share/icons/breeze",
                         NULL};
  char *hit = NULL;
  const char *home = Platform_GetRealHome();
  if (!home || !home[0])
    home = x11_env_home_dir();
  if (home && home[0]) {
    const char *fmts[] = {
        "%s/.local/share/icons/hicolor/%s/apps/%s.png",
        "%s/.local/share/icons/hicolor/%s/apps/%s.svg",
    };
    hit = x11_icon_try_sizes(bare, home, (const char *const *)sizes, fmts, 2);
    if (hit)
      return hit;
  }
  {
    const char *xdg = getenv("XDG_DATA_DIRS");
    char xdg_dirs[1024] = {0};
    if (xdg && xdg[0]) {
      safe_copy(xdg_dirs, sizeof(xdg_dirs), xdg);
    } else {
      snprintf(xdg_dirs, sizeof(xdg_dirs), "/usr/local/share:/usr/share");
    }
    char *save = NULL;
    for (char *dir = strtok_r(xdg_dirs, ":", &save); dir;
         dir = strtok_r(NULL, ":", &save)) {
      {
        const char *fmts[] = {
            "%s/icons/hicolor/%s/apps/%s.png",
            "%s/icons/hicolor/%s/apps/%s.svg",
        };
        hit =
            x11_icon_try_sizes(bare, dir, (const char *const *)sizes, fmts, 2);
        if (hit)
          return hit;
      }
      hit = icon_try(bare, "%s/icons/hicolor/%s.svg", dir, bare);
      if (hit)
        return hit;
      hit = icon_try(bare, "%s/icons/hicolor/%s.png", dir, bare);
      if (hit)
        return hit;
      hit = icon_try(bare, "%s/pixmaps/%s.png", dir, bare);
      if (hit)
        return hit;
      hit = icon_try(bare, "%s/pixmaps/%s.svg", dir, bare);
      if (hit)
        return hit;
    }
  }
  for (int b = 0; bases[b]; b++) {
    const char *fmts[] = {
        "%s/%s/apps/%s.png",
        "%s/%s/apps/%s.svg",
    };
    hit =
        x11_icon_try_sizes(bare, bases[b], (const char *const *)sizes, fmts, 2);
    if (hit)
      return hit;
    hit = icon_try(bare, "%s/%s.svg", bases[b], bare);
    if (hit)
      return hit;
    hit = icon_try(bare, "%s/%s.png", bases[b], bare);
    if (hit)
      return hit;
  }
  char pix[1024];
  hit = icon_try(bare, "/usr/share/pixmaps/%s.png", bare);
  if (hit)
    return hit;
  hit = icon_try(bare, "/usr/share/pixmaps/%s.svg", bare);
  if (hit)
    return hit;
  hit = icon_try(bare, "/usr/share/pixmaps/%s.xpm", bare);
  if (hit)
    return hit;
  snprintf(pix, sizeof(pix), "resources/IconFallback.png");
  if (access(pix, F_OK) == 0)
    return strdup(pix);
  log_info("Platform_GetIconPath app_path='%s'", app_path ? app_path : "");
  ;
  return strdup(bare[0] ? bare : "placeholder");
}

#include "helpers/x11/clipboard_x11.h"

#include "platform/x11/clipboard_backend_x11.h"

int Platform_CopyToClipboard(const char *text) {
  if (!text)
    return 0;
  long long t0 = Platform_GetWallClockMs();
  int wr = 0;
  int xr = 0;
  log_info("COPY BEGIN pid=%d len=%d text='%s' WAYLAND_DISPLAY=%s "
           "HYPRLAND_INSTANCE_SIGNATURE=%s",
           (int)getpid(), (int)strlen(text), text,
           getenv("WAYLAND_DISPLAY") ? getenv("WAYLAND_DISPLAY") : "(unset)",
           getenv("HYPRLAND_INSTANCE_SIGNATURE") ? "set" : "(unset)");
  if (x11_clipboard_available()) {
    long long tw = Platform_GetWallClockMs();
    wr = x11_clipboard_set_text(text);
    log_info("COPY step1 wayland ret=%d ms=%lld", wr,
             Platform_GetWallClockMs() - tw);
  } else {
    log_info("COPY step1 wayland SKIPPED (no WAYLAND_DISPLAY)");
  }
  long long tx = Platform_GetWallClockMs();
  xr = x11_clipboard_set_text(text);
  log_info("COPY step2 x11 ret=%d ms=%lld", xr, Platform_GetWallClockMs() - tx);
  if (!wr && !xr)
    log_warn("Platform_CopyToClipboard failed both backends");
  log_info("COPY END total_ms=%lld len=%d wr=%d xr=%d",
           Platform_GetWallClockMs() - t0, (int)strlen(text), wr, xr);
  return (wr || xr);
}

int Platform_CopyImageToClipboard(const unsigned char *png, size_t len) {
  if (!png || len == 0)
    return 0;
  int wr = 0;
  int xr = 0;
  if (x11_clipboard_available()) {
    wr = x11_clipboard_set_image(png, len);
    log_info("Platform_CopyImageToClipboard wayland=%d", wr);
  }
  xr = x11_clipboard_set_image(png, len);
  log_info("Platform_CopyImageToClipboard x11=%d", xr);
  if (!wr && !xr)
    log_warn("Platform_CopyImageToClipboard failed both backends");
  return (wr || xr);
}

int Platform_SetFileToClipboard(const char *path) {
  if (!path || path[0] == 0)
    return 0;
  int wr = 0;
  int xr = 0;
  if (x11_clipboard_available()) {
    wr = x11_clipboard_set_file(path);
    log_info("Platform_SetFileToClipboard wayland=%d", wr);
  }
  xr = x11_clipboard_set_file(path);
  log_info("Platform_SetFileToClipboard x11=%d", xr);
  return (wr || xr);
}

char *Platform_RunArgvCapture(const char *const argv[], const char *stdin_data,
                              int *out_status) {

  if (out_status)
    *out_status = -1;
  if (!argv || !argv[0])
    return NULL;
  int pin[2] = {-1, -1};
  if (stdin_data && stdin_data[0]) {
    if (pipe(pin) != 0) {
      log_error("Platform_RunArgvCapture pipe failed: %s", strerror(errno));
      return NULL;
    }
  }
  int pout[2] = {-1, -1};
  if (pipe(pout) != 0) {
    log_error("Platform_RunArgvCapture pipe failed: %s", strerror(errno));
    if (pin[0] >= 0) {
      close(pin[0]);
      close(pin[1]);
    }
    return NULL;
  }
  pid_t pid = fork();
  if (pid < 0) {
    log_error("Platform_RunArgvCapture fork failed: %s", strerror(errno));
    if (pin[0] >= 0) {
      close(pin[0]);
      close(pin[1]);
    }
    close(pout[0]);
    close(pout[1]);
    return NULL;
  }
  if (pid == 0) {
    setsid();
    if (pin[0] >= 0) {
      dup2(pin[0], STDIN_FILENO);
      close(pin[0]);
      close(pin[1]);
    } else {
      int devnull = open("/dev/null", O_RDONLY);
      if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        if (devnull > 2)
          close(devnull);
      }
    }
    dup2(pout[1], STDOUT_FILENO);
    dup2(pout[1], STDERR_FILENO);
    close(pout[0]);
    close(pout[1]);
    signal(SIGPIPE, SIG_DFL);
    execvp(argv[0], (char *const *)argv);
    _exit(127);
  }
  if (pin[0] >= 0) {
    close(pin[0]);
    pin[0] = -1;
  }
  close(pout[1]);
  if (pin[1] >= 0) {
    signal(SIGPIPE, SIG_IGN);
    size_t len = strlen(stdin_data);
    size_t off = 0;
    while (off < len) {
      ssize_t w = write(pin[1], stdin_data + off, len - off);
      if (w <= 0) {
        log_warn("stdin write stopped off=%d/%d err=%s", (int)off, (int)len,
                 strerror(errno));
        break;
      }
      off += (size_t)w;
    }
    if (len == 0 || stdin_data[len - 1] != '\n') {
      ssize_t w = write(pin[1], "\n", 1);
      (void)w;
    }
    close(pin[1]);
    pin[1] = -1;
    signal(SIGPIPE, SIG_DFL);
  }
  size_t cap = 4096, len = 0;
  char *out = malloc(cap);
  if (!out) {
    close(pout[0]);
    waitpid(pid, NULL, 0);
    return NULL;
  }
  for (;;) {
    if (len + 1024 + 1 > cap) {
      size_t ncap = cap * 2;
      char *n = realloc(out, ncap);
      if (!n) {
        free(out);
        close(pout[0]);
        waitpid(pid, NULL, 0);
        return NULL;
      }
      out = n;
      cap = ncap;
    }
    ssize_t r = read(pout[0], out + len, 1024);
    if (r <= 0)
      break;
    len += (size_t)r;
  }
  close(pout[0]);
  out[len] = '\0';
  int st = 0;
  while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {
  }
  if (out_status)
    *out_status = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
  return out;
}

PlWindowList Platform_GetWindowTitles(void) { return Wm_GetWindowTitles(); }

PlWindowList Platform_GetWindowNames(void) { return Wm_GetWindowNames(); }

static int singleton_lock_fd = -1;

bool Platform_TryAcquireSingleInstance(void) {
  char lock_path[128];
  snprintf(lock_path, sizeof(lock_path), "/tmp/skald-%d.lock", (int)getuid());
  singleton_lock_fd = open(lock_path, O_RDWR | O_CREAT, 0600);
  if (singleton_lock_fd < 0) {
    log_error("Cannot open singleton lock %s (%s)", lock_path, strerror(errno));
    return false;
  }
  if (flock(singleton_lock_fd, LOCK_EX | LOCK_NB) != 0) {
    log_info("Another instance is already running. Exiting.");
    close(singleton_lock_fd);
    singleton_lock_fd = -1;
    return false;
  }
  fcntl(singleton_lock_fd, F_SETFD, FD_CLOEXEC);
  return true;
}

bool Platform_InitDisplaySafe(void) {
  const char *dd = getenv("DISPLAY");
  if (!dd || !dd[0]) {
    const char *live = x11_get_session_display();
    if (live && live[0])
      setenv("DISPLAY", live, 1);
  }
  const char *xa = getenv("XAUTHORITY");
  if (xa && xa[0] && access(xa, R_OK) != 0)
    unsetenv("XAUTHORITY");
  const char *xa2 = getenv("XAUTHORITY");
  if (!xa2 || !xa2[0]) {
    const char *live = x11_get_session_xauthority();
    if (live && live[0])
      setenv("XAUTHORITY", live, 1);
  }
  const char *wd = getenv("WAYLAND_DISPLAY");
  if (wd && wd[0] && wd[0] != '/') {
    const char *rtd = x11_env_xdg_runtime_dir();
    if (!rtd || !rtd[0])
      rtd = x11_get_runtime_dir();
    char sock[512];
    snprintf(sock, sizeof(sock), "%s/%s", rtd ? rtd : "", wd);
    struct stat st;
    if (stat(sock, &st) != 0)
      unsetenv("WAYLAND_DISPLAY");
  }
  if (!x11_env_is_wayland()) {
    const char *rtd2 = x11_get_runtime_dir();
    if (rtd2 && rtd2[0]) {
      char pat[512];
      snprintf(pat, sizeof(pat), "%s/wayland-*", rtd2);
      glob_t g;
      memset(&g, 0, sizeof(g));
      if (glob(pat, 0, NULL, &g) == 0) {
        for (size_t i = 0; i < g.gl_pathc; i++) {
          const char *base = strrchr(g.gl_pathv[i], '/');
          base = base ? base + 1 : g.gl_pathv[i];
          size_t bl = strlen(base);
          if (bl > 5 && strcmp(base + bl - 5, ".lock") == 0)
            continue;
          struct stat st2;
          if (stat(g.gl_pathv[i], &st2) == 0) {
            setenv("WAYLAND_DISPLAY", base, 1);
            break;
          }
        }
      }
      globfree(&g);
    }
  }
  if (!x11_init_display()) {
    log_info("Continuing without X11 display (headless mode)");
    return false;
  }
  return true;
}

void Platform_CleanupDisplaySafe(void) {
  x11_clipboard_shutdown();
  x11_clipboard_shutdown();
  x11_cleanup_display();
}

void Platform_InitMainThreadContext(void) {}

void Platform_FixWorkingDirectory(void) {
#if defined(__linux__)
  char buf[512];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0 || (size_t)n >= sizeof(buf) - 1)
    return;
  buf[n] = '\0';
  char *slash = strrchr(buf, '/');
  if (!slash)
    return;
  *slash = '\0';
  char exe_dir[512];
  snprintf(exe_dir, sizeof(exe_dir), "%s", buf);
  for (int depth = 0; depth < 6; ++depth) {
    struct stat st;
    char probe[600];
    snprintf(probe, sizeof(probe), "%s/config", buf);
    bool has_core = stat(probe, &st) == 0 && S_ISDIR(st.st_mode);
    snprintf(probe, sizeof(probe), "%s/build", buf);
    has_core = has_core && stat(probe, &st) == 0 && S_ISDIR(st.st_mode);
    snprintf(probe, sizeof(probe), "%s/data", buf);
    bool has_data = stat(probe, &st) == 0 && S_ISDIR(st.st_mode);
    if (has_core || has_data) {
      if (chdir(buf) != 0)
        log_warn("could not chdir to bundle root '%s'", buf);
      return;
    }
    slash = strrchr(buf, '/');
    if (!slash || slash == buf)
      break;
    *slash = '\0';
  }
  if (exe_dir[0])
    chdir(exe_dir);
#endif
}

void Platform_CleanupPlatformResources(void) {
  x11_cleanup_per_feature_edit_map();
  {
    extern void x11_default_apps_free_cache(void);
    x11_default_apps_free_cache();
  }
  {
    extern void x11_img_cache_free_all(void);
    x11_img_cache_free_all();
  }
  {
    extern void x11_border_cache_free_all(void);
    x11_border_cache_free_all();
  }
  {
    extern void x11_wm_cleanup_cursor_display(void);
    x11_wm_cleanup_cursor_display();
  }
  extern void FcFini(void);
  FcFini();
}

int Platform_InitializeTrayIcon(void) { return 0; }

void Platform_InitUI(void) {}

void Platform_ShutdownUI(void) {}

PlColor Platform_GetPixelColor(int x, int y) {
  PlColor out = {0, 0, 0};
  Display *dpy = x11_get_display();
  if (!dpy)
    return out;
  Window root = DefaultRootWindow(dpy);
  XWindowAttributes attr;
  if (!XGetWindowAttributes(dpy, root, &attr) || !attr.visual) {
    return out;
  }
  XImage *img = XGetImage(dpy, root, x, y, 1, 1, AllPlanes, ZPixmap);
  if (!img)
    return out;
  unsigned long pixel = XGetPixel(img, 0, 0);
  out.r = x11_mask_extract(pixel, attr.visual->red_mask);
  out.g = x11_mask_extract(pixel, attr.visual->green_mask);
  out.b = x11_mask_extract(pixel, attr.visual->blue_mask);
  XDestroyImage(img);
  return out;
}

bool Platform_MoveToPos(int x, int y) {
  Display *dpy = x11_get_display();
  if (!dpy)
    return false;
#if __has_include(<X11/extensions/XTest.h>)
  XTestFakeMotionEvent(dpy, -1, x, y, CurrentTime);
  XSync(dpy, False);
  return true;
#else
  XWarpPointer(dpy, None, DefaultRootWindow(dpy), 0, 0, 0, 0, x, y);
  XSync(dpy, False);
  return true;
#endif
}

bool Platform_ClickButton(int button) {
  Display *dpy = x11_get_display();
  if (!dpy)
    return false;
  int xb = x11_button(button);
  if (xb == 0)
    return false;
#if __has_include(<X11/extensions/XTest.h>)
  usleep(1000);
  XTestFakeButtonEvent(dpy, (unsigned int)xb, True, CurrentTime);
  XSync(dpy, False);
  usleep(1000);
  XTestFakeButtonEvent(dpy, (unsigned int)xb, False, CurrentTime);
  XSync(dpy, False);
  return true;
#else
  return false;
#endif
}

bool Platform_Click(void) {
  Display *dpy = x11_get_display();
  if (!dpy)
    return false;
#if __has_include(<X11/extensions/XTest.h>)
  usleep(60000);
  XTestFakeButtonEvent(dpy, 1, True, CurrentTime);
  XSync(dpy, False);
  usleep(30000);
  XTestFakeButtonEvent(dpy, 1, False, CurrentTime);
  XSync(dpy, False);
  return true;
#else
  return false;
#endif
}

static bool x11_bounds_write(int min_x, int min_y, int max_x, int max_y,
                             int *out_x, int *out_y, int *out_w, int *out_h) {
  if (out_x)
    *out_x = min_x;
  if (out_y)
    *out_y = min_y;
  if (out_w)
    *out_w = max_x - min_x;
  if (out_h)
    *out_h = max_y - min_y;
  return true;
}

bool Platform_GetVirtualBounds(int *out_x, int *out_y, int *out_w, int *out_h) {
  if (out_x)
    *out_x = 0;
  if (out_y)
    *out_y = 0;
  if (out_w)
    *out_w = 0;
  if (out_h)
    *out_h = 0;
  Display *dpy = x11_get_display();
  if (!dpy)
    return false;
  Window root = DefaultRootWindow(dpy);
  int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
  bool any = false;
#if __has_include(<X11/extensions/Xrandr.h>)
  XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
  if (res) {
    for (int c = 0; c < res->ncrtc; c++) {
      XRRCrtcInfo *ci = XRRGetCrtcInfo(dpy, res, res->crtcs[c]);
      if (!ci)
        continue;
      if (ci->noutput > 0 && ci->width > 0 && ci->height > 0) {
        if (!any) {
          min_x = ci->x;
          min_y = ci->y;
          max_x = ci->x + ci->width;
          max_y = ci->y + ci->height;
          any = true;
        } else {
          if (ci->x < min_x)
            min_x = ci->x;
          if (ci->y < min_y)
            min_y = ci->y;
          if (ci->x + (int)ci->width > max_x)
            max_x = ci->x + (int)ci->width;
          if (ci->y + (int)ci->height > max_y)
            max_y = ci->y + (int)ci->height;
        }
      }
      XRRFreeCrtcInfo(ci);
    }
    XRRFreeScreenResources(res);
  }
#endif
  if (!any) {
    min_x = 0;
    min_y = 0;
    max_x = DisplayWidth(dpy, DefaultScreen(dpy));
    max_y = DisplayHeight(dpy, DefaultScreen(dpy));
  }
  return x11_bounds_write(min_x, min_y, max_x, max_y, out_x, out_y, out_w,
                          out_h);
}

bool Platform_MouseButtonDown(int button, bool down) {
  Display *dpy = x11_get_display();
  if (!dpy)
    return false;
  int xb = x11_button(button);
  if (xb == 0)
    return false;
#if __has_include(<X11/extensions/XTest.h>)
  XTestFakeButtonEvent(dpy, (unsigned int)xb, down, CurrentTime);
  XSync(dpy, False);
  return true;
#else
  return false;
#endif
}

bool Platform_MouseScroll(int notches) {
  if (notches == 0)
    return true;
  Display *dpy = x11_get_display();
  if (!dpy)
    return false;
#if __has_include(<X11/extensions/XTest.h>)
  int button = notches > 0 ? 4 : 5;
  int n = notches > 0 ? notches : -notches;
  for (int i = 0; i < n; i++) {
    XTestFakeButtonEvent(dpy, (unsigned int)button, True, CurrentTime);
    XTestFakeButtonEvent(dpy, (unsigned int)button, False, CurrentTime);
    XSync(dpy, False);
  }
  return true;
#else
  return false;
#endif
}

char *Platform_GetWorkingDir(void) {
  char *raw = Platform_RunTerminalCommand("pwd", false);
  if (!raw)
    return NULL;

  size_t len = strlen(raw);
  while (len > 0 && (raw[len - 1] == '\n' || raw[len - 1] == '\r')) {
    raw[--len] = '\0';
  }
  return raw;
}

void Platform_InitDataFolder(void) {
  char dir_path[300];
  snprintf(dir_path, sizeof(dir_path), "%s/.local/share/skald",
           Platform_GetRealHome());
  Platform_MkdirIfNotExists(dir_path);
}

bool Platform_ResolveAssetFile(const char *stem, const char *rel, char *out,
                               size_t out_len) {

  if (!stem || !rel || !out || out_len == 0)
    return false;
  char full[600];
  snprintf(full, sizeof(full), "%s/%s/%s", FeaturePath, stem, rel);
  struct stat st;
  if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) {
    return false;
  }
  snprintf(out, out_len, "%s", full);
  return true;
}

static size_t http_write_cb(char *ptr, size_t sz, size_t nm, void *userdata) {
  HttpBuf *b = (HttpBuf *)userdata;
  size_t n = sz * nm;
  if (n == 0)
    return 0;
  if (b->limit && b->len + n > b->limit) {
    size_t room = b->limit > b->len ? b->limit - b->len : 0;
    if (room > 0) {
      memcpy(b->buf + b->len, ptr, room);
      b->len += room;
      b->buf[b->len] = '\0';
    }
    b->overflow = 1;
    return 0;
  }
  size_t need = b->len + n + 1;
  if (need > b->cap) {
    size_t cap = b->cap ? b->cap : 4096;
    while (cap < need)
      cap *= 2;
    char *grown = (char *)realloc(b->buf, cap);
    if (!grown)
      return 0;
    b->buf = grown;
    b->cap = cap;
  }
  memcpy(b->buf + b->len, ptr, n);
  b->len += n;
  b->buf[b->len] = '\0';
  return n;
}

static void http_setopt_common(CURL *c) {
  curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(c, CURLOPT_MAXREDIRS, (long)HTTP_MAX_REDIRECTS);
  curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, (long)HTTP_CONNECT_TIMEOUT_S);
  curl_easy_setopt(c, CURLOPT_TIMEOUT, (long)HTTP_TIMEOUT_S);
  curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
}

static void http_global_init(void) {
  static int done = 0;
  static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&mtx);
  if (!done) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    done = 1;
  }
  pthread_mutex_unlock(&mtx);
}

static CURL *http_new(const char *url, size_t limit, HttpBuf *buf) {
  http_global_init();
  CURL *c = curl_easy_init();
  if (!c)
    return NULL;
  memset(buf, 0, sizeof(*buf));
  buf->limit = limit;
  if (limit) {
    buf->buf = (char *)malloc(limit + 1);
    if (!buf->buf) {
      curl_easy_cleanup(c);
      return NULL;
    }
    buf->buf[0] = '\0';
    buf->cap = limit + 1;
  }
  curl_easy_setopt(c, CURLOPT_URL, url);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, http_write_cb);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, (void *)buf);
  http_setopt_common(c);
  curl_easy_setopt(c, CURLOPT_USERAGENT, "Skald/1.0");
  curl_easy_setopt(c, CURLOPT_FAILONERROR, 0L);
  return c;
}

static char *http_finish(CURL *c, HttpBuf *buf, CURLcode res, size_t *out_len) {
  if (out_len)
    *out_len = buf->len;
  if (res != CURLE_OK) {
    log_warn("http transfer failed: %s", curl_easy_strerror(res));
    curl_easy_cleanup(c);
    free(buf->buf);
    return NULL;
  }
  curl_easy_cleanup(c);
  if (!buf->buf)
    buf->buf = (char *)calloc(1, 1);
  return buf->buf;
}

char *Platform_HttpGetLimited(const char *url, size_t max_bytes,
                              size_t *out_len) {
  if (!url || !url[0]) {
    if (out_len)
      *out_len = 0;
    return NULL;
  }
  HttpBuf buf;
  CURL *c = http_new(url, max_bytes ? max_bytes : HTTP_DEFAULT_LIMIT, &buf);
  if (!c) {
    if (out_len)
      *out_len = 0;
    return NULL;
  }
  CURLcode res = curl_easy_perform(c);
  char *out = http_finish(c, &buf, res, out_len);
  if (out && buf.overflow)
    log_warn("http response exceeded %zu byte limit", max_bytes);
  return out;
}

char *Platform_HttpGet(const char *url) {
  return Platform_HttpGetLimited(url, HTTP_DEFAULT_LIMIT, NULL);
}

char *Platform_HttpPost(const char *url, const char *content_type,
                        const char *body) {
  if (!url || !url[0])
    return NULL;
  HttpBuf buf;
  CURL *c = http_new(url, HTTP_DEFAULT_LIMIT, &buf);
  if (!c)
    return NULL;
  curl_easy_setopt(c, CURLOPT_POST, 1L);
  curl_easy_setopt(c, CURLOPT_POSTFIELDS, body ? body : "");
  curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)(body ? strlen(body) : 0));
  struct curl_slist *hdrs = curl_slist_append(
      NULL, content_type && content_type[0] ? content_type
                                            : "application/octet-stream");
  curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
  CURLcode res = curl_easy_perform(c);
  char *out = http_finish(c, &buf, res, NULL);
  if (hdrs)
    curl_slist_free_all(hdrs);
  return out;
}

static size_t x11_http_write_file_cb(char *ptr, size_t sz, size_t nm,
                                          void *userdata) {
  return fwrite(ptr, sz, nm, (FILE *)userdata);
}

bool Platform_HttpDownload(const char *url, const char *save_path) {
  if (!url || !url[0] || !save_path || !save_path[0])
    return false;
  char tmp[MAX_PATH];
  snprintf(tmp, sizeof(tmp), "%s.part", save_path);
  FILE *f = fopen(tmp, "wb");
  if (!f)
    return false;
  http_global_init();
  CURL *c = curl_easy_init();
  if (!c) {
    fclose(f);
    remove(tmp);
    return false;
  }
  curl_easy_setopt(c, CURLOPT_URL, url);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION,
                   (curl_write_callback)x11_http_write_file_cb);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, (void *)f);
  http_setopt_common(c);
  CURLcode res = curl_easy_perform(c);
  curl_easy_cleanup(c);
  fclose(f);
  if (res != CURLE_OK) {
    log_warn("http download failed: %s", curl_easy_strerror(res));
    remove(tmp);
    return false;
  }
  remove(save_path);
  if (rename(tmp, save_path) != 0) {
    log_warn("http download rename failed for %s", save_path);
    remove(tmp);
    return false;
  }
  return true;
}

bool Platform_FontInitialized(FontHandle font) { return font.xfont != NULL; }

PlTimer Platform_StartTimer(void) {
  PlTimer t = {0};
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  t.ticks = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
  return t;
}

double Platform_EndTimer(PlTimer t) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t now = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
  if (now < t.ticks)
    return 0.0;
  return (double)(now - t.ticks) / 1000000.0;
}

void *Platform_ThreadCreate(void *(*fn)(void *), void *arg) {
  if (!fn)
    return NULL;
  pthread_t *t = malloc(sizeof(*t));
  if (!t)
    return NULL;
  if (pthread_create(t, NULL, fn, arg) != 0) {
    free(t);
    return NULL;
  }
  return t;
}

void Platform_ThreadJoin(void *thread) {
  if (!thread) {
    return;
  }
  pthread_t *t = (pthread_t *)thread;
  pthread_join(*t, NULL);
  free(t);
}

void *Platform_MutexCreate(void) {
  pthread_mutex_t *m = malloc(sizeof(*m));
  if (!m)
    return NULL;
  if (pthread_mutex_init(m, NULL) != 0) {
    free(m);
    return NULL;
  }
  return m;
}

void Platform_MutexDestroy(void *mutex) {
  if (!mutex) {
    return;
  }
  pthread_mutex_t *m = (pthread_mutex_t *)mutex;
  pthread_mutex_destroy(m);
  free(m);
}

void Platform_MutexLock(void *mutex) {
  if (!mutex) {
    return;
  }
  pthread_mutex_lock((pthread_mutex_t *)mutex);
}

void Platform_MutexUnlock(void *mutex) {
  if (!mutex) {
    return;
  }
  pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

bool Platform_MutexTryLock(void *mutex) {
  if (!mutex)
    return false;
  return pthread_mutex_trylock((pthread_mutex_t *)mutex) == 0;
}

void Platform_InvalidatePrompt(void) {
  Display *dpy = x11_get_display();
  Window win = (Window)SkaldContext_Get()->window.hPopupWnd.xid;
  if (dpy && win) {
    XClearArea(dpy, win, 0, 0, 0, 0, True);
    XFlush(dpy);
  }
}

bool Platform_InjectKey(int vk, int down) { return wm_inject_key(vk, down); }

static bool x11_http_thread_start(void *(*fn)(void *), void *ud) {
  pthread_t thread;
  int err = pthread_create(&thread, NULL, fn, ud);
  if (err != 0)
    return false;

  pthread_detach(thread);
  return true;
}

void Platform_HttpGetAsync(const char *url, PlatformHttpCallback cb,
                           void *userdata) {
  if (!url || !url[0] || !cb)
    return;
  HttpAsyncJob *job = (HttpAsyncJob *)calloc(1, sizeof(*job));
  if (!job)
    return;
  job->url = strdup(url);
  job->cb = cb;
  job->userdata = userdata;
  if (!job->url) {
    free(job);
    return;
  }
  job->owner[0] = '\0';
  owner_resolve_for_func((void *)cb, job->owner, sizeof(job->owner));
  thread_registry_add(job, job->owner);

  if (!x11_http_thread_start(http_async_worker, job)) {
    thread_registry_finish(job);
    free(job->url);
    free(job);
    log_warn("http async thread create failed");
  }
}

bool Platform_GetMousePos(int *out_x, int *out_y) {
  int mx = 0, my = 0;
  if (!wm_get_mouse_pos(&mx, &my))
    return false;
  if (out_x)
    *out_x = mx;
  if (out_y)
    *out_y = my;
  return true;
}

static bool x11_lookup_is_exec(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode) &&
         access(path, X_OK) == 0;
}

static bool x11_lookup_ci_eq(const char *a, const char *b) {
  for (;;) {
    unsigned char ca = (unsigned char)*a, cb = (unsigned char)*b;
    if (ca >= 'A' && ca <= 'Z')
      ca += 32;
    if (cb >= 'A' && cb <= 'Z')
      cb += 32;
    if (ca != cb)
      return false;
    if (ca == '\0')
      return true;
    a++;
    b++;
  }
}

static bool x11_lookup_ci_ends_with(const char *s, const char *suffix) {
  size_t ns = strlen(s), nf = strlen(suffix);
  if (nf > ns)
    return false;
  return x11_lookup_ci_eq(s + ns - nf, suffix);
}

static char *x11_lookup_try_dir(const char *dir, const char *name) {
  if (!dir || !dir[0])
    return NULL;
  char cand[4096];
  int rc = snprintf(cand, sizeof(cand), "%s/%s", dir, name);
  if (rc < 0 || (size_t)rc >= sizeof(cand))
    return NULL;
  if (x11_lookup_is_exec(cand))
    return strdup(cand);
  return NULL;
}

static char *x11_lookup_find_in_list(const char *list, const char *name) {
  const char *p = list;
  while (p && *p) {
    const char *end = strchr(p, ':');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len > 0 && len < 4096) {
      char dirbuf[4096];
      memcpy(dirbuf, p, len);
      dirbuf[len] = '\0';
      char *hit = x11_lookup_try_dir(dirbuf, name);
      if (hit)
        return hit;
    }
    if (!end)
      break;
    p = end + 1;
  }
  return NULL;
}

static char *x11_lookup_find_in_path(const char *name, const char *home) {
  const char *path = getenv("PATH");
  if (path && path[0]) {
    char *hit = x11_lookup_find_in_list(path, name);
    if (hit)
      return hit;
  }
  const char *extra[] = {"/usr/local/bin", "/usr/bin",   "/bin",
                         "/usr/sbin",      "/sbin",
                         "/snap/bin",
                         "/var/lib/flatpak/exports/bin", NULL};
  for (size_t i = 0; extra[i]; i++) {
    char *hit = x11_lookup_try_dir(extra[i], name);
    if (hit)
      return hit;
  }
  if (home && home[0]) {
    char buf[1024];
    const char *home_extra[] = {"/.local/bin", "/bin",
                                "/.local/share/flatpak/exports/bin", NULL};
    for (size_t i = 0; home_extra[i]; i++) {
      int rc = snprintf(buf, sizeof(buf), "%s%s", home, home_extra[i]);
      if (rc < 0 || (size_t)rc >= sizeof(buf))
        continue;
      char *hit = x11_lookup_try_dir(buf, name);
      if (hit)
        return hit;
    }
  }
  return NULL;
}

static char *x11_lookup_resolve_exec(const char *desktop_file,
                                     const char *home) {
  FILE *f = fopen(desktop_file, "r");
  if (!f)
    return NULL;
  char line[8192];
  char *result = NULL;
  while (!result && fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t')
      p++;
    if (strncmp(p, "Exec=", 5) != 0)
      continue;
    p += 5;
    while (*p == ' ' || *p == '\t')
      p++;
    char *eol = p + strlen(p);
    while (eol > p && (eol[-1] == '\n' || eol[-1] == '\r'))
      *--eol = '\0';
    char tokens[16][512];
    int ntok = 0;
    for (char *t = p; t && *t && ntok < 16;) {
      while (*t == ' ' || *t == '\t')
        t++;
      if (!*t)
        break;
      bool quoted = false;
      if (*t == '"') {
        quoted = true;
        t++;
      }
      char *dst = tokens[ntok];
      size_t cap = sizeof(tokens[ntok]) - 1;
      size_t o = 0;
      while (*t && o < cap) {
        if (quoted) {
          if (*t == '"') {
            t++;
            break;
          }
          dst[o++] = *t++;
        } else {
          if (*t == ' ' || *t == '\t')
            break;
          dst[o++] = *t++;
        }
      }
      dst[o] = '\0';
      ntok++;
    }
    int i = 0;
    if (ntok > 0 && x11_lookup_ci_eq(tokens[0], "env")) {
      i = 1;
      while (i < ntok && strchr(tokens[i], '=') != NULL)
        i++;
    }
    if (i >= ntok)
      continue;
    char *prog = tokens[i];
    if (!prog[0] || prog[0] == '%')
      continue;
    if (strchr(prog, '/')) {
      if (x11_lookup_is_exec(prog))
        result = strdup(prog);
      continue;
    }
    result = x11_lookup_find_in_path(prog, home);
  }
  fclose(f);
  return result;
}

static char *x11_lookup_scan_app_dir(const char *apps_dir, const char *name,
                                     const char *home) {
  char desktop[4096];
  snprintf(desktop, sizeof(desktop), "%s/%s.desktop", apps_dir, name);
  struct stat st;
  if (stat(desktop, &st) == 0 && S_ISREG(st.st_mode)) {
    char *hit = x11_lookup_resolve_exec(desktop, home);
    if (hit)
      return hit;
  }
  char *hit = NULL;
  PlDirIter *it = Platform_DirOpen(apps_dir);
  if (!it)
    return NULL;
  char entry[256];
  bool is_dir = false;
  while (!hit && Platform_DirNext(it, entry, sizeof(entry), &is_dir)) {
    if (is_dir || !x11_lookup_ci_ends_with(entry, ".desktop"))
      continue;
    char stem[256];
    snprintf(stem, sizeof(stem), "%.*s", (int)(strlen(entry) - 8), entry);
    char full[4096];
    snprintf(full, sizeof(full), "%s/%s", apps_dir, entry);
    if (x11_lookup_ci_eq(stem, name)) {
      hit = x11_lookup_resolve_exec(full, home);
      continue;
    }
    hit = x11_lookup_resolve_exec(full, home);
    if (hit) {
      char base[512];
      const char *slash = strrchr(hit, '/');
      snprintf(base, sizeof(base), "%s", slash ? slash + 1 : hit);
      if (!x11_lookup_ci_eq(base, name)) {
        free(hit);
        hit = NULL;
      }
    }
  }
  Platform_DirClose(it);
  return hit;
}

static char *x11_lookup_find_in_apps_db(const char *name, const char *home) {
  const char *data_home = getenv("XDG_DATA_HOME");
  char dh[1024];
  char *hit = NULL;
  if (!data_home || !data_home[0]) {
    if (home && home[0]) {
      snprintf(dh, sizeof(dh), "%s/.local/share", home);
      data_home = dh;
    }
  }
  if (data_home && data_home[0]) {
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/applications", data_home);
    hit = x11_lookup_scan_app_dir(dir, name, home);
    if (hit)
      return hit;
  }
  const char *data_dirs = getenv("XDG_DATA_DIRS");
  if (!data_dirs || !data_dirs[0])
    data_dirs = "/usr/local/share:/usr/share";
  const char *p = data_dirs;
  while (p && *p && !hit) {
    const char *end = strchr(p, ':');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len > 0 && len < sizeof(dh)) {
      char dirbuf[1024];
      memcpy(dirbuf, p, len);
      dirbuf[len] = '\0';
      char full[1024];
      int rc = snprintf(full, sizeof(full), "%s/applications", dirbuf);
      if (rc > 0 && (size_t)rc < sizeof(full))
        hit = x11_lookup_scan_app_dir(full, name, home);
    }
    if (!end)
      break;
    p = end + 1;
  }
  if (hit)
    return hit;
  const char *flatpak[] = {"/var/lib/flatpak/exports/share/applications",
                           "/var/lib/snapd/desktop/applications", NULL};
  for (size_t i = 0; flatpak[i] && !hit; i++)
    hit = x11_lookup_scan_app_dir(flatpak[i], name, home);
  if (hit || !home || !home[0])
    return hit;
  snprintf(dh, sizeof(dh), "%s/.local/share/flatpak/exports/share/applications",
           home);
  return x11_lookup_scan_app_dir(dh, name, home);
}

static char *x11_lookup_first_line(char *out) {
  char *start = out;
  while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
    start++;
  char *end = start;
  while (*end && *end != '\n' && *end != '\r')
    end++;
  while (end > start && (end[-1] == ' ' || end[-1] == '\t'))
    end--;
  *end = '\0';
  return start;
}

char *Platform_FindExecutable(const char *name) {
  if (!name || !name[0])
    return NULL;

  const char *home = Platform_GetRealHome();

  if (strchr(name, '/')) {
    char expanded[4096];
    if (!expand_path(name, expanded, sizeof(expanded)))
      return NULL;
    if (x11_lookup_is_exec(expanded))
      return strdup(expanded);
    return NULL;
  }

  char *hit = x11_lookup_find_in_path(name, home);
  if (hit)
    return hit;

  hit = x11_lookup_find_in_apps_db(name, home);
  if (hit)
    return hit;

  int status = 0;
  char *out = Platform_RunArgvCapture((const char *const[]){"which", name, NULL},
                                      NULL, &status);
  if (!out)
    return NULL;
  char *line = x11_lookup_first_line(out);
  if (line[0] && x11_lookup_is_exec(line))
    hit = strdup(line);
  free(out);
  return hit;
}
