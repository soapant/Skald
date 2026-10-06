#include "core/flapi.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>

static char current_keyword[256] = "";
static int current_image_id = 0;
static int previous_image_id = 0;

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

void next_image(void) {
  char cmd[400];
  snprintf(cmd,sizeof(cmd),"~/Programming/RandomShit/shit/run.sh get_ids.py %s --random",current_keyword);
  char* output = fl_run_terminal_command(cmd);
  if (output) {
      char* title = fl_get_active_window_title();
      if (!title) title = "";
      if (fl_string_contains("feh",title)) {
          fl_close_active_window();
      }
      char cmd[512];
      FlStringList list = fl_string_split(output,"\n");
      snprintf(cmd,sizeof(cmd),"feh /home/dog/Programming/RandomShit/shit/images/%s --zoom 65",list.items[0]);
      fl_run_terminal_command(cmd);
      previous_image_id = current_image_id;
      current_image_id = atoi(list.items[0]);
  }
}

void last_image(void) {
    char* title = fl_get_active_window_title();
    if (!title) title = "";
    if (fl_string_contains("feh",title)) {
        fl_close_active_window();
    }
    char cmd[512];
    snprintf(cmd,sizeof(cmd),"feh /home/dog/Programming/RandomShit/shit/images/%i --zoom 65",previous_image_id);
    fl_run_terminal_command(cmd);
}


void get_current_keyword(void) {
    char *popup_res = fl_popup();
    if (popup_res) {
        strncpy(current_keyword, popup_res, sizeof(current_keyword) - 1);
        current_keyword[sizeof(current_keyword) - 1] = '\0';
    } else {
        current_keyword[0] = '\0';
    }
}

void copy_seifisbad(void) {
    fl_copy_to_clipboard("seifisbad@gmail.com");
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
    fl_register_hotkey("Ctrl+Shift+X", fl_screenshot_copy_area);
    fl_register_hotkey("Alt+Shift+C", fl_ocr_area_to_clipboard);
    fl_register_hotkey("Alt+Shift+G", GoogleMeaning);
    fl_register_hotkey("Alt+Shift+F", next_image);
    fl_register_hotkey("E", next_image,WHITELIST,"feh");
    fl_register_hotkey("Q", last_image,WHITELIST,"feh");
    fl_register_hotkey("Alt+Shift+D", get_current_keyword);
    fl_register_hotkey("RShift",fl_toggle_play_pause); 
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
    fl_register_snippet("!pass","nigga");
}

