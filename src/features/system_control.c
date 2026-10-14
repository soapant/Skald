#include "core/flapi.h"

static void do_restart(void) {
#if defined(_WIN32)
    fl_run_terminal_command("Restart-Computer");
#else
    fl_run_terminal_command("reboot");
#endif
}
static void do_shutdown(void) {
#if defined(_WIN32)
    fl_run_terminal_command("Stop-Computer");
#else
    fl_run_terminal_command("shutdown now");
#endif
}

FLINT_FEATURE_INIT {
    fl_add_to_cc(.display_name="Restart/Reboot",.on_enter=do_restart,.type="SystemControl",.icon="⚙️");
    fl_add_to_cc(.display_name="ShutDown",.on_enter=do_shutdown,.type="SystemControl",.icon="⚙️");
}
