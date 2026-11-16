#define _GNU_SOURCE
#include "helpers/x11/raw_misc_x11.h"
#include "helpers/x11/env_helpers_x11.h"
#include "deps/log.h"
#include "wm/wm.h"
#include "helpers/x11/clipboard_x11.h"
#include "helpers/x11/clipboard_x11.h"
#include "helpers/helpers_text.h"
#include "config_values.h"
#include "core/types.h"
#include "core/types/keys.h"

uid_t x11_probe_session_uid(void) {

  DIR *d = opendir("/tmp/.X11-unix");
  if (!d)
    return (uid_t)-1;
  const struct dirent *e;
  while ((e = readdir(d)) != NULL) {
    if (e->d_name[0] != 'X')
      continue;
    char path[512];
    snprintf(path, sizeof(path), "/tmp/.X11-unix/%s", e->d_name);
    struct stat st;
    if (stat(path, &st) == 0 && st.st_uid != 0) {
      closedir(d);
      return st.st_uid;
    }
  }
  closedir(d);
  return (uid_t)-1;
}

const char *x11_get_session_display(void) {

  static char buf[64] = {0};
  const char *env = getenv("DISPLAY");
  if (env && env[0]) {
    snprintf(buf, sizeof(buf), "%s", env);
    return buf;
  }
  DIR *d = opendir("/tmp/.X11-unix");
  if (d) {
    const struct dirent *e;
    while ((e = readdir(d)) != NULL) {
      if (e->d_name[0] != 'X')
        continue;
      char *end = NULL;
      long n = strtol(e->d_name + 1, &end, 10);
      if (end == e->d_name + 1 || *end != '\0')
        continue;
      char path[512];
      snprintf(path, sizeof(path), "/tmp/.X11-unix/%s", e->d_name);
      struct stat st;
      if (stat(path, &st) != 0)
        continue;

      if (st.st_uid == geteuid() || st.st_uid != 0) {
        snprintf(buf, sizeof(buf), ":%ld", n);
        break;
      }
    }
    closedir(d);
  }
  log_info("x11_get_session_display completed");
  return buf[0] ? buf : ":0";
}

const char *x11_get_session_xauthority(void) {

  static char buf[512] = {0};
  struct stat xst;
  const char *xa = getenv("XAUTHORITY");
  if (xa && xa[0] && stat(xa, &xst) == 0 && xst.st_size > 0 &&
      access(xa, R_OK) == 0) {
    snprintf(buf, sizeof(buf), "%s", xa);
    return buf;
  }
  char rtd[256];
  const char *rt = x11_env_xdg_runtime_dir();
  if (rt && rt[0])
    snprintf(rtd, sizeof(rtd), "%s", rt);
  else
    snprintf(rtd, sizeof(rtd), "/run/user/%d", (int)getuid());
  DIR *d = opendir(rtd);
  if (d) {
    const struct dirent *e;
    time_t best_mtime = 0;
    char best[512] = {0};
    while ((e = readdir(d)) != NULL) {
      if (strncmp(e->d_name, "xauth_", 6) != 0)
        continue;
      char path[512];
      snprintf(path, sizeof(path), "%s/%s", rtd, e->d_name);
      struct stat st;
      if (stat(path, &st) != 0)
        continue;
      if (st.st_size == 0 || access(path, R_OK) != 0)
        continue;
      if (st.st_mtime >= best_mtime) {
        best_mtime = st.st_mtime;
        snprintf(best, sizeof(best), "%s", path);
      }
    }
    closedir(d);
    if (best[0]) {
      snprintf(buf, sizeof(buf), "%s", best);
      return buf;
    }
  }
  snprintf(buf, sizeof(buf), "%s/.Xauthority", Platform_GetRealHome());
  if (stat(buf, &xst) == 0 && xst.st_size > 0 && access(buf, R_OK) == 0)
    return buf;
  buf[0] = '\0';
  log_info("x11_get_session_xauthority completed");
  return "";
}

const char *x11_get_runtime_dir(void) {

  static char buf[256] = {0};
  const char *rt = x11_env_xdg_runtime_dir();
  if (rt && rt[0]) {
    snprintf(buf, sizeof(buf), "%s", rt);
    return buf;
  }
  const char *u = Platform_GetRealUser();
  const struct passwd *pw = u ? getpwnam(u) : NULL;
  if (pw) {
    snprintf(buf, sizeof(buf), "/run/user/%d", (int)pw->pw_uid);
    return buf;
  }
  snprintf(buf, sizeof(buf), "/run/user/%d", (int)getuid());
  log_info("x11_get_runtime_dir completed");
  return buf[0] ? buf : "/run/user/1000";
}

bool x11_cmd_exists(const char *cmd) {

  if (!cmd || !cmd[0])
    return false;
  if (strchr(cmd, '/'))
    return access(cmd, X_OK) == 0;
  const char *path = getenv("PATH");
  if (!path || !path[0])
    path = "/usr/bin:/bin:/usr/local/bin";
  StringList dirs = String_Split(path, ":");
  for (size_t i = 0; i < dirs.count; i++) {
    const char *dir = dirs.items[i];
    if (!dir || !dir[0])
      continue;
    char full[4352];
    snprintf(full, sizeof(full), "%s/%s", dir, cmd);
    if (access(full, X_OK) == 0) {
      string_list_free(&dirs);
      return true;
    }
  }
  string_list_free(&dirs);
  return false;
}

bool x11_launch_browser(const char *browser_path, const char *url,
                        bool new_window) {
  if (!browser_path || !browser_path[0] || !url || !url[0])
    return false;
  char command[2048];
  const char *real_user = Platform_GetRealUser();
  const char *display = getenv("DISPLAY");
  if (!display)
    display = ":0";
  const char *xauthority = getenv("XAUTHORITY");
  if (!xauthority)
    xauthority = "";
  const char *env_runtime = x11_env_xdg_runtime_dir();
  char runtime_dir[256];
  if (env_runtime)
    snprintf(runtime_dir, sizeof(runtime_dir), "%s", env_runtime);
  else
    snprintf(runtime_dir, sizeof(runtime_dir), "/run/user/1000");
  if (Platform_IsRunningAsRoot()) {
    if (new_window)
      snprintf(command, sizeof(command),
               "sudo -u %s DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s %s "
               "--new-window \"%s\" > /dev/null 2>&1 &",
               real_user, display, xauthority, runtime_dir, browser_path, url);
    else
      snprintf(command, sizeof(command),
               "sudo -u %s DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s %s "
               "\"%s\" > /dev/null 2>&1 &",
               real_user, display, xauthority, runtime_dir, browser_path, url);
  } else {
    if (new_window)
      snprintf(command, sizeof(command),
               "DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s %s --new-window "
               "\"%s\" > /dev/null 2>&1 &",
               display, xauthority, runtime_dir, browser_path, url);
    else
      snprintf(command, sizeof(command),
               "DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s %s \"%s\" > "
               "/dev/null 2>&1 &",
               display, xauthority, runtime_dir, browser_path, url);
  }
  return Platform_RunTerminalCommand(command, false);
}
