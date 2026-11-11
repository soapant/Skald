#include "core/skapi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
const char *MusicPlayerPath = "C:/Projects/Music Player/music_player.exe";
#elif defined(__linux__)
const char *MusicPlayerPath =
    "~/Programming/Projects/Music Player/music_player";
#endif
#if defined(_WIN32)
const char *MusicPath = "C:/Music";
#elif defined(__linux__)
const char *MusicPath = "~/Music";
#endif

char *allowed_audio_formats[] = {"mp3", "ogg", "flac", NULL};

ListOfFiles s_music_files = {0};
static const char *s_music_items[MAX_VISIBLE_ITEMS];

void music_refresh_callback(PromptState *state) {
  size_t n = s_music_files.count;
  if (n > MAX_VISIBLE_ITEMS)
    n = MAX_VISIBLE_ITEMS;
  for (size_t i = 0; i < n; i++) {
    s_music_items[i] = s_music_files.files[i].name;
  }
  sk_populate_listbox(state->lb, state->eb->filter, s_music_items, (int)n);
}
bool play_music(char *name) {
  SkWindowHandle wh = sk_get_window_id("Music Player");
  if (!WH_EQUALS(wh, WH_NULL)) {
    bool ok = sk_window_set_string_property(wh, "_MP_TRACK", name);
#if defined(__linux__)
    ok = ok && sk_window_send_message(wh, "_MP_TRACK_CHANGED", 1);
#endif
    return ok;
  }
  if (strlen(MusicPlayerPath) > 0) {
    char quoted_name[MAX_PATH + 3] = {0};
    snprintf(quoted_name, sizeof(quoted_name), "\"%s\"", name);
    sk_launch_with_args(MusicPlayerPath, quoted_name);
  }
  return true;
}

bool music_enter_callback(PromptState *state) {
  char *result = NULL;
  if (!sk_is_selected(state->lb)) {
    return false;
  }
  for (size_t i = 0; i < s_music_files.count; i++) {
    if (strcmp(s_music_files.files[i].name, state->lb->selected_text) == 0) {
      result = s_music_files.files[i].path;
      break;
    }
  }
  return play_music(result);
}

static void music_cmd(int cmd) {
  SkWindowHandle wh = sk_get_window_id("Music Player");
  if (!WH_EQUALS(wh, WH_NULL))
    sk_window_send_message(wh, "_MP_PLAYER_CMD", cmd);
}
void music_cmd_1(void) { music_cmd(1); }
void music_cmd_5(void) { music_cmd(5); }
void music_cmd_6(void) { music_cmd(6); }
void music_cmd_2(void) { music_cmd(2); }

void random_music(void) {
  int idx = sk_rand_num_between(0, s_music_files.count - 1);
  play_music(s_music_files.files[idx].path);
}
void music_prompt(void) {
  sk_window(.title = "Player", .refresh = music_refresh_callback,
            .enter = music_enter_callback);
}
void reload_music(void) {
  s_music_files.count = 0;
  sk_get_all_files_from_dir(MusicPath, allowed_audio_formats, &s_music_files,
                            NULL, true, SK_DIR_WALK_UNLIMITED);
}
void add_music(void) {
  char *url = sk_popup(.placeholder = "Enter youtube url for the video");
  char *name = sk_popup(
          .placeholder =
              "Enter a custom name for this song (or leave blank for default)");
  if (!url)
    return;
  char cmd[512];

  if (name && strlen(name) > 0) {
    snprintf(cmd, sizeof(cmd),
             "yt-dlp -f \"ba\" -x --audio-format mp3 --audio-quality 0 -o "
             "\"%s/%s.%%(ext)s\" \"%s\"",
             MusicPath, name, url);
  } else {
    snprintf(cmd, sizeof(cmd),
             "yt-dlp -f \"ba\" -x --audio-format mp3 --audio-quality 0 -o "
             "\"%s/%%(title)s.%%(ext)s\" \"%s\"",
             MusicPath, url);
  }
  free(name);
  free(url);
  sk_run_terminal_command(cmd);
}

typedef void (*toggle_play_pause)(void);

static void pause() {
  sk_cap_bind(playpause, "toggle.playpause", toggle_play_pause);
  if (playpause) {
    if (sk_process_running("music_player")) {
      music_cmd_1();
    } else {
      playpause();
    }
  }
}

SKALD_FEATURE_INIT {
  sk_register_hotkey(.hotkey = "Alt+,", .cb = music_cmd_1);
  sk_register_hotkey(.hotkey = "Alt+/", .cb = music_cmd_5);
  sk_register_hotkey(.hotkey = "Alt+;", .cb = music_cmd_6);
  sk_register_hotkey(.hotkey = "Alt+'", .cb = music_cmd_2);
  sk_register_hotkey(.hotkey = "Alt+P", .cb = music_prompt);
  sk_register_hotkey(.hotkey = "Alt+Shift+P", .cb = random_music);
  sk_register_hotkey(.hotkey = "RShift", .cb = pause);
  sk_add_to_cc(.display_name = "Force Reload Music", .on_action = reload_music,
               .icon = "/music.png");
  sk_add_feature(.display_name = "Music Player", .on_action = music_prompt,
                 .icon = "/music.png");
  sk_add_to_cc(.display_name = "Add Music", .on_action = add_music,
               .icon = "/music.png");

  reload_music();
}
