#include "helpers/helpers_text.h"
#include <windows.h>
#include "helpers/helpers_fs.h"
#include <winreg.h>
#include <ctype.h>
#include "config_values.h"
#include "core/types.h"
#include "deps/log.h"
#include <tlhelp32.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "platform/platform.h"
#include "helpers/win32/misc_win32.h"
#include <time.h>

bool win32_default_browser_exe(char *out, size_t cap) {

  HKEY key = NULL;
  if (RegOpenKeyExA(HKEY_CURRENT_USER,
                    "Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAsso"
                    "ciations\\http\\UserChoice",
                    0, KEY_READ, &key) != ERROR_SUCCESS)
    return false;

  char prog[128] = {0};
  DWORD sz = sizeof(prog) - 1;
  LONG r = RegQueryValueExA(key, "ProgId", NULL, NULL, (LPBYTE)prog, &sz);
  RegCloseKey(key);
  if (r != ERROR_SUCCESS || !prog[0])
    return false;

  char sub[256];
  snprintf(sub, sizeof(sub), "%s\\shell\\open\\command", prog);
  if (RegOpenKeyExA(HKEY_CLASSES_ROOT, sub, 0, KEY_READ, &key) != ERROR_SUCCESS)
    return false;

  char cmd[1024] = {0};
  sz = sizeof(cmd) - 1;
  r = RegQueryValueExA(key, NULL, NULL, NULL, (LPBYTE)cmd, &sz);
  RegCloseKey(key);
  if (r != ERROR_SUCCESS || !cmd[0])
    return false;

  const char *p = cmd;
  size_t len = 0;
  if (*p == '"') {
    p++;
    while (p[len] && p[len] != '"' && len < cap - 1)
      len++;
  } else {
    while (p[len] && !isspace((unsigned char)p[len]) && len < cap - 1)
      len++;
  }
  if (len == 0)
    return false;
  memcpy(out, p, len);
  out[len] = '\0';
  return out[0] != '\0';
}

bool win32_launch_default_browser_blank(void) {
  char exe[512];
  if (!win32_default_browser_exe(exe, sizeof(exe))) {
    log_info("Platform_LaunchBrowserAtUrl could not resolve default browser");
    return false;
  }
  log_info("Platform_LaunchBrowserAtUrl default browser exe='%s'", exe);
  HINSTANCE r = ShellExecuteA(NULL, "open", exe, NULL, NULL, SW_SHOWNORMAL);
  if ((intptr_t)r <= 32) {
    log_info("Platform_LaunchBrowserAtUrl ShellExecute failed for '%s'", exe);
    return false;
  }
  return true;
}
