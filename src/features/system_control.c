#include "core/flapi.h"

static void restart(const char* arg) {
#if defined(_WIN32)
    fl_run_terminal_command("Restart-Computer");
#else
    fl_run_terminal_command("reboot");
#endif
}
static void shutdown(const char* arg) {
#if defined(_WIN32)
    fl_run_terminal_command("Stop-Computer");
#else
    fl_run_terminal_command("shutdown now");
#endif
}

FLINT_FEATURE_INIT {
    fl_add_to_cc(.display_name="Restart",.on_enter=restart,.type="SystemControl",.icon="⚙️");
}
