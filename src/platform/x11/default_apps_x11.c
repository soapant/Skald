#include "helpers/x11/raw_default_apps_x11.h"

static char default_apps_x11[DEFAULT_APP_COUNT][256];

static const char *get_default_terminal(int slot) {

  default_apps_x11[slot][0] = '\0';

  const char *term = NULL;

  const char *env = getenv("TERMINAL");
  if (env && env[0]) {
    term = env;
  }

  if (!term) {
    const char *st = getenv("XDG_SESSION_TYPE");
    bool is_wayland =
        (st && strcmp(st, "wayland") == 0) || x11_env_is_compositor();
    term = is_wayland ? x11_get_default_terminal_wayland()
                      : x11_get_default_terminal();
  }

  if (!term) {
    return NULL;
  }

  snprintf(default_apps_x11[slot], sizeof(default_apps_x11[slot]), "%s", term);
  return default_apps_x11[slot];
}

static const char *get_default_browser(int slot) {

  default_apps_x11[slot][0] = '\0';

  const char *env = getenv("BROWSER");
  if (env && env[0]) {
    snprintf(default_apps_x11[slot], sizeof(default_apps_x11[slot]), "%s", env);
    return default_apps_x11[slot];
  }
  const char *possible_browsers[] = {"Chrome",
                                     "Firefox",
                                     "Edge",
                                     "Brave",
                                     "Opera",
                                     "Vivaldi",
                                     "Tor",
                                     "Waterfox",
                                     "LibreWolf",
                                     "Arc",
                                     "Sidekick",
                                     "Pale Moon",
                                     "Internet Explorer",
                                     "Zen"};
  ListOfFiles browser_files = {0};
  FileWalk_AllFilesFromDir("/usr/share/applications", NULL, &browser_files,
                           NULL, true, 0);
  for (size_t i = 0; i < browser_files.count; i++) {
    const char *name = browser_files.files[i].name;
    for (size_t j = 0;
         j < sizeof(possible_browsers) / sizeof(possible_browsers[0]); j++) {
      if (String_ContainsCI(name, possible_browsers[j])) {
        snprintf(default_apps_x11[slot], sizeof(default_apps_x11[slot]), "%s",
                 String_ToLower(possible_browsers[j]));
        return default_apps_x11[slot];
      }
    }
    break;
  }
  StringList l = File_ReadLines("~/.config/mimeapps.list");
  for (size_t i = 0; i < l.count; i++) {
    const char *line = l.items[i];
    if (!String_ContainsCI(line, "text/html"))
      continue;
    char *value = String_CutAfter(line, '=');
    if (!value)
      break;
    char *first = String_CutBefore(value, ';');
    free(value);
    if (!first)
      break;
    char *trimmed = String_Trim(first);
    free(first);
    if (trimmed) {
      char *stem = String_CutBefore(trimmed, '.');
      if (stem) {
        if (stem[0])
          snprintf(default_apps_x11[slot], sizeof(default_apps_x11[slot]), "%s",
                   stem);
        free(stem);
      }
      free(trimmed);
    }
    break;
  }
  StringList_Free(&l);
  return default_apps_x11[slot];
}

static const char *get_default_shell(int slot) {
  default_apps_x11[slot][0] = '\0';
  const char *shell = getenv("SHELL");
  const char *name = strrchr(shell, '/') ? strrchr(shell, '/') + 1 : shell;
  snprintf(default_apps_x11[slot], sizeof(default_apps_x11[slot]), "%s", name);
  return default_apps_x11[slot];
}

const char *Platform_GetDefaultApp(DefaultAppKind kind) {

  if (kind < 0 || kind >= DEFAULT_APP_COUNT)
    return "";
  if (kind == DEFAULT_BROWSER)
    return get_default_browser(DEFAULT_BROWSER);
  if (kind == DEFAULT_TERMINAL)
    return get_default_terminal(DEFAULT_TERMINAL);
  if (kind == DEFAULT_SHELL)
    return get_default_shell(DEFAULT_SHELL);
  default_apps_x11[kind][0] = '\0';
  return default_apps_x11[kind];
}

bool Platform_ResolveDesktopExec(const char *desktop_id, char *out,
                                 size_t out_size) {

  if (!desktop_id || !desktop_id[0] || !out || out_size == 0)
    return false;
  if (strchr(desktop_id, '/'))
    return false;
  out[0] = '\0';
  char dirs[1024];
  const char *data_home = getenv("XDG_DATA_HOME");
  const char *data_dirs = getenv("XDG_DATA_DIRS");
  if (!data_dirs || !data_dirs[0])
    data_dirs = "/usr/local/share:/usr/share";
  if (data_home && data_home[0])
    snprintf(dirs, sizeof(dirs), "%s:%s", data_home, data_dirs);
  else
    snprintf(dirs, sizeof(dirs), "%s/.local/share:%s",
             x11_env_home_dir() ? x11_env_home_dir() : "/root", data_dirs);
  StringList dir_list = String_Split(dirs, ":");
  for (size_t i = 0; i < dir_list.count; i++) {
    const char *d = dir_list.items[i];
    if (!d || !d[0])
      continue;
    char path[1024];
    snprintf(path, sizeof(path), "%s/applications/%s", d, desktop_id);
    static char content[16384];
    if (File_ReadAll(path, content, (long)sizeof(content)) <= 0)
      continue;
    bool found = false;
    char *cur = content;
    while (*cur) {
      char *nl = strchr(cur, '\n');
      if (nl)
        *nl = '\0';
      char *line = cur;
      cur = nl ? nl + 1 : cur + strlen(cur);
      if (strncmp(line, "Exec=", 5) == 0) {
        found = x11_default_apps_exec_token(line, out, out_size);
        break;
      }
    }
    if (found) {
      StringList_Free(&dir_list);
      return true;
    }
    out[0] = '\0';
  }
  StringList_Free(&dir_list);
  return false;
}
