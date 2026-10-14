#include "core/flapi.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
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
  if (current_keyword[0]) {
      char cmd1[400];
      snprintf(cmd1,sizeof(cmd1),"~/Programming/RandomShit/shit/run.sh get_ids.py %s --random",current_keyword);
      char* output = fl_run_terminal_command(cmd1);
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
      free(output);
  } else {
     char cmd[400];
     char* output = fl_run_terminal_command("ls /home/dog/Programming/RandomShit/shit/images");
     FlStringList list = fl_string_split(output,"\n");
     int idx = fl_rand_num_between(0, list.count - 1);
     char* title = fl_get_active_window_title();
     if (!title) title = "";
     if (fl_string_contains("feh",title)) {
         fl_close_active_window();
     }
     snprintf(cmd,sizeof(cmd),"feh /home/dog/Programming/RandomShit/shit/images/%s --zoom 65",list.items[idx]);
     fl_run_terminal_command(cmd);
     previous_image_id = current_image_id;
     current_image_id = atoi(list.items[0]);
     free(output);
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

void easy(void) {
 fl_send_hotkey("1"); fl_send_hotkey("Space"); fl_sleep(0.1); fl_send_hotkey("1");
}

void medium(void) {
 fl_send_hotkey("2"); fl_send_hotkey("Space"); fl_sleep(0.1); fl_send_hotkey("2");
}
void hard(void) {
 fl_send_hotkey("3"); fl_send_hotkey("Space"); fl_sleep(0.1); fl_send_hotkey("3");
}
void fuckme(void) {
 fl_send_hotkey("4"); fl_send_hotkey("Space"); fl_sleep(0.1); fl_send_hotkey("4");
}

static FlPos last_mouse_pos = {0, 0}; 

void start_auto_click(void) {
    last_mouse_pos = fl_get_mouse_pos();
    while (fl_get_mouse_pos().x == last_mouse_pos.x && 
           fl_get_mouse_pos().y == last_mouse_pos.y) {
        fl_click();
    }
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Ctrl+Alt+B", start_auto_click,"Zen");
    fl_register_hotkey("Alt+Shift+G", GoogleMeaning);
    fl_register_hotkey("Alt+Shift+F", next_image);
    fl_register_hotkey("E", next_image,WHITELIST,"feh");
    fl_register_hotkey("Q", last_image,WHITELIST,"feh");
    fl_register_hotkey("Alt+Shift+D", get_current_keyword);
    fl_register_hotkey("RShift",fl_toggle_play_pause); 
    fl_register_hotkey("Ctrl+]",IncrementVolume); 
    fl_register_hotkey("Ctrl+[",DecrementVolume); 
    fl_register_hotkey("Alt+Q", open_terminal);
    fl_register_hotkey("Ctrl+Shift+B", open_browser);
    fl_register_hotkey("Alt+Shift+L", move_active_window_right);
    fl_register_hotkey("Alt+Shift+H", move_active_window_left);
    fl_register_hotkey("Alt+Shift+O", focus_window_right);
    fl_register_hotkey("Alt+Shift+I", focus_window_left);
    fl_register_hotkey("Ctrl+Q", close_active_window);
    fl_register_hotkey("Alt+F", toggle_fullscreen);
    fl_register_hotkey("Alt+M", toggle_minimized);
    fl_register_hotkey("J", easy,WHITELIST,"anki");
    fl_register_hotkey("K", medium,WHITELIST,"anki");
    fl_register_hotkey("L", hard,WHITELIST,"anki");
    fl_register_hotkey(";", fuckme,WHITELIST,"anki");
}

