#ifndef ENV_HELPERS_X11_H
#define ENV_HELPERS_X11_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

typedef struct {
  const char *real_user;
  const char *real_home;
  const char *display;
  const char *xauthority;
  const char *runtime_dir;
  const char *wayland_display;
} X11SessionEnv;

void x11_env_get_session_env(X11SessionEnv *out);
const char *x11_env_home_dir(void);
const char *x11_env_xdg_runtime_dir(void);
bool x11_env_is_wayland(void);
bool x11_env_is_compositor(void);
void x11_env_build_env_strings(const X11SessionEnv *env, char disp_env[256],
                               char xauth_env[512], char runtime_env[512],
                               char wayland_env[512]);
void x11_env_apply_to_current_process(const X11SessionEnv *env);
bool x11_env_is_running_as_root(void);

pid_t x11_env_detach_fork(void);

bool x11_env_daemon_exec(char *const argv[]);
bool x11_env_daemon_exec_with_env(char *const argv[], const X11SessionEnv *env);
bool x11_desktop_parse(const char *desktop_path, char exec_line[1024],
                       char workdir[1024], bool *is_terminal);
void x11_desktop_clean_exec(char exec_line[1024]);
bool x11_exe_in_path(const char *name);
bool x11_resolve_desktop_exec(const char *exec_line, const char *desktop_dir,
                              const char *workdir, char *out, size_t out_size);
int x11_run_status(char *const argv[], const X11SessionEnv *env,
                   bool apply_env);

char *x11_paste_wtype(const char *content);

#endif
