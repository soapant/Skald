#include "platform/platform.h"
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>

const char *Platform_GetRealHome(void) {
  struct passwd *pw = getpwuid(getuid());
  if (pw && pw->pw_dir)
    return pw->pw_dir;
  char *home = getenv("HOME");
  return home ? home : "/tmp";
}

const char *Platform_GetRealUser(void) {
  struct passwd *pw = getpwuid(getuid());
  if (pw && pw->pw_name)
    return pw->pw_name;
  char *user = getenv("USER");
  return user ? user : "user";
}

bool Platform_QueryBrowser(const char *query) { return false; }

bool Platform_GetFeatureBuildArgv(const char *src, const char *out_tmp,
                                 const char *argv[], size_t argv_cap) {
  size_t n = 0;
#define ADD_ARG(arg)                                                     \
  do {                                                                         \
    if (n + 1 >= argv_cap)                                                     \
      return false;                                                            \
    argv[n++] = (arg);                                                         \
  } while (0)
  ADD_ARG("gcc");
  ADD_ARG("-std=gnu17");
  ADD_ARG("-I");
  ADD_ARG("include");
  ADD_ARG("-I/usr/include/freetype2");
  ADD_ARG("-I/usr/include/libpng16");
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

bool Platform_ProcessRunning(const char *name) {
  if (!name || !*name)
    return false;
  FILE *fp = popen("ps -A -o comm=", "r");
  if (!fp)
    return false;
  bool found = false;
  char line[512];
  while (fgets(line, sizeof(line), fp)) {
    char *nl = strchr(line, '\n');
    if (nl)
      *nl = '\0';
    if (strcasestr(line, name)) {
      found = true;
      break;
    }
  }
  pclose(fp);
  return found;
}
