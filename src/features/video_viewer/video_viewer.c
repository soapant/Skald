#include "core/skapi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32)
const char *VideoViewerPath = "C:/Projects/Video Viewer/video_viewer.exe";
#elif defined(__linux__)
const char *VideoViewerPath =
    "~/Programming/Projects/Video Viewer/video_viewer";
#endif
#if defined(_WIN32)
const char *VideosPath = "C:/Videos";
#elif defined(__linux__)
const char *VideosPath = "~/Videos";
#endif

char *allowed_video_formats[] = {"webm", "mp4"};

ListOfFiles video_files = {0};
static const char *s_video_items[MAX_VISIBLE_ITEMS];

void video_refresh_callback(PromptState *state) {
  video_files.count = 0;
  sk_get_all_files_from_dir(VideosPath, allowed_video_formats, &video_files,
                            NULL, true, SK_DIR_WALK_UNLIMITED);

  size_t n = video_files.count;
  if (n > MAX_VISIBLE_ITEMS)
    n = MAX_VISIBLE_ITEMS;
  for (size_t i = 0; i < n; i++) {
    s_video_items[i] = video_files.files[i].name;
  }
  sk_populate_listbox(state->lb, state->eb->filter, s_video_items, (int)n);
}

bool video_enter_callback(PromptState *state) {
  char *result = NULL;
  if (!sk_is_selected(state->lb)) {
    return false;
  }
  for (size_t i = 0; i < video_files.count; i++) {
    if (strcmp(video_files.files[i].name, state->lb->selected_text) == 0) {
      result = video_files.files[i].path;
      break;
    }
  }
  if (!result) {
    ListOfFiles tmp = {0};
    sk_get_all_files_from_dir(VideosPath, allowed_video_formats, &tmp, NULL,
                              true, SK_DIR_WALK_UNLIMITED);
    for (size_t i = 0; i < tmp.count; i++) {
      if (strcmp(tmp.files[i].name, state->lb->selected_text) == 0) {
        result = tmp.files[i].path;
        break;
      }
    }
    if (result) {
      char found_path[MAX_PATH];
      strncpy(found_path, result, sizeof(found_path) - 1);
      found_path[sizeof(found_path) - 1] = '\0';
      result = found_path;
    }
    free(tmp.files);
  }
  if (!result)
    return true;
  SkWindowHandle wh = sk_get_window_id("Video Viewer");
  if (!WH_EQUALS(wh, WH_NULL)) {
    bool ok = sk_window_set_string_property(wh, "_MP_TRACK", result);
#if defined(__linux__)
    ok = ok && sk_window_send_message(wh, "_MP_TRACK_CHANGED", 1);
#endif
    return ok;
  }
  if (strlen(VideoViewerPath) > 0) {
    char quoted_result[MAX_PATH + 3] = {0};
    snprintf(quoted_result, sizeof(quoted_result), "\"%s\"", result);
    sk_launch_with_args(VideoViewerPath, quoted_result);
  }
  return true;
}
static void video_cmd(long cmd) {
  SkWindowHandle wh = sk_get_window_id("Video Viewer");
  if (!WH_EQUALS(wh, WH_NULL))
    sk_window_set_long_property(wh, "_MP_CMD", cmd);
}
void video_cmd_1(void) { video_cmd(1); }
void video_cmd_5(void) { video_cmd(5); }
void video_cmd_6(void) { video_cmd(6); }
void video_prompt(void) {
  sk_window(.title = "Viewer", .refresh = video_refresh_callback,
            .enter = video_enter_callback);
}

SKALD_FEATURE_INIT {
  sk_register_hotkey(.hotkey = "Alt+Shift+,", .cb = video_cmd_1);
  sk_register_hotkey(.hotkey = "Alt+Shift+.", .cb = video_cmd_5);
  sk_register_hotkey(.hotkey = "Alt+Shift+;", .cb = video_cmd_6);
  sk_register_hotkey(.hotkey = "Alt+V", .cb = video_prompt);

  sk_add_feature(.display_name = "Video Viewer", .on_action = video_prompt,
                 .icon = "/video.png");
}
