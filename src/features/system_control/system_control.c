#include "core/skapi.h"

static bool do_restart(void) {
#if defined(_WIN32)
  sk_run_terminal_command("Restart-Computer");
#elif defined(__linux__)
  sk_run_terminal_command("reboot");
#endif
  return true;
}
static bool do_shutdown(void) {
#if defined(_WIN32)
  sk_run_terminal_command("Stop-Computer");
#elif defined(__linux__)
  sk_run_terminal_command("shutdown now");
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
