#include "core/flapi.h"

void KillMyProgram(void) {
    fl_terminate_process_by_name("flint.exe");
    fl_terminate_process_by_name("main.exe");
}
void ShowCurrentTime(void) {
  char* output = fl_run_terminal_command("date");
  if (output) {
    SubjectiveMessageDisplay(output,140,3,COLOR_TEXT);
  }
}
void ShowCurrentBatteryPercentage(void) {
  char* output = fl_run_terminal_command("cat /sys/class/power_supply/BAT0/capacity");
  if (output) {
    SubjectiveMessageDisplay(output,140,3,COLOR_TEXT);
  }
}

void SettingsTrigger(void) {
    Platform_SettingsPopUp();
}

void MediaPlayPause(void) {
  fl_toggle_play_pause();
}

void ScreenshotArea(void) {
  fl_screenshot_copy_area();
}

void SaveScreenshotArea(void) {
  fl_screenshot_save_area("/home/diggy/Pictures/screenshot.png");
}

void OcrRegionAndCopy(void) {
  fl_ocr_area_to_clipboard();
}

void IncrementVolume(void) {
  fl_increment_volume_by_percent(10);
}

void DecrementVolume(void) {
  fl_decrement_volume_by_percent(10);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+K", KillMyProgram);
    fl_register_hotkey("Alt+Shift+T", ShowCurrentTime);
    fl_register_hotkey("Alt+D", SettingsTrigger);
    fl_register_hotkey("Alt+Shift+N", ShowCurrentBatteryPercentage);
    fl_register_hotkey("Ctrl+Shift+X", ScreenshotArea);
    fl_register_hotkey("Alt+Shift+X", SaveScreenshotArea);
    fl_register_hotkey("Alt+Shift+C", OcrRegionAndCopy);
    fl_register_hotkey("RShift",MediaPlayPause); 
    fl_register_hotkey("Ctrl+]",IncrementVolume); 
    fl_register_hotkey("Ctrl+[",DecrementVolume); 
}

