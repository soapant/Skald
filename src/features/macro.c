#include "core/flapi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool g_recording = false;
static FlThread g_thread = NULL;

#define WIDTH fl_get_screen_width()
#define HEIGHT fl_get_screen_height()

#define MACRO_MAX_EVENTS 4096

typedef struct {
    int vk;
    int scancode;
    int mods;
    bool down;
    bool repeat;
    unsigned long long t_us;
} MacroEvent;

static MacroEvent g_events[MACRO_MAX_EVENTS];
static int g_event_count = 0;
static bool g_overflow = false;

static void recording_render() {
    int max_x = WIDTH - 1;
    int max_y = HEIGHT - 1;

    fl_draw_line(.x1 = 0,     .y1 = 0,     .x2 = max_x, .y2 = 0,     .color = RGB(180, 0, 0));
    fl_draw_line(.x1 = max_x, .y1 = 0,     .x2 = max_x, .y2 = max_y, .color = RGB(180, 0, 0));
    fl_draw_line(.x1 = max_x, .y1 = max_y, .x2 = 0,     .y2 = max_y, .color = RGB(180, 0, 0));
    fl_draw_line(.x1 = 0,     .y1 = max_y, .x2 = 0,     .y2 = 0,     .color = RGB(180, 0, 0));
}

static void record_push(const FlKeyEvent* e) {
    if (g_event_count >= MACRO_MAX_EVENTS) { g_overflow = true; return; }
    MacroEvent* m = &g_events[g_event_count++];
    m->vk = e->vk;
    m->scancode = e->scancode;
    m->mods = e->mods;
    m->down = e->down;
    m->repeat = e->repeat;
    m->t_us = e->t_us;
}

static void* recorder_loop(void* arg) {
  (void)arg;
  while (g_recording) {
    FlKeyEvent e;
    fl_message(.render=recording_render,.no_bg=true,.w=1920,.h=1080,.x=960,.y=540,.timeout=0.1);
    while (fl_poll_key(&e)) {
      record_push(&e);
    }
    fl_sleep(0.02);
  }

  g_thread = NULL;
  return NULL;
}

static void print_recording(void) {
  printf("--- %d events%s ---\n", g_event_count, g_overflow ? " (OVERFLOW, truncated)" : "");
  for (int i = 0; i < g_event_count; i++) {
    const MacroEvent* m = &g_events[i];
    unsigned long long dt = (i + 1 < g_event_count) ? (g_events[i + 1].t_us - m->t_us) : 0ULL;
    printf("%-8s vk=%-4d sc=%-3d mods=0x%02x%s  +%6lluus  dt_next=%lluus\n",
           m->down ? "DOWN" : "UP",
           m->vk, m->scancode, (unsigned)m->mods,
           m->repeat ? " rep" : "",
           m->t_us - g_events[0].t_us,
           dt);
  }
}

static void reset_recording(void) {
  g_event_count = 0;
  g_overflow = false;
}

void RecordToggle(void) {
  if (!g_recording) {
    fl_clear_key_queue();
    reset_recording();
    g_recording = true;
    if (!g_thread) g_thread = fl_thread_create(recorder_loop, NULL);
    return;
  }
  g_recording = false;
  print_recording();
}

FLINT_FEATURE_INIT {
  fl_register_hotkey("Ctrl+Alt+R", RecordToggle);
}