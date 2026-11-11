#include "core/skapi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char current_keyword[256] = "";
static int current_image_id = 0;
static int previous_image_id = 0;

void GoogleMeaning(void) {
  char *clip = sk_get_last_clipboard_entry();
  char search[50];
  snprintf(search, sizeof(search), "what is %s", clip);
  sk_query_browser(search);
}

void next_image(void) {
  if (current_keyword[0]) {
    char cmd1[400];
    snprintf(cmd1, sizeof(cmd1),
             "python ~/Programming/RandomShit/shit/get_ids.py %s --random",
             current_keyword);
    char *output = sk_run_terminal_command(cmd1);
    char *title = sk_get_active_window_title();
    if (!title)
      title = "";
    if (sk_string_contains(title, "feh")) {
      sk_close_active_window();
    }
    char cmd[512];
    SkStringList list = sk_string_split(output, "\n");
    snprintf(cmd, sizeof(cmd),
             "feh ~/Programming/RandomShit/shit/images/%s --zoom 65",
             list.items[0]);
    sk_launch_detached(cmd);
    previous_image_id = current_image_id;
    current_image_id = atoi(list.items[0]);
    free(output);
  } else {
    char cmd[400];
    char *output =
        sk_run_terminal_command("ls ~/Programming/RandomShit/shit/images");
    SkStringList list = sk_string_split(output, "\n");
    int idx = sk_rand_num_between(0, list.count - 1);
    char *title = sk_get_active_window_title();
    if (!title)
      title = "";
    if (sk_string_contains(title, "feh")) {
      sk_close_active_window();
    }
    snprintf(cmd, sizeof(cmd),
             "feh ~/Programming/RandomShit/shit/images/%s --zoom 65",
             list.items[idx]);
    sk_launch_detached(cmd);
    previous_image_id = current_image_id;
    current_image_id = atoi(list.items[0]);
    free(output);
  }
}

void last_image(void) {
  char *title = sk_get_active_window_title();
  if (!title)
    title = "";
  if (sk_string_contains(title, "feh")) {
    sk_close_active_window();
  }
  char cmd[512];
  snprintf(cmd, sizeof(cmd),
           "feh ~/Programming/RandomShit/shit/images/%i --zoom 65",
           previous_image_id);
  sk_launch_detached(cmd);
}

void get_current_keyword(void) {
  char *popup_res = sk_popup();
  if (popup_res) {
    strncpy(current_keyword, popup_res, sizeof(current_keyword) - 1);
    current_keyword[sizeof(current_keyword) - 1] = '\0';
  } else {
    current_keyword[0] = '\0';
  }
}

void easy(void) {
  sk_send_hotkey("1");
  sk_send_hotkey("Space");
  sk_sleep(0.1);
  sk_send_hotkey("1");
}

void medium(void) {
  sk_send_hotkey("2");
  sk_send_hotkey("Space");
  sk_sleep(0.1);
  sk_send_hotkey("2");
}
void hard(void) {
  sk_send_hotkey("3");
  sk_send_hotkey("Space");
  sk_sleep(0.1);
  sk_send_hotkey("3");
}
void fuckme(void) {
  sk_send_hotkey("4");
  sk_send_hotkey("Space");
  sk_sleep(0.1);
  sk_send_hotkey("4");
}

void start_auto_click(void) {
  SkMouseEvent ev;
  while (sk_poll_mouse(&ev)) {
  }
  while (1) {
    sk_click();
    sk_sleep(0.01);
    bool stop = false;
    while (sk_poll_mouse(&ev)) {
      if (ev.down)
        stop = true;
    }
    if (stop)
      break;
  }
}

SKALD_FEATURE_INIT {
  sk_register_hotkey(.hotkey = "MouseButton4", .cb = start_auto_click,
                     .blacklist = {"Fortnite", "Librewolf"});
  sk_register_hotkey(.hotkey = "Alt+Shift+G", .cb = GoogleMeaning);
  sk_register_hotkey(.hotkey = "Alt+Shift+F", .cb = next_image);
  sk_register_hotkey(.hotkey = "E", .cb = next_image, .whitelist = {"feh"});
  sk_register_hotkey(.hotkey = "Q", .cb = last_image, .whitelist = {"feh"});
  sk_register_hotkey(.hotkey = "Alt+Shift+D", .cb = get_current_keyword);
  sk_register_hotkey(.hotkey = "J", .cb = easy, .whitelist = {"anki"});
  sk_register_hotkey(.hotkey = "K", .cb = medium, .whitelist = {"anki"});
  sk_register_hotkey(.hotkey = "L", .cb = hard, .whitelist = {"anki"});
  sk_register_hotkey(.hotkey = ";", .cb = fuckme, .whitelist = {"anki"});
}
