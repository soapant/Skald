#ifndef HELPERS_X11_MISC_H
#define HELPERS_X11_MISC_H

#include <sys/types.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "helpers/helpers_fs.h"
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <dirent.h>
#include <time.h>
#include <linux/input-event-codes.h>
#include <stdint.h>
#include <strings.h>
#include "platform/platform.h"

const char *x11_get_session_display(void);
const char *x11_get_session_xauthority(void);
const char *x11_get_runtime_dir(void);

uid_t x11_probe_session_uid(void);
bool x11_cmd_exists(const char *cmd);
bool x11_launch_browser(const char *browser_path, const char *url,
                        bool new_window);

#endif
