#ifndef HELPERS_X11_MISC_H
#define HELPERS_X11_MISC_H

#include <sys/types.h>
#include <stdbool.h>

const char *x11_get_session_display(void);
const char *x11_get_session_xauthority(void);
const char *x11_get_runtime_dir(void);

uid_t x11_probe_session_uid(void);
bool x11_cmd_exists(const char *cmd);
bool x11_launch_browser(const char *browser_path, const char *url,
                        bool new_window);

#endif
