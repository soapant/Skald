#include "core/flapi.h"
#include <stdlib.h>
#include "core/window_manager.h"

void HandleKillMyProgram(void) {
#if defined(_WIN32)
    fl_terminate_process_by_name("flint.exe");
    fl_terminate_process_by_name("main.exe");
#else
    // try normal pkill first (now fixed to not use -i)
    fl_terminate_process_by_name("flint");
    fl_terminate_process_by_name("main");
    fl_terminate_process_by_name("flint.exe");
#endif
    // fallback: directly quit this process (hotkey should work even if pkill fails due to perms)
    WindowManager* wm = WindowManager_Get();
    if (wm) WindowManager_Quit(wm);
    // also ensure exit if WindowManager not running (global hotkey loop)
    exit(0);
}
void HandleShowCurrentTime(void) {
  char* output = fl_run_terminal_command("date now");
  if (output) {
    SubjectiveMessageDisplay(output,140,3,COLOR_TEXT);
  }
}
void HandleShowCurrentBatteryPercentage(void) {
  char* output = fl_run_terminal_command("cat /sys/class/power_supply/BAT0/capacity");
  if (output) {
    SubjectiveMessageDisplay(output,140,3,COLOR_TEXT);
  }
}

void HandleSettingsTrigger(void) {
    Platform_SettingsPopUp();
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+K", HandleKillMyProgram);
    fl_register_hotkey("Alt+Shift+T", HandleShowCurrentTime);
    fl_register_hotkey("Alt+D", HandleSettingsTrigger);
    fl_register_hotkey("Alt+Shift+N", HandleShowCurrentBatteryPercentage);
}
