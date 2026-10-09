#include "core/flapi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool g_recording = false;
static FlThread g_thread = NULL;

#define WIDTH fl_get_screen_width()
#define HEIGHT fl_get_screen_height()

static void recording_render() {
    int max_x = WIDTH - 1;
    int max_y = HEIGHT - 1;

    fl_draw_line(.x1 = 0,     .y1 = 0,     .x2 = max_x, .y2 = 0,     .color = RGB(180, 0, 0));
    fl_draw_line(.x1 = max_x, .y1 = 0,     .x2 = max_x, .y2 = max_y, .color = RGB(180, 0, 0));
    fl_draw_line(.x1 = max_x, .y1 = max_y, .x2 = 0,     .y2 = max_y, .color = RGB(180, 0, 0));
    fl_draw_line(.x1 = 0,     .y1 = max_y, .x2 = 0,     .y2 = 0,     .color = RGB(180, 0, 0));
}

static void* recorder_loop(void* arg) {
  while (g_recording) {
    FlKeyEvent e;
    fl_message(.render=recording_render,.no_bg=true,.w=1920,.h=1080,.x=960,.y=540,.timeout=0.1);
    while (fl_poll_key(&e)) {
      printf("mods=%d vk=%d sc=%d\n", e.mods, e.vk, e.scancode);
    }
    fl_sleep(0.1);
  }

  g_thread = NULL;
  return NULL;
}

void RecordToggle(void) {
  if (!g_recording) {
    fl_clear_key_queue();
    g_recording = true;
    if (!g_thread) g_thread = fl_thread_create(recorder_loop, NULL);
    return;
  }
  g_recording = false;
}

FLINT_FEATURE_INIT {
  fl_register_hotkey("Ctrl+Alt+R", RecordToggle);
}
