#include "core/skapi.h"
#include <stdlib.h>
#include <stdio.h>


#if defined(__linux__)
static bool sudo_run(const char *const argv[]) {
  char *pw = sk_popup(.password_mode = true);
  if (!pw)
    return false;
  int status = -1;
  char *out = sk_run_argv(argv, pw, &status);
  free(pw);
  free(out);
  return status == 0;
}
#endif

static bool do_restart(void) {
#if defined(_WIN32)
#elif defined(__linux__)
  const char *argv[] = {"sudo", "-S", "xbps-query", "-l", NULL};
  return sudo_run(argv);
#endif
  return true;
}
static bool do_shutdown(void) {
#if defined(_WIN32)
#elif defined(__linux__)
  const char *argv[] = {"sudo", "-S", "shutdown", "now", NULL};
  return sudo_run(argv);
#endif
  return true;
}

SKALD_FEATURE_INIT {
  sk_add_to_cc(.display_name = "Restart/Reboot",
               .on_enter = (bool (*)(void))do_restart, .type = "SystemControl",
               .icon = "/refresh.png");
  sk_add_to_cc(.display_name = "ShutDown",
               .on_enter = (bool (*)(void))do_shutdown, .type = "SystemControl",
               .icon = "/power.png");
}
