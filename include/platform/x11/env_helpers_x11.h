#ifndef ENV_HELPERS_X11_H
#define ENV_HELPERS_X11_H

#include <stdbool.h>

typedef struct {
    const char* real_user;
    const char* real_home;
    const char* display;
    const char* xauthority;
    const char* runtime_dir;
    const char* wayland_display;
} X11SessionEnv;

void Env_GetSessionEnv(X11SessionEnv* out);
void Env_BuildEnvStrings(const X11SessionEnv* env, char disp_env[256], char xauth_env[512], char runtime_env[512], char wayland_env[512]);
void Env_ApplyToCurrentProcess(const X11SessionEnv* env);
bool Env_IsRunningAsRoot(void);

bool Env_DaemonExec(char* const argv[]);
bool Env_DaemonExecWithEnv(char* const argv[], const X11SessionEnv* env);

#endif
