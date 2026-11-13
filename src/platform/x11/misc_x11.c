#define _GNU_SOURCE
#include "helpers/x11/raw_misc_x11.h"
#include "helpers/x11/env_helpers_x11.h"
#include "deps/log.h"
#include "wm/wm.h"
#include "helpers/x11/clipboard_x11.h"
#include "platform/x11/clipboard_backend_x11.h"
#include "helpers/helpers_text.h"
#include "config_values.h"
#include "core/types.h"
#include "core/prompt_geometry.h"
#include "core/types/keys.h"
#include "platform/draw.h"
#include "helpers/x11/window_factory_x11.h"
#include "helpers/x11/bg_image_x11.h"
#include "helpers/x11/drawing_x11.h"
#include "helpers/x11/display_x11.h"

const char *Platform_GetRealUser(void) {

  static char buf[256] = {0};
  static const char *const env_names[] = {"SUDO_USER", "USER", "LOGNAME", NULL};
  for (int i = 0; env_names[i]; i++) {
    const char *u = getenv(env_names[i]);
    if (u && u[0] && strcmp(u, "root") != 0) {
      SafeCopy(buf, sizeof(buf), u);
      return buf;
    }
  }

  uid_t suid = x11_probe_session_uid();
  if (suid != (uid_t)-1) {
    struct passwd *spw = getpwuid(suid);
    if (spw && spw->pw_name) {
      SafeCopy(buf, sizeof(buf), spw->pw_name);
      return buf;
    }
  }
  struct passwd *pw = getpwuid(getuid());
  if (pw && pw->pw_name && strcmp(pw->pw_name, "root") != 0) {
    SafeCopy(buf, sizeof(buf), pw->pw_name);
    log_info("Platform_GetRealUser completed");
    return buf;
  }
  return NULL;
}

const char *Platform_GetRealHome(void) {

  static char buf[512] = {0};
  const char *u = Platform_GetRealUser();
  struct passwd *pw = u ? getpwnam(u) : NULL;
  if (pw && pw->pw_dir) {
    SafeCopy(buf, sizeof(buf), pw->pw_dir);
    return buf;
  }
  pw = getpwuid(getuid());
  if (pw && pw->pw_dir) {
    SafeCopy(buf, sizeof(buf), pw->pw_dir);
    return buf;
  }
  const char *h = x11_env_home_dir();
  if (h && h[0]) {
    SafeCopy(buf, sizeof(buf), h);
    return buf;
  }
  snprintf(buf, sizeof(buf), "/home/%s", u);
  log_info("Platform_GetRealHome completed");
  return buf;
}

bool Platform_IsRunningAsRoot(void) {
  log_info("Platform_IsRunningAsRoot completed");
  return getuid() == 0;
}

bool Platform_MkdirIfNotExists(const char *path) {
  if (!path)
    return false;
  if (mkdir(path, 0755) == 0)
    return true;
  return errno == EEXIST;
}

long long Platform_GetWallClockMs(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (long long)ts.tv_sec * 1000LL + (long long)ts.tv_nsec / 1000000LL;
}

bool Platform_ProcessRunning(const char *name) {
  if (!name || !*name)
    return false;
  DIR *d = opendir("/proc");
  if (!d)
    return false;
  bool found = false;
  struct dirent *e;
  while (!found && (e = readdir(d)) != NULL) {
    if (e->d_name[0] < '0' || e->d_name[0] > '9')
      continue;
    char path[512];
    snprintf(path, sizeof(path), "/proc/%s/comm", e->d_name);
    FILE *f = fopen(path, "r");
    if (!f)
      continue;
    char comm[256] = {0};
    if (fgets(comm, sizeof(comm), f)) {
      char *nl = strchr(comm, '\n');
      if (nl)
        *nl = '\0';
      if (strcasestr(comm, name))
        found = true;
    }
    fclose(f);
  }
  closedir(d);
  return found;
}

bool Platform_TerminateProcessByName(const char *exe_name) {
  if (!exe_name || !*exe_name)
    return false;
  char tmp[256];
  SafeCopy(tmp, sizeof(tmp), exe_name);
  char *ext = strrchr(tmp, '.');
  if (ext && strcmp(ext, ".exe") == 0)
    *ext = '\0';

  char cmd[2048];
  snprintf(cmd, sizeof(cmd),
           "sh -c 'pkill -9 \"%s\" 2>/dev/null; pkill -9 -f \"%s\" "
           "2>/dev/null; pkill"
           "\"%s\" 2>/dev/null; killall -9 \"%s\" 2>/dev/null; killall -9 -r "
           "\"%s\" 2>/dev/null; killall \"%s\" 2>/dev/null' &",
           tmp, tmp, tmp, tmp, tmp, tmp);

  log_info("Platform_TerminateProcessByName exe_name='%s'",
           exe_name ? exe_name : "");
  ;
  return Platform_RunTerminalCommand(cmd, false);
}

bool Platform_LaunchBrowserAtUrl(char *url, bool new_window) {
  char full_url[2048];
  char browser_tok[512];
  size_t tok_len = UsedBrowser ? strcspn(UsedBrowser, " \t") : 0;
  if (tok_len >= sizeof(browser_tok))
    tok_len = sizeof(browser_tok) - 1;
  if (UsedBrowser)
    memcpy(browser_tok, UsedBrowser, tok_len);
  browser_tok[tok_len] = '\0';
  bool have_browser = tok_len > 0 && x11_cmd_exists(browser_tok);
  if (!have_browser)
    return false;
  if (String_StartsWithCI(url, "https://")) {
    snprintf(full_url, sizeof(full_url), "%s", url);
  } else {
    snprintf(full_url, sizeof(full_url), "https://%s", url);
  }
  return x11_launch_browser(UsedBrowser, full_url, new_window);
}

bool Platform_QueryBrowser(const char *query) {
  if (!query || !query[0])
    return false;

  char url[2048];
  Url_BuildSearchUrl(UsedSearchEngine, query, url, sizeof(url));
  bool ok = x11_launch_browser(UsedBrowser, url, false);
  log_info("Platform_QueryBrowser query='%s'", query);
  return ok;
}

char *Platform_GetClipboardEntry(void) {
  static long long next_in_ms = 0;
  static int fail_streak = 0;
  static unsigned long last_owner = 0;
  static bool owner_seen = false;
  long long now = Platform_GetWallClockMs();
  unsigned long owner = x11_clipboard_owner();
  if (!owner_seen || owner != last_owner) {
    owner_seen = true;
    last_owner = owner;
    fail_streak = 0;
    next_in_ms = 0;
  }
  if (owner == 0)
    return NULL;
  if (fail_streak > 0 && now < next_in_ms) {
    return NULL;
  }
  char *out = NULL;
  if (x11_clipboard_available()) {
    out = x11_clipboard_get_text();
    if (out) {
      fail_streak = 0;
      next_in_ms = 0;
      return out;
    }
  }
  out = x11_clipboard_get_text();
  if (!out) {
    fail_streak++;
    if (fail_streak > 4)
      fail_streak = 4;
    next_in_ms = now + (200LL << fail_streak);
  } else {
    fail_streak = 0;
    next_in_ms = 0;
  }
  return out;
}

bool Platform_PasteContent(char *content) {
  log_info("Platform_PasteContent content length=%zu",
           content ? strlen(content) : 0);
  char *result = Wm_Paste(content);
  bool ok = result != NULL;
  free(result);
  return ok;
}

bool Platform_GetFeatureBuildArgv(const char *src, const char *out_tmp,
                                  const char *argv[], size_t argv_cap) {
  size_t n = 0;
#define ADD_ARG(arg)                                                           \
  do {                                                                         \
    if (n + 1 >= argv_cap)                                                     \
      return false;                                                            \
    argv[n++] = (arg);                                                         \
  } while (0)
  ADD_ARG("gcc");
  ADD_ARG("-std=gnu17");
  ADD_ARG("-I");
  ADD_ARG("build/sdk");
  ADD_ARG("-I/usr/include/freetype2");
  ADD_ARG("-I/usr/include/libpng16");
  ADD_ARG("-I/usr/include/gio-unix-2.0");
  ADD_ARG("-I/usr/include/X11");
  ADD_ARG("-pipe");
  ADD_ARG("-fPIC");
  ADD_ARG("-shared");
  ADD_ARG("-o");
  ADD_ARG(out_tmp);
  ADD_ARG(src);
  argv[n] = NULL;
#undef ADD_ARG
  return true;
}

bool Platform_AtomicRename(const char *old_path, const char *new_path) {
  return rename(old_path, new_path) == 0;
}

int64_t Platform_TimerNowTicks(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

int64_t Platform_TimerTickFrequency(void) { return 1000000000LL; }

struct PlDirIter {
  char dir[MAX_PATH];
  DIR *dh;
};

PlDirIter *Platform_DirOpen(const char *dir_path) {
  if (!dir_path)
    return NULL;
  DIR *dh = opendir(dir_path);
  if (!dh)
    return NULL;
  PlDirIter *it = calloc(1, sizeof(*it));
  if (!it) {
    closedir(dh);
    return NULL;
  }
  snprintf(it->dir, sizeof(it->dir), "%s", dir_path);
  it->dh = dh;
  return it;
}

bool Platform_DirNext(PlDirIter *it, char *name_out, size_t name_cap,
                      bool *is_dir_out) {
  if (!it || !name_out || name_cap == 0 || !is_dir_out)
    return false;
  struct dirent *ent;
  while ((ent = readdir(it->dh)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
      continue;
    snprintf(name_out, name_cap, "%s", ent->d_name);
    char full[MAX_PATH];
    size_t dl = strlen(it->dir), el = strlen(ent->d_name);
    if (dl + 1 + el + 1 > sizeof(full))
      continue;
    memcpy(full, it->dir, dl);
    full[dl] = '/';
    memcpy(full + dl + 1, ent->d_name, el + 1);
    struct stat st;
    *is_dir_out = (stat(full, &st) == 0 && S_ISDIR(st.st_mode));
    return true;
  }
  return false;
}

void Platform_DirClose(PlDirIter *it) {
  if (!it)
    return;
  if (it->dh)
    closedir(it->dh);
  free(it);
}

int Platform_GetTextLineHeight(int fontsize) {
  if (fontsize <= 0)
    fontsize = FontSize;
  Display *dpy = x11_get_display();
  if (!dpy)
    return fontsize;
  FontHandle font = x11_create_font_on_display(dpy, fontsize, UsedFont);
  XftFont *xf = FH_TO_XFT(font);
  int r = xf ? xf->ascent + xf->descent : fontsize;
  Platform_DestroyFont(font);
  return r;
}
void Platform_PrewarmBackgroundPixmap(void) {
  Display *dpy = x11_get_display();
  if (!dpy) {
    return;
  }
  x11_bg_image_prewarm(dpy, PromptWidth, PromptHeight);
  Window _pw = x11_create_prompt_window(dpy, 0, 0, PromptWidth, PromptHeight);
  if (_pw != None) {
    x11_get_cached_background(dpy, _pw, PromptWidth, PromptHeight);
    XDestroyWindow(dpy, _pw);
  }
}
