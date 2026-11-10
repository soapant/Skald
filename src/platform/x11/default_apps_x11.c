#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdbool.h>

#include "platform/platform.h"
#include "platform/x11/env_helpers_x11.h"
#include "helpers/helpers_text.h"
#include "core/types.h"

static char default_apps_x11[DEFAULT_APP_COUNT][256];

#define APPDIR_CACHE_SLOTS 8
static ListOfFiles g_ad_cache[APPDIR_CACHE_SLOTS];
static char g_ad_path[APPDIR_CACHE_SLOTS][1024];
static bool g_ad_valid[APPDIR_CACHE_SLOTS];

static ListOfFiles *x11_applications_dir_files(const char *dir) {

  static int next_slot = 0;
  for (int i = 0; i < APPDIR_CACHE_SLOTS; i++) {
    if (g_ad_valid[i] && strcmp(g_ad_path[i], dir) == 0)
      return &g_ad_cache[i];
  }
  struct stat st;
  if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode))
    return NULL;
  int slot = next_slot;
  next_slot = (next_slot + 1) % APPDIR_CACHE_SLOTS;
  if (g_ad_valid[slot]) {
    free(g_ad_cache[slot].files);
    g_ad_cache[slot].files = NULL;
    g_ad_cache[slot].count = 0;
    g_ad_cache[slot].capacity = 0;
  }
  FileWalk_AllFilesFromDir(dir, NULL, &g_ad_cache[slot], NULL, true, 0);
  snprintf(g_ad_path[slot], sizeof(g_ad_path[slot]), "%s", dir);
  g_ad_valid[slot] = true;
  return &g_ad_cache[slot];
}

static const char *match_in_applications_dir(const char *dir,
                                             const char *const *names,
                                             size_t count) {

  ListOfFiles *files = x11_applications_dir_files(dir);
  if (!files)
    return NULL;
  const char *found = NULL;
  for (size_t i = 0; i < files->count && !found; i++) {
    for (size_t j = 0; j < count; j++) {
      if (String_ContainsCI(files->files[i].name,
                                                 names[j])) {
        found = names[j];
        break;
      }
    }
  }
  return found;
}

void x11_default_apps_free_cache(void) {
  for (int i = 0; i < APPDIR_CACHE_SLOTS; i++) {
    if (!g_ad_valid[i])
      continue;
    free(g_ad_cache[i].files);
    g_ad_cache[i].files = NULL;
    g_ad_cache[i].count = 0;
    g_ad_cache[i].capacity = 0;
    g_ad_path[i][0] = '\0';
    g_ad_valid[i] = false;
  }
}

const char *x11_find_terminal_in_desktop_files(const char *const *names,
                                               size_t count) {

  char dir[1024];
  const char *home = Env_HomeDir();
  if (home && home[0]) {
    snprintf(dir, sizeof(dir), "%s/.local/share/applications", home);
    const char *hit = match_in_applications_dir(dir, names, count);
    if (hit)
      return hit;
  }
  const char *data_dirs = getenv("XDG_DATA_DIRS");
  if (!data_dirs || !data_dirs[0])
    data_dirs = "/usr/local/share:/usr/share";
  StringList dirs = String_Split(data_dirs, ":");
  for (size_t i = 0; i < dirs.count; i++) {
    const char *tok = dirs.items[i];
    if (!tok || !tok[0])
      continue;
    snprintf(dir, sizeof(dir), "%s/applications", tok);
    const char *hit = match_in_applications_dir(dir, names, count);
    if (hit) {
      StringList_Free(&dirs);
      return hit;
    }
  }
  StringList_Free(&dirs);
  return match_in_applications_dir("/usr/share/applications", names, count);
}

const char *x11_get_default_terminal_wayland(void) {

  const char *const candidates[] = {
      "ptyxis",         "gnome-console", "foot",       "gnome-terminal",
      "konsole",        "alacritty",     "kitty",      "wezterm",
      "xfce4-terminal", "mate-terminal", "lxterminal", "qterminal",
      "yakuake",
  };
  size_t n = sizeof(candidates) / sizeof(candidates[0]);
  const char *out = NULL;
  if (!out) {
    out = x11_find_terminal_in_desktop_files(candidates, n);
  }
  return out ? out : NULL;
}

const char *x11_get_default_terminal_x11(void) {

  const char *const candidates[] = {
      "gnome-terminal",
      "konsole",
      "foot",
      "alacritty",
      "kitty",
      "wezterm",
      "xfce4-terminal",
      "mate-terminal",
      "lxterminal",
      "qterminal",
      "yakuake",
      "terminator",
      "tilix",
      "st",
      "stterm",
      "sakura",
      "zutty",
      "urxvt",
      "rxvt-unicode",
      "xterm",
      "x-terminal-emulator",
  };
  size_t n = sizeof(candidates) / sizeof(candidates[0]);
  const char *out = Platform_RunTerminalCommand(
      "gsettings get org.gnome.desktop.default-applications.terminal exec",
      false);
  char trimmed[256];
  trimmed[0] = '\0';
  if (out) {
    const char *p = out;
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')
      p++;
    if (*p == '\'')
      p++;
    snprintf(trimmed, sizeof(trimmed), "%s", p);
    char *end = trimmed + strlen(trimmed);
    while (end > trimmed && (end[-1] == '\'' || end[-1] == ' ' ||
                             end[-1] == '\n' || end[-1] == '\r'))
      *--end = '\0';
    free(out);
    if (strchr(trimmed, ' ') || strchr(trimmed, '/') || strchr(trimmed, ':')) {
      trimmed[0] = '\0';
    }
    for (size_t i = 0; trimmed[0]; i++) {
      if (!isprint((unsigned char)trimmed[i]))
        trimmed[0] = '\0';
    }
  }
  if (trimmed[0])
    return trimmed;
  return x11_find_terminal_in_desktop_files(candidates, n);
}

const char *get_default_terminal(int slot) {

  default_apps_x11[slot][0] = '\0';

  const char *term = NULL;

  const char *env = getenv("TERMINAL");
  if (env && env[0]) {
    term = env;
  }

  if (!term) {
    const char *st = getenv("XDG_SESSION_TYPE");
    bool is_wayland = (st && strcmp(st, "wayland") == 0) || Env_IsCompositor();
    term = is_wayland ? x11_get_default_terminal_wayland()
                      : x11_get_default_terminal_x11();
  }

  if (!term) {
    return NULL;
  }

  snprintf(default_apps_x11[slot], sizeof(default_apps_x11[slot]), "%s", term);
  return default_apps_x11[slot];
}
const char *get_default_browser(int slot) {

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

const char *get_default_shell(int slot) {

  // temporary maybe in future just use the terminal config or somethign idk
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

int x11_default_apps_exec_token(const char *line, char *out, size_t cap) {
  const char *p = line + 5;
  while (*p == ' ' || *p == '\t')
    p++;
  int quoted = (*p == '"');
  if (quoted)
    p++;
  size_t n = 0;
  while (*p && *p != '\n' && n + 1 < cap) {
    if (quoted && *p == '"')
      break;
    if (!quoted && (*p == ' ' || *p == '\t'))
      break;
    out[n++] = *p++;
  }
  out[n] = '\0';
  return n > 0;
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
             Env_HomeDir() ? Env_HomeDir() : "/root", data_dirs);
  StringList dir_list = String_Split(dirs, ":");
  for (size_t i = 0; i < dir_list.count; i++) {
    char *d = dir_list.items[i];
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
