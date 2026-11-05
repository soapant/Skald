#include "platform/x11/env_helpers_x11.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>

#include "platform/platform.h"
#include "deps/log.h"

static bool env_sock_live(const char *path) {
  struct stat st;
  return path && path[0] && stat(path, &st) == 0;
}

static const char *env_or_null(const char *value) {
  return value && value[0] ? value : NULL;
}

const char *Env_HomeDir(void) { return env_or_null(getenv("HOME")); }

const char *Env_XdgRuntimeDir(void) {
  return env_or_null(getenv("XDG_RUNTIME_DIR"));
}

bool Env_IsWayland(void) { return env_or_null(getenv("WAYLAND_DISPLAY")) != NULL; }

bool Env_IsCompositor(void) {
  return getenv("WAYLAND_DISPLAY") != NULL ||
         getenv("SWAYSOCK") != NULL || getenv("I3SOCK") != NULL ||
         getenv("HYPRLAND_INSTANCE_SIGNATURE") != NULL ||
         getenv("NIRI_SOCKET") != NULL || getenv("KDE_FULL_SESSION") != NULL;
}

void Env_GetSessionEnv(X11SessionEnv *out) {
  if (!out)
    return;
  out->real_user = Platform_GetRealUser();
  out->real_home = Platform_GetRealHome();
  out->display = Platform_GetSessionDisplay();
  out->xauthority = Platform_GetSessionXAuthority();
  out->runtime_dir = Platform_GetRuntimeDir();
  out->wayland_display = getenv("WAYLAND_DISPLAY");
  if (out->wayland_display && out->wayland_display[0]) {
    char sock[512];
    sock[0] = '\0';
    const char *rtd = (out->runtime_dir && out->runtime_dir[0])
                          ? out->runtime_dir
                          : Env_XdgRuntimeDir();
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

void Env_BuildEnvStrings(const X11SessionEnv *env, char disp_env[256],
                         char xauth_env[512], char runtime_env[512],
                         char wayland_env[512]) {
  if (disp_env)
    snprintf(disp_env, 256, "DISPLAY=%s",
             env && env->display ? env->display : "");
  if (xauth_env)
    snprintf(xauth_env, 512, "XAUTHORITY=%s",
             env && env->xauthority ? env->xauthority : "");
  if (runtime_env)
    snprintf(runtime_env, 512, "XDG_RUNTIME_DIR=%s",
             env && env->runtime_dir ? env->runtime_dir : "");
  if (wayland_env)
    snprintf(wayland_env, 512, "WAYLAND_DISPLAY=%s",
             env && env->wayland_display ? env->wayland_display : "");
}

void Env_ApplyToCurrentProcess(const X11SessionEnv *env) {
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

bool Env_IsRunningAsRoot(void) { return Platform_IsRunningAsRoot(); }

bool Env_DaemonExec(char *const argv[]) {
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
      dup2(fd, STDIN_FILENO);
      dup2(fd, STDOUT_FILENO);
      dup2(fd, STDERR_FILENO);
      if (fd > 2)
        close(fd);
    }
    execvp(argv[0], argv);
    _exit(127);
  }
  int st = 0;
  waitpid(pid, &st, 0);
  return true;
}

bool Env_DaemonExecWithEnv(char *const argv[], const X11SessionEnv *env) {
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
      dup2(fd, STDIN_FILENO);
      dup2(fd, STDOUT_FILENO);
      dup2(fd, STDERR_FILENO);
      if (fd > 2)
        close(fd);
    }
    if (env)
      Env_ApplyToCurrentProcess(env);
    execvp(argv[0], argv);
    _exit(127);
  }
  int st = 0;
  waitpid(pid, &st, 0);
  return true;
}
