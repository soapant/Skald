#include "core/flapi.h"
#include "config.h"
#include <stdio.h>

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
void OpenImage(void) {
  char *clip = fl_get_last_clipboard_entry();
  char cmd[512];
  snprintf(cmd,sizeof(cmd)," feh /home/diggy/Programming/RandomShit/shit/images/%s --zoom 65",clip);
  fl_run_terminal_command(cmd);
  fl_send_hotkey("Alt+F");
}

void copy_seifisbad(void) {
    fl_copy_to_clipboard("seifisbad@gmail.com");
    SubjectiveMessageDisplay("seifisbad@gmail.com",140,1,COLOR_TEXT);
}
void copy_password(void) {
    fl_copy_to_clipboard("i_am_retarded1");
}
void open_terminal(void) {
    fl_run_terminal_command_and_open("");
}
void open_browser(void) {
    fl_launch_browser("");
}

void move_active_window_right(void) {
    fl_move_active_window_right();
}
void move_active_window_left(void) {
    fl_move_active_window_left();
}
void focus_window_right(void) {
    fl_focus_window_right();
}
void focus_window_left(void) {
    fl_focus_window_left();
}
void close_active_window(void) {
    fl_close_active_window();
}
void toggle_fullscreen(void) {
    fl_toggle_fullscreen_active();
}
void toggle_minimized(void) {
    fl_toggle_minimized_active();
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+T", ShowCurrentTime);
    // fl_register_hotkey("Alt+Shift+N", ShowCurrentBatteryPercentage);
    fl_register_hotkey("Ctrl+Shift+X", ScreenshotArea);
    fl_register_hotkey("Alt+Shift+C", OcrRegionAndCopy);
    fl_register_hotkey("Alt+Shift+G", GoogleMeaning);
    fl_register_hotkey("Alt+Shift+F", OpenImage);
    fl_register_hotkey("RShift",MediaPlayPause); 
    fl_register_hotkey("Ctrl+]",IncrementVolume); 
    fl_register_hotkey("Ctrl+[",DecrementVolume); 
    fl_register_hotkey("Alt+Ctrl+O", copy_seifisbad);
    fl_register_hotkey("Alt+Ctrl+P", copy_password);
    fl_register_hotkey("Alt+Q", open_terminal);
    fl_register_hotkey("Ctrl+Shift+B", open_browser);
    fl_register_hotkey("Alt+Shift+L", move_active_window_right);
    fl_register_hotkey("Alt+Shift+H", move_active_window_left);
    fl_register_hotkey("Alt+Shift+O", focus_window_right);
    fl_register_hotkey("Alt+Shift+I", focus_window_left);
    fl_register_hotkey("Ctrl+Q", close_active_window);
    fl_register_hotkey("Alt+F", toggle_fullscreen);
    fl_register_hotkey("Alt+M", toggle_minimized);
}

