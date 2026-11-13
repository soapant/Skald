#ifndef HELPERS_X11_DEFAULT_APPS_H
#define HELPERS_X11_DEFAULT_APPS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include "platform/platform.h"
#include "helpers/x11/env_helpers_x11.h"
#include "helpers/helpers_text.h"
#include "core/types.h"

void x11_default_apps_free_cache(void);
const char *x11_find_terminal_in_desktop_files(const char *const *names,
                                               size_t count);
const char *x11_get_default_terminal_wayland(void);
const char *x11_get_default_terminal(void);
int x11_default_apps_exec_token(const char *line, char *out, size_t cap);

#endif
