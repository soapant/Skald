#include "helpers/x11/env_helpers_x11.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>

#include "platform/platform.h"
#include "helpers/x11/raw_misc_x11.h"
#include "deps/log.h"

static bool env_sock_live(const char *path) {
  struct stat st;
  return path && path[0] && stat(path, &st) == 0;
}

static const char *env_or_null(const char *value) {
  return value && value[0] ? value : NULL;
}

const char *x11_env_home_dir(void) { return env_or_null(getenv("HOME")); }

const char *x11_env_xdg_runtime_dir(void) {
  return env_or_null(getenv("XDG_RUNTIME_DIR"));
}

bool x11_env_is_wayland(void) {
  return env_or_null(getenv("WAYLAND_DISPLAY")) != NULL;
}

bool x11_env_is_compositor(void) {
  return getenv("WAYLAND_DISPLAY") != NULL || getenv("SWAYSOCK") != NULL ||
         getenv("I3SOCK") != NULL ||
         getenv("HYPRLAND_INSTANCE_SIGNATURE") != NULL ||
         getenv("NIRI_SOCKET") != NULL || getenv("KDE_FULL_SESSION") != NULL;
}

void x11_env_get_session_env(X11SessionEnv *out) {
  if (!out)
    return;
  out->real_user = Platform_GetRealUser();
  out->real_home = Platform_GetRealHome();
  out->display = x11_get_session_display();
  out->xauthority = x11_get_session_xauthority();
  out->runtime_dir = x11_get_runtime_dir();
  out->wayland_display = getenv("WAYLAND_DISPLAY");
  if (out->wayland_display && out->wayland_display[0]) {
    char sock[512];
    sock[0] = '\0';
    const char *rtd = (out->runtime_dir && out->runtime_dir[0])
                          ? out->runtime_dir
                          : x11_env_xdg_runtime_dir();
    if (out->wayland_display[0] == '/')
      snprintf(sock, sizeof(sock), "%s", out->wayland_display);
    else if (rtd && rtd[0])
      snprintf(sock, sizeof(sock), "%s/%s", rtd, out->wayland_display);
    if (!env_sock_live(sock)) {
      log_info("dropping dead WAYLAND_DISPLAY='%s'", out->wayland_display);
      out->wayland_display = NULL;
    }
  }
}

void x11_env_build_env_strings(FnX11EnvBuildEnvStringsSpec *argspec) {
  if (argspec->f_disp_env)
    snprintf(argspec->f_disp_env, 256, "DISPLAY=%s",
             argspec->f_env && argspec->f_env->display ? argspec->f_env->display : "");
  if (argspec->f_xauth_env)
    snprintf(argspec->f_xauth_env, 512, "XAUTHORITY=%s",
             argspec->f_env && argspec->f_env->xauthority ? argspec->f_env->xauthority : "");
  if (argspec->f_runtime_env)
    snprintf(argspec->f_runtime_env, 512, "XDG_RUNTIME_DIR=%s",
             argspec->f_env && argspec->f_env->runtime_dir ? argspec->f_env->runtime_dir : "");
  if (argspec->f_wayland_env)
    snprintf(argspec->f_wayland_env, 512, "WAYLAND_DISPLAY=%s",
             argspec->f_env && argspec->f_env->wayland_display ? argspec->f_env->wayland_display : "");
}

void x11_env_apply_to_current_process(const X11SessionEnv *env) {
  if (env->display)
    setenv("DISPLAY", env->display, 1);
  if (env->xauthority)
    setenv("XAUTHORITY", env->xauthority, 1);
  if (env->runtime_dir)
    setenv("XDG_RUNTIME_DIR", env->runtime_dir, 1);
  if (env->wayland_display && env->wayland_display[0])
    setenv("WAYLAND_DISPLAY", env->wayland_display, 1);
  else
    unsetenv("WAYLAND_DISPLAY");
  if (env->real_user && env->real_user[0])
    setenv("USER", env->real_user, 1);
  if (env->real_user && env->real_user[0])
    setenv("LOGNAME", env->real_user, 1);
  if (env->real_home && env->real_home[0])
    setenv("HOME", env->real_home, 1);
  const char *cfg = getenv("XDG_CONFIG_HOME");
  if (!cfg || !cfg[0]) {
    if (env->real_home && env->real_home[0]) {
      char buf[1024];
      snprintf(buf, sizeof(buf), "%s/.config", env->real_home);
      setenv("XDG_CONFIG_HOME", buf, 1);
    }
  }
}

bool x11_env_is_running_as_root(void) { return Platform_IsRunningAsRoot(); }

pid_t x11_env_detach_fork(void) {
  pid_t pid = fork();
  if (pid < 0)
    return -1;
  if (pid == 0) {
    setsid();
    pid_t pid2 = fork();
    if (pid2 < 0)
      _exit(1);
    if (pid2 > 0)
      _exit(0);
    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
      dup2(fd, STDIN_FILENO);
      dup2(fd, STDOUT_FILENO);
      dup2(fd, STDERR_FILENO);
      if (fd > 2)
        close(fd);
    }
    return 0;
  }
  int st = 0;
  waitpid(pid, &st, 0);
  return pid;
}

bool x11_env_daemon_exec(char *const argv[]) {
  pid_t p = x11_env_detach_fork();
  if (p < 0)
    return false;
  if (p == 0) {
    execvp(argv[0], argv);
    _exit(127);
  }
  return true;
}

bool x11_env_daemon_exec_with_env(char *const argv[],
                                  const X11SessionEnv *env) {
  pid_t p = x11_env_detach_fork();
  if (p < 0)
    return false;
  if (p == 0) {
    if (env)
      x11_env_apply_to_current_process(env);
    execvp(argv[0], argv);
    _exit(127);
  }
  return true;
}

char *x11_paste_wtype(const char *content) {
  if (!content) {
    log_error("content is NULL");
    return NULL;
  }
  const char *argv[] = {"wtype", content, NULL};
  int status = 0;
  char *out = Platform_RunArgvCapture(argv, NULL, &status);
  if (status == 127) {
    free(out);
    out = NULL;
  }
  if (!out)
    log_error("paste via wtype failed");
  return out;
}
