#include "platform/x11/env_helpers_x11.h"
#include "core/platform.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

void Env_GetSessionEnv(X11SessionEnv* out) {
    if (!out) return;
    out->real_user = Platform_GetRealUser();
    out->display = Platform_GetSessionDisplay();
    out->xauthority = Platform_GetSessionXAuthority();
    out->runtime_dir = Platform_GetRuntimeDir();
    out->wayland_display = getenv("WAYLAND_DISPLAY");
}

void Env_BuildEnvStrings(const X11SessionEnv* env, char disp_env[256], char xauth_env[512], char runtime_env[512], char wayland_env[512]) {
    if (disp_env) snprintf(disp_env, 256, "DISPLAY=%s", env && env->display ? env->display : "");
    if (xauth_env) snprintf(xauth_env, 512, "XAUTHORITY=%s", env && env->xauthority ? env->xauthority : "");
    if (runtime_env) snprintf(runtime_env, 512, "XDG_RUNTIME_DIR=%s", env && env->runtime_dir ? env->runtime_dir : "");
    if (wayland_env) snprintf(wayland_env, 512, "WAYLAND_DISPLAY=%s", env && env->wayland_display ? env->wayland_display : "");
}

void Env_ApplyToCurrentProcess(const X11SessionEnv* env) {
    if (!env) return;
    if (env->display) setenv("DISPLAY", env->display, 1);
    if (env->xauthority) setenv("XAUTHORITY", env->xauthority, 1);
    if (env->runtime_dir) setenv("XDG_RUNTIME_DIR", env->runtime_dir, 1);
    if (env->wayland_display && env->wayland_display[0]) setenv("WAYLAND_DISPLAY", env->wayland_display, 1);
}

bool Env_IsRunningAsRoot(void) {
    return Platform_IsRunningAsRoot();
}

bool Env_DaemonExec(char* const argv[]) {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        setsid();
        pid_t pid2 = fork();
        if (pid2 < 0) _exit(1);
        if (pid2 > 0) _exit(0);
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > 2) close(fd);
        }
        execvp(argv[0], argv);
        _exit(127);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    return true;
}

bool Env_DaemonExecWithEnv(char* const argv[], const X11SessionEnv* env) {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        setsid();
        pid_t pid2 = fork();
        if (pid2 < 0) _exit(1);
        if (pid2 > 0) _exit(0);
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > 2) close(fd);
        }
        if (env) Env_ApplyToCurrentProcess(env);
        execvp(argv[0], argv);
        _exit(127);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    return true;
}


