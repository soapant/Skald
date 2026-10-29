#include "core/flapi.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char current_keyword[256] = "";
static int current_image_id = 0;
static int previous_image_id = 0;

void GoogleMeaning(void) {
  char *clip = fl_get_last_clipboard_entry();
  char search[50];
  snprintf(search,sizeof(search),"what is %s",clip);
  fl_query_browser(search);
}

void next_image(void) {
  if (current_keyword[0]) {
      char cmd1[400];
      snprintf(cmd1,sizeof(cmd1),"python ~/Programming/RandomShit/shit/get_ids.py %s --random",current_keyword);
      char* output = fl_run_terminal_command(cmd1);
      char* title = fl_get_active_window_title();
      if (!title) title = "";
      if (fl_string_contains("feh",title)) {
          fl_close_active_window();
      }
      char cmd[512];
      FlStringList list = fl_string_split(output,"\n");
      snprintf(cmd,sizeof(cmd),"feh ~/Programming/RandomShit/shit/images/%s --zoom 65",list.items[0]);
      fl_launch_detached(cmd);
      previous_image_id = current_image_id;
      current_image_id = atoi(list.items[0]);
      free(output);
  } else {
     char cmd[400];
     char* output = fl_run_terminal_command("ls ~/Programming/RandomShit/shit/images");
     FlStringList list = fl_string_split(output,"\n");
     int idx = fl_rand_num_between(0, list.count - 1);
     char* title = fl_get_active_window_title();
     if (!title) title = "";
     if (fl_string_contains("feh",title)) {
         fl_close_active_window();
     }
     snprintf(cmd,sizeof(cmd),"feh ~/Programming/RandomShit/shit/images/%s --zoom 65",list.items[idx]);
     fl_launch_detached(cmd);
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
    snprintf(cmd,sizeof(cmd),"feh ~/Programming/RandomShit/shit/images/%i --zoom 65",previous_image_id);
    fl_launch_detached(cmd);
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
    fl_launch_detached("librewolf");
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

void start_auto_click(void) {
    FlMouseEvent ev;
    while (fl_poll_mouse(&ev)) {}
    while (1) {
        fl_click();
        fl_sleep(0.01);
        bool stop = false;
        while (fl_poll_mouse(&ev)) {
            if (ev.down) stop = true;
        }
        if (stop) break;
    }
}

FLINT_FEATURE_INIT {
    fl_register_hotkey(.hotkey="MouseButton4", .cb=start_auto_click,.blacklist={"Fortnite","Librewolf"});
    fl_register_hotkey(.hotkey="Alt+Shift+G", .cb=GoogleMeaning);
    fl_register_hotkey(.hotkey="Alt+Shift+F", .cb=next_image);
    fl_register_hotkey(.hotkey="E", .cb=next_image, .whitelist={"feh"});
    fl_register_hotkey(.hotkey="Q", .cb=last_image, .whitelist={"feh"});
    fl_register_hotkey(.hotkey="Alt+Shift+D", .cb=get_current_keyword);
    fl_register_hotkey(.hotkey="Alt+Q", .cb=open_terminal);
    fl_register_hotkey(.hotkey="Ctrl+Shift+B", .cb=open_browser);
    fl_register_hotkey(.hotkey="J", .cb=easy, .whitelist={"anki"});
    fl_register_hotkey(.hotkey="K", .cb=medium, .whitelist={"anki"});
    fl_register_hotkey(.hotkey="L", .cb=hard, .whitelist={"anki"});
    fl_register_hotkey(.hotkey=";", .cb=fuckme, .whitelist={"anki"});
}
