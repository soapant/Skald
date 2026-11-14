#include "helpers/x11/raw_default_apps_x11.h"

#define APPDIR_CACHE_SLOTS 8

static ListOfFiles ad_cache[APPDIR_CACHE_SLOTS];

static char ad_path[APPDIR_CACHE_SLOTS][1024];

static bool ad_valid[APPDIR_CACHE_SLOTS];

static ListOfFiles *x11_applications_dir_files(const char *dir) {

  static int next_slot = 0;
  for (int i = 0; i < APPDIR_CACHE_SLOTS; i++) {
    if (ad_valid[i] && strcmp(ad_path[i], dir) == 0)
      return &ad_cache[i];
  }
  struct stat st;
  if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode))
    return NULL;
  int slot = next_slot;
  next_slot = (next_slot + 1) % APPDIR_CACHE_SLOTS;
  if (ad_valid[slot]) {
    free(ad_cache[slot].files);
    ad_cache[slot].files = NULL;
    ad_cache[slot].count = 0;
    ad_cache[slot].capacity = 0;
  }
  file_walk_all_files_from_dir(dir, NULL, &ad_cache[slot], NULL, true, 0);
  snprintf(ad_path[slot], sizeof(ad_path[slot]), "%s", dir);
  ad_valid[slot] = true;
  return &ad_cache[slot];
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
      if (string_contains_ci(files->files[i].name, names[j])) {
        found = names[j];
        break;
      }
    }
  }
  return found;
}

void x11_default_apps_free_cache(void) {
  for (int i = 0; i < APPDIR_CACHE_SLOTS; i++) {
    if (!ad_valid[i])
      continue;
    free(ad_cache[i].files);
    ad_cache[i].files = NULL;
    ad_cache[i].count = 0;
    ad_cache[i].capacity = 0;
    ad_path[i][0] = '\0';
    ad_valid[i] = false;
  }
}

const char *x11_find_terminal_in_desktop_files(const char *const *names,
                                               size_t count) {

  char dir[1024];
  const char *home = x11_env_home_dir();
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
      string_list_free(&dirs);
      return hit;
    }
  }
  string_list_free(&dirs);
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

const char *x11_get_default_terminal(void) {

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
  char *out = Platform_RunTerminalCommand(
      "gsettings get org.gnome.desktop.default-applications.terminal exec",
      false);
  static __thread char trimmed[256];
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
