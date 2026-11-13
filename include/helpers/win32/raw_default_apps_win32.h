#ifndef HELPERS_WIN32_DEFAULT_APPS_H
#define HELPERS_WIN32_DEFAULT_APPS_H

#include <stddef.h>
#include <stdbool.h>
#include <windows.h>

bool win32_default_browser_exe(char *out, size_t cap);
bool win32_launch_default_browser_blank(void);

const char *win32_terminal_name_from_guid(const char *console_guid,
                                          const char *terminal_guid);
void win32_default_apps_free_cache(void);
const char *win32_sm_lookup(int slot, const char *const possibilities[],
                            size_t count);
#define SM_SLOTS 8

#endif
