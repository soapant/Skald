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

void OcrRegionAndCopy(void) {
  fl_ocr_area_to_clipboard();
}

void IncrementVolume(void) {
  fl_increment_volume_by_percent(10);
}

void DecrementVolume(void) {
  fl_decrement_volume_by_percent(10);
}
void GoogleMeaning(void) {
  char *clip = fl_get_last_clipboard_entry();
  char search[50];
  snprintf(search,sizeof(search),"what is %s",clip);
  fl_query_browser(search);
}
void OpenFehImage(void) {
  char *clip = fl_get_last_clipboard_entry();
  char args[256];
  snprintf(args,sizeof(args),"\"/home/diggy/Programming/RandomShit/shit/images/%s\" -g 1920x1080 --zoom 65",clip ? clip : "");
  fl_launch_with_args("feh", args);
  fl_sleep(0.5);
  printf("Sending Alt+F hotkey to feh\n");
  fl_send_hotkey("Alt+F");
}

void copy_seifisbad(void) {
    fl_copy_to_clipboard("seifisbad@gmail.com");
    SubjectiveMessageDisplay("seifisbad@gmail.com",140,1,COLOR_TEXT);
}
void copy_password(void) {
    fl_copy_to_clipboard("i_am_retarded1");
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+K", KillMyProgram);
    fl_register_hotkey("Alt+Shift+T", ShowCurrentTime);
    fl_register_hotkey("Alt+D", SettingsTrigger);
    fl_register_hotkey("Alt+Shift+N", ShowCurrentBatteryPercentage);
    fl_register_hotkey("Ctrl+Shift+X", ScreenshotArea);
    fl_register_hotkey("Alt+Shift+C", OcrRegionAndCopy);
    fl_register_hotkey("Alt+Shift+G", GoogleMeaning);
    fl_register_hotkey("Alt+Shift+F", OpenFehImage);
    fl_register_hotkey("RShift",MediaPlayPause); 
    fl_register_hotkey("Ctrl+]",IncrementVolume); 
    fl_register_hotkey("Ctrl+[",DecrementVolume); 
    fl_register_hotkey("Alt+Ctrl+O", copy_seifisbad);
    fl_register_hotkey("Alt+Ctrl+P", copy_password);
}

