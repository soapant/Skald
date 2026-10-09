#include "core/flapi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool g_recording = false;
static FlThread g_thread = NULL;

#define WIDTH fl_get_screen_width()
#define HEIGHT fl_get_screen_height()

#define MACRO_MAX_EVENTS 32768

#define EVT_KEY   1
#define EVT_MOUSE 2
#define EVT_MOVE  3

typedef struct {
    int type;
    int a; /* vk for key, button for mouse */
    int b; /* scancode for key, unused for mouse */
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
    m.type = EVT_MOUSE;
    m.a = e->button;
    m.x = e->x;
    m.y = e->y;
    m.mods = e->mods;
    m.down = e->down;
    m.repeat = e->repeat;
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

static void record_push_move(int x, int y, unsigned long long t_us) {
    MacroEvent m;
    memset(&m, 0, sizeof(m));
    m.type = EVT_MOVE;
    m.x = x;
    m.y = y;
    m.mods = 0;
    m.t_us = t_us;
    record_push(&m);
}

static unsigned long long now_us(void) {
  FlTimer t = fl_start_timer();
  return t.ticks / 1000ULL;
}

static void* recorder_loop(void* arg) {
  (void)arg;
  int last_x = -1, last_y = -1;
  while (g_recording) {
    fl_message(.render=recording_render,.no_bg=true,.w=1920,.h=1080,.x=960,.y=540,.timeout=0.1);

    FlKeyEvent ke;
    while (fl_poll_key(&ke)) record_push_key(&ke);

    FlMouseEvent me;
    while (fl_poll_mouse(&me)) record_push_mouse(&me);

    FlPos p = fl_get_cursor_pos();
    if (p.x != last_x || p.y != last_y) {
      record_push_move(p.x, p.y, now_us());
      last_x = p.x;
      last_y = p.y;
    }

    fl_sleep(0.005);
  }

  g_thread = NULL;
  return NULL;
}

static const char* mouse_button_name(int b) {
    switch (b) {
        case FL_MOUSE_LEFT:    return "L";
        case FL_MOUSE_RIGHT:   return "R";
        case FL_MOUSE_MIDDLE:  return "M";
        case FL_MOUSE_SIDE1:   return "S1";
        case FL_MOUSE_SIDE2:   return "S2";
        case FL_MOUSE_BACK:    return "BK";
        case FL_MOUSE_FORWARD: return "FW";
        default:               return "?";
    }
}

static void print_recording(void) {
  sort_events();
  unsigned long long t0 = (g_event_count > 0) ? g_events[0].t_us : 0ULL;
  printf("--- %d events%s ---\n", g_event_count, g_overflow ? " (OVERFLOW, truncated)" : "");
  for (int i = 0; i < g_event_count; i++) {
    const MacroEvent* m = &g_events[i];
    unsigned long long dt = (i + 1 < g_event_count) ? (g_events[i + 1].t_us - m->t_us) : 0ULL;
    if (m->type == EVT_KEY) {
      printf("KEY  %-4s vk=%-4d sc=%-3d mods=0x%02x%s  +%6lluus  dt_next=%lluus\n",
             m->down ? "DOWN" : "UP", m->a, m->b, (unsigned)m->mods,
             m->repeat ? " rep" : "", m->t_us - t0, dt);
    } else if (m->type == EVT_MOUSE) {
      printf("MOUSE %-3s btn=%-2s mods=0x%02x pos=%d,%d  +%6lluus  dt_next=%lluus\n",
             m->down ? "DOWN" : "UP", mouse_button_name(m->a), (unsigned)m->mods,
             m->x, m->y, m->t_us - t0, dt);
    } else {
      printf("MOVE       pos=%d,%-6d +%6lluus  dt_next=%lluus\n",
             m->x, m->y, m->t_us - t0, dt);
    }
  }
}

static void reset_recording(void) {
  g_event_count = 0;
  g_overflow = false;
}

void RecordToggle(void) {
  if (!g_recording) {
    fl_clear_key_queue();
    fl_clear_mouse_queue();
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