#ifndef HELPERS_WIN32_MISC_H
#define HELPERS_WIN32_MISC_H

#include <stdbool.h>
#include <windows.h>

bool win32_default_browser_exe(char *out, size_t cap);
bool win32_launch_default_browser_blank(void);

#endif
