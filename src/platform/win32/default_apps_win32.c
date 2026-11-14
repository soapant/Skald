#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "core/types/file.h"
#include "platform/platform.h"
#include "helpers/helpers_text.h"
#include "config.h"
#include "helpers/win32/raw_default_apps_win32.h"

static char default_apps_win32[DEFAULT_APP_COUNT][256];
#define SM_SLOTS 8

static const char *get_default_terminal(void) {
  const char *possible_terminals[] = {"Alacritty",
                                      "WezTerm",
                                      "Tabby",
                                      "Warp",
                                      "Ghostty",
                                      "Hyper",
                                      "Yaw",
                                      "Wave Terminal",
                                      "Fluent Terminal",
                                      "Rio",
                                      "Cmder",
                                      "ConEmu",
                                      "PuTTY",
                                      "MobaXterm",
                                      "Git Bash Terminal (mintty)",
                                      "SecureCRT",
                                      "Xshell",
                                      "ZOC Terminal",
                                      "KittyY",
                                      "PowerShell (Modern pwsh)",
                                      "Windows PowerShell "};

  const char *found =
      win32_sm_lookup(0, possible_terminals, ARRAY_LEN(possible_terminals));
  return found ? found : "pwsh";
}

static const char *get_default_browser(void) {

  const char *possible_browsers[] = {"Google Chrome",
                                     "Mozilla Firefox",
                                     "Microsoft Edge",
                                     "Brave",
                                     "Opera",
                                     "Vivaldi",
                                     "Tor Browser",
                                     "Waterfox",
                                     "LibreWolf",
                                     "Arc",
                                     "Sidekick",
                                     "Pale Moon",
                                     "Internet Explorer",
                                     "Zen"};

  const char *found =
      win32_sm_lookup(1, possible_browsers, ARRAY_LEN(possible_browsers));
  return found ? found : "chrome";
}

static const char *win32_get_default_terminal(void) {

  int slot = DEFAULT_TERMINAL;
  default_apps_win32[slot][0] = '\0';

  HKEY hKey = NULL;
  if (RegOpenKeyExA(HKEY_CURRENT_USER, "Console\\%%Startup", 0, KEY_READ,
                    &hKey) != ERROR_SUCCESS) {
    snprintf(default_apps_win32[slot], sizeof(default_apps_win32[slot]),
             "Let Windows Decide");
    return default_apps_win32[slot];
  }

  char console_guid[256] = {0};
  char terminal_guid[256] = {0};
  DWORD size = 0;
  DWORD type = 0;

  size = (DWORD)sizeof(console_guid);
  RegQueryValueExA(hKey, "DelegationConsole", NULL, &type, (BYTE *)console_guid,
                   &size);

  size = (DWORD)sizeof(terminal_guid);
  RegQueryValueExA(hKey, "DelegationTerminal", NULL, &type,
                   (BYTE *)terminal_guid, &size);

  RegCloseKey(hKey);

  const char *name = win32_terminal_name_from_guid(console_guid, terminal_guid);
  if (!name) {
    name = get_default_terminal();
  }
  snprintf(default_apps_win32[slot], sizeof(default_apps_win32[slot]), "%s",
           name);
  return default_apps_win32[slot];
}

const char *Platform_GetDefaultApp(DefaultAppKind kind) {

  if (kind < 0 || kind >= DEFAULT_APP_COUNT)
    return "";

  const char *progid = NULL;
  const char *fallback_name = NULL;

  switch (kind) {
  case DEFAULT_BROWSER:
    fallback_name = get_default_browser();
    break;

  case DEFAULT_TERMINAL:
    return win32_get_default_terminal();

  default:
    default_apps_win32[kind][0] = '\0';
    return default_apps_win32[kind];
  }

  if (progid && progid[0] != '\0') {
    snprintf(default_apps_win32[kind], sizeof(default_apps_win32[kind]), "%s",
             progid);
  } else {
    snprintf(default_apps_win32[kind], sizeof(default_apps_win32[kind]), "%s",
             fallback_name);
  }
  return default_apps_win32[kind];
}

bool Platform_ResolveDesktopExec(const char *desktop_id, char *out,
                                 size_t out_size) {

  (void)desktop_id;
  (void)out;
  (void)out_size;
  return false;
}
