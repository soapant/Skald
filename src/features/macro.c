#include "core/flapi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool g_recording = false;
static FlThread g_thread = NULL;

#define WIDTH fl_get_screen_width()
#define HEIGHT fl_get_screen_height()

#define MACRO_MAX_EVENTS 32768 * 2

#define MACRO_RECORD_VK 'R'
#define MACRO_PLAY_VK 'P'

#define EVT_KEY   1
#define EVT_MOUSE 2
#define EVT_MOVE  3

typedef struct {
    int type;
    int a; 
    int b;
    int x;
    int y;
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

static void record_push(const MacroEvent* m) {
    if (g_event_count >= MACRO_MAX_EVENTS) { g_overflow = true; return; }
    g_events[g_event_count++] = *m;
}

static void record_push_key(const FlKeyEvent* e) {
    MacroEvent m;
    memset(&m, 0, sizeof(m));
    m.type = EVT_KEY;
    m.a = e->vk;
    m.b = e->scancode;
    m.mods = e->mods;
    m.down = e->down;
    m.repeat = e->repeat;
    m.t_us = e->t_us;
    record_push(&m);
}

static void record_push_mouse(const FlMouseEvent* e) {
    MacroEvent m;
    memset(&m, 0, sizeof(m));
    if (e->button == 0) {
        m.type = EVT_MOVE;
    } else {
        m.type = EVT_MOUSE;
        m.a = e->button;
        m.mods = e->mods;
        m.down = e->down;
        m.repeat = e->repeat;
    }
    m.x = e->x;
    m.y = e->y;
    m.t_us = e->t_us;
    record_push(&m);
}

static bool macro_event_less(const MacroEvent* a, const MacroEvent* b) {
    if (a->t_us != b->t_us) return a->t_us < b->t_us;
    return a->type < b->type;
}

static void sort_events(void) {
    for (int i = 1; i < g_event_count; i++) {
        MacroEvent key = g_events[i];
        int j = i - 1;
        while (j >= 0 && macro_event_less(&key, &g_events[j])) {
            g_events[j + 1] = g_events[j];
            j--;
        }
        g_events[j + 1] = key;
    }
}

/* Event timestamps come from the platform input queue in microseconds. Keep the
   same clock here: QueryPerformanceCounter runs at 10MHz, so raw ticks are not
   microseconds and mixing the two scales makes every wait 100x too long. */
static void* recorder_loop(void* arg) {
  (void)arg;
  while (g_recording) {
    FlKeyEvent ke;
    while (fl_poll_key(&ke)) record_push_key(&ke);

    FlMouseEvent me;
    while (fl_poll_mouse(&me)) record_push_mouse(&me);

    fl_sleep(0.005);
  }

  g_thread = NULL;
  return NULL;
}

static void reset_recording(void) {
  g_event_count = 0;
  g_overflow = false;
}

static bool g_playing = false;
static FlThread g_play_thread = NULL;

#define MACRO_MAX_HELD 64
static int g_held_keys[MACRO_MAX_HELD];
static int g_held_count = 0;

static void held_add(int vk) {
    if (vk == 0) return;
    for (int i = 0; i < g_held_count; i++) if (g_held_keys[i] == vk) return;
    if (g_held_count < MACRO_MAX_HELD) g_held_keys[g_held_count++] = vk;
}

static void held_remove(int vk) {
    for (int i = 0; i < g_held_count; i++) {
        if (g_held_keys[i] == vk) { g_held_keys[i] = g_held_keys[--g_held_count]; return; }
    }
}

static void release_everything(void) {
    for (int i = 0; i < g_held_count; i++) fl_emit_key(g_held_keys[i], false);
    g_held_count = 0;
    fl_emit_key(VK_SHIFT, false);
    fl_emit_key(VK_CONTROL, false);
    fl_emit_key(VK_MENU, false);
    fl_emit_key(VK_LWIN, false);
    fl_emit_key(VK_RWIN, false);
    for (int b = 1; b <= 5; b++) fl_emit_mouse_button(b, false);
}

static bool is_macro_control_chord(int vk, int mods) {
  if (mods != (HOTKEY_MOD_CTRL | HOTKEY_MOD_ALT)) return false;
  return vk == MACRO_RECORD_VK || vk == MACRO_PLAY_VK;
}

static void* playback_loop(void* arg) {
  (void)arg;
  if (g_event_count == 0) {
    g_playing = false;
    g_play_thread = NULL;
    return NULL;
  }
  sort_events();
  unsigned long long t0 = g_events[0].t_us;
  FlTimer start = fl_start_timer();
  g_held_count = 0;
  for (int i = 0; i < g_event_count && g_playing; i++) {
    const MacroEvent* m = &g_events[i];
    double target_ms = (double)(m->t_us - t0) / 1000.0;
    double cur_ms = fl_end_timer(start);
    double wait_ms = target_ms - cur_ms;
    if (wait_ms > 0.0) fl_sleep(wait_ms / 1000.0);
    if (m->type == EVT_KEY) {
      if (m->repeat) continue;
      if (m->down && is_macro_control_chord(m->a, m->mods)) continue;
      if (m->down && fl_replay_hotkey(m->a, m->mods)) continue;
      fl_emit_key(m->a, m->down);
      if (m->down) held_add(m->a); else held_remove(m->a);
    } else if (m->type == EVT_MOUSE) {
      if (m->a != 0) fl_emit_mouse_button(m->a, m->down);
    } else {
      FlPos p = { m->x, m->y };
      fl_move_to_pos(p);
    }
  }
  release_everything();
  g_playing = false;
  g_play_thread = NULL;
  return NULL;
}

void RecordToggle(void) {
  if (!g_recording) {
    fl_message(.render=recording_render,.no_bg=true,.w=1920,.h=1080,.x=960,.y=540,.timeout=1000.0);
    fl_clear_key_queue();
    fl_clear_mouse_queue();
    reset_recording();
    g_recording = true;
    if (!g_thread) g_thread = fl_thread_create(recorder_loop, NULL);
    return;
  }
  fl_dismiss_message();
  g_recording = false;
}

void PlayToggle(void) {
  if (!g_playing) {
    if (g_event_count == 0) return;
    fl_clear_key_queue();
    fl_clear_mouse_queue();
    g_playing = true;
    if (!g_play_thread) g_play_thread = fl_thread_create(playback_loop, NULL);
    return;
  }
  g_playing = false;
  release_everything();
}

FLINT_FEATURE_INIT {
  fl_register_hotkey("Ctrl+Alt+R", RecordToggle);
  fl_register_hotkey("Ctrl+Alt+P", PlayToggle);
}
