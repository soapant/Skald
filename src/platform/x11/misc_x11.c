#define _GNU_SOURCE
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <dirent.h>
#include <time.h>
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdint.h>
#include <strings.h>
#include <sys/types.h>
#include "platform/platform.h"
#include "platform/x11/env_helpers_x11.h"
#include "deps/log.h"
#include "wm/wm.h"
#include "platform/x11/clipboard_x11.h"
#include "platform/x11/clipboard_backend_x11.h"
#include "helpers/helpers_text.h"
#include "config_values.h"
#include "core/types.h"
#include "core/types/keys.h"

static uid_t ProbeSessionUid(void) {

  DIR *d = opendir("/tmp/.X11-unix");
  if (!d)
    return (uid_t)-1;
  struct dirent *e;
  while ((e = readdir(d)) != NULL) {
    if (e->d_name[0] != 'X')
      continue;
    char path[512];
    snprintf(path, sizeof(path), "/tmp/.X11-unix/%s", e->d_name);
    struct stat st;
    if (stat(path, &st) == 0 && st.st_uid != 0) {
      closedir(d);
      return st.st_uid;
    }
  }
  closedir(d);
  return (uid_t)-1;
}

const char *Platform_GetRealUser(void) {

  static char buf[256] = {0};
  static const char *const env_names[] = {"SUDO_USER", "USER", "LOGNAME", NULL};
  for (int i = 0; env_names[i]; i++) {
    const char *u = getenv(env_names[i]);
    if (u && u[0] && strcmp(u, "root") != 0) {
      strncpy(buf, u, sizeof(buf) - 1);
      return buf;
    }
  }

  uid_t suid = ProbeSessionUid();
  if (suid != (uid_t)-1) {
    struct passwd *spw = getpwuid(suid);
    if (spw && spw->pw_name) {
      strncpy(buf, spw->pw_name, sizeof(buf) - 1);
      return buf;
    }
  }
  struct passwd *pw = getpwuid(getuid());
  if (pw && pw->pw_name && strcmp(pw->pw_name, "root") != 0) {
    strncpy(buf, pw->pw_name, sizeof(buf) - 1);
    log_info("Platform_GetRealUser completed");
    return buf;
  }
  return NULL;
}

const char *Platform_GetRealHome(void) {

  static char buf[512] = {0};
  const char *u = Platform_GetRealUser();
  struct passwd *pw = getpwnam(u);
  if (pw && pw->pw_dir) {
    strncpy(buf, pw->pw_dir, sizeof(buf) - 1);
    return buf;
  }
  pw = getpwuid(getuid());
  if (pw && pw->pw_dir) {
    strncpy(buf, pw->pw_dir, sizeof(buf) - 1);
    return buf;
  }
  const char *h = Env_HomeDir();
  if (h && h[0]) {
    strncpy(buf, h, sizeof(buf) - 1);
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

const char *Platform_GetSessionDisplay(void) {

  static char buf[64] = {0};
  const char *env = getenv("DISPLAY");
  if (env && env[0]) {
    snprintf(buf, sizeof(buf), "%s", env);
    return buf;
  }
  DIR *d = opendir("/tmp/.X11-unix");
  if (d) {
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
      if (e->d_name[0] != 'X')
        continue;
      char *end = NULL;
      long n = strtol(e->d_name + 1, &end, 10);
      if (end == e->d_name + 1 || *end != '\0')
        continue;
      char path[512];
      snprintf(path, sizeof(path), "/tmp/.X11-unix/%s", e->d_name);
      struct stat st;
      if (stat(path, &st) != 0)
        continue;

      if (st.st_uid == geteuid() || st.st_uid != 0) {
        snprintf(buf, sizeof(buf), ":%ld", n);
        break;
      }
    }
    closedir(d);
  }
  log_info("Platform_GetSessionDisplay completed");
  return buf[0] ? buf : ":0";
}

const char *Platform_GetSessionXAuthority(void) {

  static char buf[512] = {0};
  struct stat xst;
  const char *xa = getenv("XAUTHORITY");
  if (xa && xa[0] && stat(xa, &xst) == 0 && xst.st_size > 0 &&
      access(xa, R_OK) == 0) {
    snprintf(buf, sizeof(buf), "%s", xa);
    return buf;
  }
  char rtd[256];
  const char *rt = Env_XdgRuntimeDir();
  if (rt && rt[0])
    snprintf(rtd, sizeof(rtd), "%s", rt);
  else
    snprintf(rtd, sizeof(rtd), "/run/user/%d", (int)getuid());
  DIR *d = opendir(rtd);
  if (d) {
    struct dirent *e;
    time_t best_mtime = 0;
    char best[512] = {0};
    while ((e = readdir(d)) != NULL) {
      if (strncmp(e->d_name, "xauth_", 6) != 0)
        continue;
      char path[512];
      snprintf(path, sizeof(path), "%s/%s", rtd, e->d_name);
      struct stat st;
      if (stat(path, &st) != 0)
        continue;
      if (st.st_size == 0 || access(path, R_OK) != 0)
        continue;
      if (st.st_mtime >= best_mtime) {
        best_mtime = st.st_mtime;
        snprintf(best, sizeof(best), "%s", path);
      }
    }
    closedir(d);
    if (best[0]) {
      snprintf(buf, sizeof(buf), "%s", best);
      return buf;
    }
  }
  snprintf(buf, sizeof(buf), "%s/.Xauthority", Platform_GetRealHome());
  if (stat(buf, &xst) == 0 && xst.st_size > 0 && access(buf, R_OK) == 0)
    return buf;
  buf[0] = '\0';
  log_info("Platform_GetSessionXAuthority completed");
  return "";
}

const char *Platform_GetRuntimeDir(void) {

  static char buf[256] = {0};
  const char *rt = Env_XdgRuntimeDir();
  if (rt && rt[0]) {
    snprintf(buf, sizeof(buf), "%s", rt);
    return buf;
  }
  const char *u = Platform_GetRealUser();
  struct passwd *pw = getpwnam(u);
  if (pw) {
    snprintf(buf, sizeof(buf), "/run/user/%d", (int)pw->pw_uid);
    return buf;
  }
  snprintf(buf, sizeof(buf), "/run/user/%d", (int)getuid());
  log_info("Platform_GetRuntimeDir completed");
  return buf[0] ? buf : "/run/user/1000";
}

bool Platform_ExpandPath(const char *template_path, char *out,
                         size_t out_size) {

  if (template_path == NULL || out_size == 0)
    return false;

  if (template_path[0] == '~' &&
      (template_path[1] == '/' || template_path[1] == '\0')) {
    const char *home = Platform_GetRealHome();
    if (home && home[0]) {
      size_t hn = strlen(home);
      size_t total = 0;
      if (hn < out_size) {
        memcpy(out, home, hn);
        total = hn;
      }
      const char *rest = template_path + 1;
      while (*rest && total < out_size - 1)
        out[total++] = *rest++;
      out[total] = '\0';
      if (total >= 4) {
        size_t i = total - 4;
        if (out[i] == '.' && (out[i + 1] == 'e' || out[i + 1] == 'E') &&
            (out[i + 2] == 'x' || out[i + 2] == 'X') &&
            (out[i + 3] == 'e' || out[i + 3] == 'E')) {
          out[i] = '\0';
        }
      }
      return true;
    }
  }

  size_t o = 0;
  const char *p = template_path;
  while (*p != '\0' && o < out_size - 1) {
    if (*p == '{') {
      const char *close = strchr(p + 1, '}');
      if (close != NULL) {
        size_t token_len = (size_t)(close - p - 1);
        if (token_len > 0 && token_len < 64) {
          char token[64];
          memcpy(token, p + 1, token_len);
          token[token_len] = '\0';
          return false;
        }
      }
    }
    out[o++] = *p++;
  }
  out[o] = '\0';

  if (o >= 4) {
    size_t i = o - 4;
    if (out[i] == '.' && (out[i + 1] == 'e' || out[i + 1] == 'E') &&
        (out[i + 2] == 'x' || out[i + 2] == 'X') &&
        (out[i + 3] == 'e' || out[i + 3] == 'E')) {
      out[i] = '\0';
    }
  }
  return true;
}

int ConvertLinuxKeyToWindowsVK(int key_code) {
  if (key_code >= KEY_1 && key_code <= KEY_0) {
    return '1' + (key_code - KEY_1);
  }

  switch (key_code) {
  case KEY_A:
    return 'A';
  case KEY_B:
    return 'B';
  case KEY_C:
    return 'C';
  case KEY_D:
    return 'D';
  case KEY_E:
    return 'E';
  case KEY_F:
    return 'F';
  case KEY_G:
    return 'G';
  case KEY_H:
    return 'H';
  case KEY_I:
    return 'I';
  case KEY_J:
    return 'J';
  case KEY_K:
    return 'K';
  case KEY_L:
    return 'L';
  case KEY_M:
    return 'M';
  case KEY_N:
    return 'N';
  case KEY_O:
    return 'O';
  case KEY_P:
    return 'P';
  case KEY_Q:
    return 'Q';
  case KEY_R:
    return 'R';
  case KEY_S:
    return 'S';
  case KEY_T:
    return 'T';
  case KEY_U:
    return 'U';
  case KEY_V:
    return 'V';
  case KEY_W:
    return 'W';
  case KEY_X:
    return 'X';
  case KEY_Y:
    return 'Y';
  case KEY_Z:
    return 'Z';
  case KEY_SPACE:
    return VK_SPACE;
  case KEY_ENTER:
    return VK_RETURN;
  case KEY_BACKSPACE:
    return VK_BACK;
  case KEY_TAB:
    return VK_TAB;
  case KEY_ESC:
    return VK_ESCAPE;
  case KEY_LEFT:
    return VK_LEFT;
  case KEY_UP:
    return VK_UP;
  case KEY_RIGHT:
    return VK_RIGHT;
  case KEY_DOWN:
    return VK_DOWN;
  case KEY_LEFTCTRL:
    return VK_LCONTROL;
  case KEY_RIGHTCTRL:
    return VK_RCONTROL;
  case KEY_LEFTALT:
    return VK_LMENU;
  case KEY_RIGHTALT:
    return VK_RMENU;
  case KEY_LEFTSHIFT:
    return VK_LSHIFT;
  case KEY_RIGHTSHIFT:
    return VK_RSHIFT;
  case KEY_LEFTMETA:
    return VK_LWIN;
  case KEY_RIGHTMETA:
    return VK_RWIN;
  case KEY_SEMICOLON:
    return VK_OEM_1;
  case KEY_SLASH:
    return VK_OEM_2;
  case KEY_LEFTBRACE:
    return VK_OEM_4;
  case KEY_RIGHTBRACE:
    return VK_OEM_6;
  case KEY_COMMA:
    return VK_OEM_COMMA;
  case KEY_DOT:
    return VK_OEM_PERIOD;
  case KEY_APOSTROPHE:
    return VK_OEM_7;
  case KEY_MINUS:
    return VK_OEM_MINUS;
  case KEY_EQUAL:
    return VK_OEM_PLUS;
  case KEY_GRAVE:
    return VK_OEM_3;
  case KEY_BACKSLASH:
    return VK_OEM_5;
  default:
    return 0;
  }
}

bool Platform_StringContainsCaseInsensitive(const char *haystack,
                                            const char *needle) {

  if (!haystack || !needle)
    return false;
  return strcasestr(haystack, needle) != NULL;
}

bool Platform_StringStartsWithCaseInsensitive(const char *str,
                                              const char *prefix) {

  if (!str || !prefix)
    return false;
  size_t plen = strlen(prefix);
  if (plen == 0)
    return true;
  return strncasecmp(str, prefix, plen) == 0;
}

bool Platform_StringEqualsCaseInsensitive(const char *a, const char *b) {

  if (!a || !b)
    return false;
  return strcasecmp(a, b) == 0;
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

bool Platform_TerminateProcessByName(const char *exe_name) {
  if (!exe_name || !*exe_name)
    return false;
  char tmp[256];
  strncpy(tmp, exe_name, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = '\0';
  char *ext = strrchr(tmp, '.');
  if (ext && strcmp(ext, ".exe") == 0)
    *ext = '\0';

  char cmd[1024];
  snprintf(cmd, sizeof(cmd),
           "sh -c 'pkill -9 \"%s\" 2>/dev/null; pkill -9 -f \"%s\" "
           "2>/dev/null; pkill -9 -f \"build/[f]lint\" 2>/dev/null; pkill "
           "\"%s\" 2>/dev/null; killall -9 \"%s\" 2>/dev/null; killall -9 -r "
           "\"%s\" 2>/dev/null; killall \"%s\" 2>/dev/null' &",
           tmp, tmp, tmp, tmp, tmp, tmp);

  log_info("Platform_TerminateProcessByName exe_name='%s'",
           exe_name ? exe_name : "");
  ;
  return Platform_RunTerminalCommand(cmd, false);
}

static bool x11_cmd_exists(const char *cmd) {

  if (!cmd || !cmd[0])
    return false;
  if (strchr(cmd, '/'))
    return access(cmd, X_OK) == 0;
  const char *path = getenv("PATH");
  if (!path || !path[0])
    path = "/usr/bin:/bin:/usr/local/bin";
  StringList dirs = String_Split(path, ":");
  for (size_t i = 0; i < dirs.count; i++) {
    char *dir = dirs.items[i];
    if (!dir || !dir[0])
      continue;
    char full[4352];
    snprintf(full, sizeof(full), "%s/%s", dir, cmd);
    if (access(full, X_OK) == 0) {
      StringList_Free(&dirs);
      return true;
    }
  }
  StringList_Free(&dirs);
  return false;
}

static bool x11_launch_browser(const char *browser_path, const char *url,
                               bool new_window) {
  if (!browser_path || !browser_path[0] || !url || !url[0])
    return false;
  char command[2048];
  const char *real_user = Platform_GetRealUser();
  const char *display = getenv("DISPLAY");
  if (!display)
    display = ":0";
  const char *xauthority = getenv("XAUTHORITY");
  if (!xauthority)
    xauthority = "";
  const char *env_runtime = Env_XdgRuntimeDir();
  char runtime_dir[256];
  if (env_runtime)
    snprintf(runtime_dir, sizeof(runtime_dir), "%s", env_runtime);
  else
    snprintf(runtime_dir, sizeof(runtime_dir), "/run/user/1000");
  if (Platform_IsRunningAsRoot()) {
    if (new_window)
      snprintf(command, sizeof(command),
               "sudo -u %s DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s %s "
               "--new-window \"%s\" > /dev/null 2>&1 &",
               real_user, display, xauthority, runtime_dir, browser_path, url);
    else
      snprintf(command, sizeof(command),
               "sudo -u %s DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s %s "
               "\"%s\" > /dev/null 2>&1 &",
               real_user, display, xauthority, runtime_dir, browser_path, url);
  } else {
    if (new_window)
      snprintf(command, sizeof(command),
               "DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s %s --new-window "
               "\"%s\" > /dev/null 2>&1 &",
               display, xauthority, runtime_dir, browser_path, url);
    else
      snprintf(command, sizeof(command),
               "DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s %s \"%s\" > "
               "/dev/null 2>&1 &",
               display, xauthority, runtime_dir, browser_path, url);
  }
  return Platform_RunTerminalCommand(command, false);
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
  if (Platform_StringStartsWithCaseInsensitive(url, "https://")) {
    snprintf(full_url, sizeof(full_url), "%s", url);
  } else {
    snprintf(full_url, sizeof(full_url), "https://%s", url);
  }
  return x11_launch_browser(UsedBrowser, full_url, new_window);
}

bool Platform_QueryBrowser(char *query) {
  if (!query || !query[0])
    return false;

  char browser_tok[512];
  size_t tok_len = UsedBrowser ? strcspn(UsedBrowser, " \t") : 0;
  if (tok_len >= sizeof(browser_tok))
    tok_len = sizeof(browser_tok) - 1;
  if (UsedBrowser)
    memcpy(browser_tok, UsedBrowser, tok_len);
  browser_tok[tok_len] = '\0';

  char url[2048];

  char encoded_query[2048] = {0};
  char *src = query;
  char *dst = encoded_query;
  while (*src && (dst - encoded_query) < sizeof(encoded_query) - 1) {
    if (*src == ' ') {
      *dst++ = '%';
      *dst++ = '2';
      *dst++ = '0';
    } else if (*src == '&') {
      *dst++ = '%';
      *dst++ = '2';
      *dst++ = '6';
    } else if (*src == '?') {
      *dst++ = '%';
      *dst++ = '3';
      *dst++ = 'F';
    } else {
      *dst++ = *src;
    }
    src++;
  }
  *dst = '\0';

  if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "google")) {
    snprintf(url, sizeof(url), "https://www.google.com/search?q=%s",
             encoded_query);
  } else if (Platform_StringContainsCaseInsensitive(UsedSearchEngine,
                                                    "duckduckgo")) {
    snprintf(url, sizeof(url), "https://duckduckgo.com/?q=%s", encoded_query);
  } else if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "bing")) {
    snprintf(url, sizeof(url), "https://www.bing.com/search?q=%",
             encoded_query);
  } else {
    snprintf(url, sizeof(url), "https://duckduckgo.com/?q=%s", encoded_query);
  }
  bool ok = x11_launch_browser(UsedBrowser, url, false);
  log_info("Platform_QueryBrowser query='%s'", query ? query : "");
  return ok;
}

char *Platform_GetClipboardEntry() {
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
  if (Platform_ClipboardAvailable()) {
    out = Platform_ClipboardGetText();
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
  log_info("Platform_PasteContent content='%s'", content ? content : "");
  ;
  return Wm_Paste(content);
}

bool Platform_GetFeatureBuildCommand(const char *src, const char *out_tmp,
                                     char *cmd, size_t cmd_size) {
  snprintf(cmd, cmd_size,
           "gcc -std=gnu17 -I include -I/usr/include/freetype2 "
           "-I/usr/include/libpng16 -I/usr/include/gio-unix-2.0 "
           "-I/usr/include/X11 -pipe -fPIC -shared -o %s %s 2>&1",
           out_tmp, src);
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
    snprintf(full, sizeof(full), "%s/%s", it->dir, ent->d_name);
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
