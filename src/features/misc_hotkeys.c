#include "core/flapi.h"

void HandleKillMyProgram(void) {
    fl_terminate_process_by_name("flint.exe");
    fl_terminate_process_by_name("main.exe");
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

void HandleMediaPlayPause(void) {
  fl_toggle_play_pause();
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+K", HandleKillMyProgram);
    fl_register_hotkey("Alt+Shift+T", HandleShowCurrentTime);
    fl_register_hotkey("Alt+D", HandleSettingsTrigger);
    fl_register_hotkey("Alt+Shift+N", HandleShowCurrentBatteryPercentage);
    fl_register_hotkey("Ctrl+N",HandleMediaPlayPause);
}
