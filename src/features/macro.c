#include "core/flapi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/flapi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>

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

    fl_draw_line(.x1 = 0,     .y1 = 0,     .x2 = max_x, .y2 = 0,     .color = FL_RED);
    fl_draw_line(.x1 = max_x, .y1 = 0,     .x2 = max_x, .y2 = max_y, .color = FL_RED);
    fl_draw_line(.x1 = max_x, .y1 = max_y, .x2 = 0,     .y2 = max_y, .color = FL_RED);
    fl_draw_line(.x1 = 0,     .y1 = max_y, .x2 = 0,     .y2 = 0,     .color = FL_RED);
}

static void playback_render() {
    int max_x = WIDTH - 1;
    int max_y = HEIGHT - 1;

    fl_draw_line(.x1 = 0,     .y1 = 0,     .x2 = max_x, .y2 = 0,     .color = FL_BLUE);
    fl_draw_line(.x1 = max_x, .y1 = 0,     .x2 = max_x, .y2 = max_y, .color = FL_BLUE);
    fl_draw_line(.x1 = max_x, .y1 = max_y, .x2 = 0,     .y2 = max_y, .color = FL_BLUE);
    fl_draw_line(.x1 = 0,     .y1 = max_y, .x2 = 0,     .y2 = 0,     .color = FL_BLUE);
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


static int g_macro_last_mx = -1, g_macro_last_my = -1;

static void* recorder_loop(void* arg) {
  (void)arg;
  while (g_recording) {
    FlKeyEvent ke;
    while (fl_poll_key(&ke)) record_push_key(&ke);

    FlMouseEvent me;
    while (fl_poll_mouse(&me)) record_push_mouse(&me);

    {
      FlPos _mp = fl_get_cursor_pos();
      if (_mp.x != g_macro_last_mx || _mp.y != g_macro_last_my) {
        unsigned long long _now = fl_start_timer().ticks / 1000ULL;
        FlMouseEvent _me;
        memset(&_me, 0, sizeof(_me));
        _me.button = 0;
        _me.x = _mp.x;
        _me.y = _mp.y;
        _me.t_us = _now;
        record_push_mouse(&_me);
        g_macro_last_mx = _mp.x;
        g_macro_last_my = _mp.y;
      }
    }
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
static bool g_macro_infinite = false;
static bool g_macro_remove_dead = false;

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
  while (g_playing) {
  sort_events();
  if (g_event_count == 0) break;
  unsigned long long t0 = g_events[0].t_us;
  FlTimer start = fl_start_timer();
  g_held_count = 0;
  for (int i = 0; i < g_event_count && g_playing; i++) {
    const MacroEvent* m = &g_events[i];
    double target_ms = (double)(m->t_us - t0) / 1000.0;
    double cur_ms = fl_end_timer(start);
    double wait_ms = target_ms - cur_ms;
    if (g_macro_remove_dead && wait_ms > 500.0) wait_ms = 0.0;
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
    if (g_playing && g_macro_infinite) continue;
    break;
  }
  release_everything();
  fl_dismiss_message();
  g_playing = false;
  g_play_thread = NULL;
  return NULL;
}


static ListOfFiles macro_load_files;
static const char *macro_file_name_ptrs[MAX_VISIBLE_ITEMS + 1];

static bool macro_save(const char *name) {
    if (g_event_count == 0) return false;
    if (!name || !name[0]) name = "last.macro";
    char filename[256];
    snprintf(filename, sizeof(filename), "%s", name);
    size_t ln = strlen(filename);
    bool has_ext = (ln >= 4 && filename[ln-4] == '.' && filename[ln-3] == 't' && filename[ln-2] == 'x' && filename[ln-1] == 't');
    if (!has_ext) {
        size_t l = strlen(filename);
        if (l + 4 < sizeof(filename)) strcat(filename, ".txt");
    }


    size_t cap = (size_t)g_event_count * 128 + 256;
    char *buf = (char*)malloc(cap);
    if (!buf) return false;
    size_t off = 0;
    off += (size_t)snprintf(buf + off, cap - off, "%d\n", g_event_count);
    for (int i = 0; i < g_event_count; i++) {
        off += (size_t)snprintf(buf + off, cap - off, "%d %d %d %d %d %d %d %d %llu\n",
            g_events[i].type, g_events[i].a, g_events[i].b, g_events[i].x, g_events[i].y,
            g_events[i].mods, g_events[i].down ? 1 : 0, g_events[i].repeat ? 1 : 0, g_events[i].t_us);
    }
    bool ok = fl_write_string_to_file(filename, buf);
    free(buf);
    return ok;
}

static int macro_load(const char *name) {
    if (!name || !name[0]) name = "last.macro";
    char filename[256];
    snprintf(filename, sizeof(filename), "%s", name);
    size_t ln = strlen(filename);
    bool has_ext2 = (ln >= 4 && filename[ln-4] == '.' && filename[ln-3] == 't' && filename[ln-2] == 'x' && filename[ln-1] == 't');
    if (!has_ext2) {
        if (ln + 4 < sizeof(filename)) strcat(filename, ".txt");
    }
    char *path = fl_resolve_data_path(filename);
    if (!path) return 0;
    size_t len = 0;
    char *data = fl_read_file_to_string(path, &len);
    free(path);
    if (!data) return 0;
    int count = 0;
    char *cursor = data;
    if (sscanf(cursor, "%d", &count) != 1 || count < 0 || count > MACRO_MAX_EVENTS) { free(data); return 0; }
    while (*cursor && *cursor != '\n') cursor++;
    if (*cursor == '\n') cursor++;
    g_event_count = 0;
    for (int i = 0; i < count; i++) {
        MacroEvent m;
        memset(&m, 0, sizeof(m));
        int down = 0, repeat = 0;
        unsigned long long t = 0;
        if (sscanf(cursor, "%d %d %d %d %d %d %d %d %llu",
            &m.type, &m.a, &m.b, &m.x, &m.y, &m.mods, &down, &repeat, &t) != 9) break;
        m.down = down != 0;
        m.repeat = repeat != 0;
        m.t_us = t;
        record_push(&m);
        while (*cursor && *cursor != '\n') cursor++;
        if (*cursor == '\n') cursor++;
    }
    free(data);
    return g_event_count;
}

static void macro_refresh_cache(void) {
    macro_load_files.count = 0;
    char *resolved = fl_resolve_data_path("last.macro");
    if (resolved) {
        char dir[1024];
        snprintf(dir, sizeof(dir), "%s", resolved);
        char *slash = strrchr(dir, '/');
        char *back = strrchr(dir, '\\');
        if (back && (!slash || back > slash)) slash = back;
        if (slash) *slash = '\0';
        fl_get_all_files_from_dir(dir, NULL, &macro_load_files, NULL, false, 1);
        free(resolved);
    }
}

static void macro_refresh(const char *filter, ListBoxInfo *lb) {
    macro_refresh_cache();
    int n = (int)macro_load_files.count;
    if (n > MAX_VISIBLE_ITEMS) n = MAX_VISIBLE_ITEMS;
    for (int i = 0; i < n; i++) macro_file_name_ptrs[i] = macro_load_files.files[i].name;
    fl_populate_listbox(lb, filter, macro_file_name_ptrs, n);
}

static bool macro_load_enter(const char *selection, ListBoxInfo *lb) {
    (void)lb;
    if (selection) macro_load(selection);
    return true;
}

static bool macro_load_ctrlenter(const char *selection, ListBoxInfo *lb) {
    if (selection && selection[0]) {
        char *path = fl_resolve_data_path(selection);
        if (path) {
            remove(path);
            free(path);
        }
        macro_refresh("", lb);
    }
    return false;
}
static bool macro_load_ctrlenter(const char *selection, ListBoxInfo *lb) {
    return true;
}

void SaveMacro(void) {
    if (g_event_count == 0) {
        fl_message(.message="No recording in memory", .timeout=3.0);
        return;
    }
    char *name = fl_popup(.placeholder="Macro file name");
    char chosen[256];
    if (name && name[0]) {
        snprintf(chosen, sizeof(chosen), "%s", name);
        free(name);
    } else {
        if (name) free(name);
        snprintf(chosen, sizeof(chosen), "%s", "last.macro");
    }
    macro_save(chosen);
}

void LoadMacro(void) {
    PROMPT(.title="Macro Load", .refresh=macro_refresh, .enter=macro_load_enter,.ctrl_enter=macro_load_ctrlenter,.auto_scale=true);
}

void PlayMacro(void) {
    if (!g_playing) {
        if (g_event_count == 0) {
            fl_message(.message="No macro loaded", .timeout=3.0);
            return;
        }
        fl_message(.render=playback_render,.no_bg=true,.w=1920,.h=1080,.x=960,.y=540,.timeout=1000.0);
        fl_clear_key_queue();
        fl_clear_mouse_queue();
        g_playing = true;
        if (!g_play_thread) g_play_thread = fl_thread_create(playback_loop, NULL);
        return;
    }
    fl_dismiss_message();
    g_playing = false;
    release_everything();
}

void InfinitePlaybackToggle(void) {
    g_macro_infinite = !g_macro_infinite;
    if (g_macro_infinite) fl_message(.message="Infinite playback enabled", .timeout=1.0);
    else fl_message(.message="Infinite playback disabled", .timeout=1.0);
}

void RemoveDeadTimeToggle(void) {
    g_macro_remove_dead = !g_macro_remove_dead;
    if (g_macro_remove_dead) fl_message(.message="Remove dead time enabled", .timeout=1.0);
    else fl_message(.message="Remove dead time disabled", .timeout=1.0);
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


FLINT_FEATURE_INIT {
  fl_register_hotkey("Ctrl+Alt+R", RecordToggle);
  fl_add_to_cc(.display_name="SaveMacro",.on_enter=SaveMacro,.type="Macro",.icon="");
  fl_add_to_cc(.display_name="LoadMacro",.on_enter=LoadMacro,.type="Macro",.icon="");
  fl_register_hotkey("Ctrl+Alt+P", PlayMacro);
  fl_add_to_cc(.display_name="InfinitePlayback",.on_enter=InfinitePlaybackToggle,.type="Macro",.icon="");
  fl_add_to_cc(.display_name="RemoveDeadTime",.on_enter=RemoveDeadTimeToggle,.type="Macro",.icon="");

}

#include <sys/stat.h>
#include <sys/types.h>

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

    fl_draw_line(.x1 = 0,     .y1 = 0,     .x2 = max_x, .y2 = 0,     .color = FL_RED);
    fl_draw_line(.x1 = max_x, .y1 = 0,     .x2 = max_x, .y2 = max_y, .color = FL_RED);
    fl_draw_line(.x1 = max_x, .y1 = max_y, .x2 = 0,     .y2 = max_y, .color = FL_RED);
    fl_draw_line(.x1 = 0,     .y1 = max_y, .x2 = 0,     .y2 = 0,     .color = FL_RED);
}

static void playback_render() {
    int max_x = WIDTH - 1;
    int max_y = HEIGHT - 1;

    fl_draw_line(.x1 = 0,     .y1 = 0,     .x2 = max_x, .y2 = 0,     .color = FL_BLUE);
    fl_draw_line(.x1 = max_x, .y1 = 0,     .x2 = max_x, .y2 = max_y, .color = FL_BLUE);
    fl_draw_line(.x1 = max_x, .y1 = max_y, .x2 = 0,     .y2 = max_y, .color = FL_BLUE);
    fl_draw_line(.x1 = 0,     .y1 = max_y, .x2 = 0,     .y2 = 0,     .color = FL_BLUE);
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


static int g_macro_last_mx = -1, g_macro_last_my = -1;

static void* recorder_loop(void* arg) {
  (void)arg;
  while (g_recording) {
    FlKeyEvent ke;
    while (fl_poll_key(&ke)) record_push_key(&ke);

    FlMouseEvent me;
    while (fl_poll_mouse(&me)) record_push_mouse(&me);

    {
      FlPos _mp = fl_get_cursor_pos();
      if (_mp.x != g_macro_last_mx || _mp.y != g_macro_last_my) {
        unsigned long long _now = fl_start_timer().ticks / 1000ULL;
        FlMouseEvent _me;
        memset(&_me, 0, sizeof(_me));
        _me.button = 0;
        _me.x = _mp.x;
        _me.y = _mp.y;
        _me.t_us = _now;
        record_push_mouse(&_me);
        g_macro_last_mx = _mp.x;
        g_macro_last_my = _mp.y;
      }
    }
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
static bool g_macro_infinite = false;
static bool g_macro_remove_dead = false;

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
  while (g_playing) {
  sort_events();
  if (g_event_count == 0) break;
  unsigned long long t0 = g_events[0].t_us;
  FlTimer start = fl_start_timer();
  g_held_count = 0;
  for (int i = 0; i < g_event_count && g_playing; i++) {
    const MacroEvent* m = &g_events[i];
    double target_ms = (double)(m->t_us - t0) / 1000.0;
    double cur_ms = fl_end_timer(start);
    double wait_ms = target_ms - cur_ms;
    if (g_macro_remove_dead && wait_ms > 500.0) wait_ms = 0.0;
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
    if (g_playing && g_macro_infinite) continue;
    break;
  }
  release_everything();
  fl_dismiss_message();
  g_playing = false;
  g_play_thread = NULL;
  return NULL;
}


static ListOfFiles macro_load_files;
static const char *macro_file_name_ptrs[MAX_VISIBLE_ITEMS + 1];

static bool macro_save(const char *name) {
    if (g_event_count == 0) return false;
    if (!name || !name[0]) name = "last.macro";
    char filename[256];
    snprintf(filename, sizeof(filename), "%s", name);
    size_t ln = strlen(filename);
    bool has_ext = (ln >= 4 && filename[ln-4] == '.' && filename[ln-3] == 't' && filename[ln-2] == 'x' && filename[ln-1] == 't');
    if (!has_ext) {
        size_t l = strlen(filename);
        if (l + 4 < sizeof(filename)) strcat(filename, ".txt");
    }


    size_t cap = (size_t)g_event_count * 128 + 256;
    char *buf = (char*)malloc(cap);
    if (!buf) return false;
    size_t off = 0;
    off += (size_t)snprintf(buf + off, cap - off, "%d\n", g_event_count);
    for (int i = 0; i < g_event_count; i++) {
        off += (size_t)snprintf(buf + off, cap - off, "%d %d %d %d %d %d %d %d %llu\n",
            g_events[i].type, g_events[i].a, g_events[i].b, g_events[i].x, g_events[i].y,
            g_events[i].mods, g_events[i].down ? 1 : 0, g_events[i].repeat ? 1 : 0, g_events[i].t_us);
    }
    bool ok = fl_write_string_to_file(filename, buf);
    free(buf);
    return ok;
}

static int macro_load(const char *name) {
    if (!name || !name[0]) name = "last.macro";
    char filename[256];
    snprintf(filename, sizeof(filename), "%s", name);
    size_t ln = strlen(filename);
    bool has_ext2 = (ln >= 4 && filename[ln-4] == '.' && filename[ln-3] == 't' && filename[ln-2] == 'x' && filename[ln-1] == 't');
    if (!has_ext2) {
        if (ln + 4 < sizeof(filename)) strcat(filename, ".txt");
    }
    char *path = fl_resolve_data_path(filename);
    if (!path) return 0;
    size_t len = 0;
    char *data = fl_read_file_to_string(path, &len);
    free(path);
    if (!data) return 0;
    int count = 0;
    char *cursor = data;
    if (sscanf(cursor, "%d", &count) != 1 || count < 0 || count > MACRO_MAX_EVENTS) { free(data); return 0; }
    while (*cursor && *cursor != '\n') cursor++;
    if (*cursor == '\n') cursor++;
    g_event_count = 0;
    for (int i = 0; i < count; i++) {
        MacroEvent m;
        memset(&m, 0, sizeof(m));
        int down = 0, repeat = 0;
        unsigned long long t = 0;
        if (sscanf(cursor, "%d %d %d %d %d %d %d %d %llu",
            &m.type, &m.a, &m.b, &m.x, &m.y, &m.mods, &down, &repeat, &t) != 9) break;
        m.down = down != 0;
        m.repeat = repeat != 0;
        m.t_us = t;
        record_push(&m);
        while (*cursor && *cursor != '\n') cursor++;
        if (*cursor == '\n') cursor++;
    }
    free(data);
    return g_event_count;
}

static void macro_refresh_cache(void) {
    macro_load_files.count = 0;
    char *resolved = fl_resolve_data_path("last.macro");
    if (resolved) {
        char dir[1024];
        snprintf(dir, sizeof(dir), "%s", resolved);
        char *slash = strrchr(dir, '/');
        char *back = strrchr(dir, '\\');
        if (back && (!slash || back > slash)) slash = back;
        if (slash) *slash = '\0';
        fl_get_all_files_from_dir(dir, NULL, &macro_load_files, NULL, false, 1);
        free(resolved);
    }
}

static void macro_refresh(const char *filter, ListBoxInfo *lb) {
    macro_refresh_cache();
    int n = (int)macro_load_files.count;
    if (n > MAX_VISIBLE_ITEMS) n = MAX_VISIBLE_ITEMS;
    for (int i = 0; i < n; i++) macro_file_name_ptrs[i] = macro_load_files.files[i].name;
    fl_populate_listbox(lb, filter, macro_file_name_ptrs, n);
}

static bool macro_load_enter(const char *selection, ListBoxInfo *lb) {
    (void)lb;
    if (selection) macro_load(selection);
    return true;
}
static bool macro_load_ctrlenter(const char *selection, ListBoxInfo *lb) {
    return true;
}

void SaveMacro(void) {
    if (g_event_count == 0) {
        fl_message(.message="No recording in memory", .timeout=3.0);
        return;
    }
    char *name = fl_popup(.placeholder="Macro file name");
    char chosen[256];
    if (name && name[0]) {
        snprintf(chosen, sizeof(chosen), "%s", name);
        free(name);
    } else {
        if (name) free(name);
        snprintf(chosen, sizeof(chosen), "%s", "last.macro");
    }
    macro_save(chosen);
}

void LoadMacro(void) {
    PROMPT(.title="Macro Load", .refresh=macro_refresh, .enter=macro_load_enter,.auto_scale=true);
}

void PlayMacro(void) {
    if (!g_playing) {
        if (g_event_count == 0) {
            fl_message(.message="No macro loaded", .timeout=3.0);
            return;
        }
        fl_message(.render=playback_render,.no_bg=true,.w=1920,.h=1080,.x=960,.y=540,.timeout=1000.0);
        fl_clear_key_queue();
        fl_clear_mouse_queue();
        g_playing = true;
        if (!g_play_thread) g_play_thread = fl_thread_create(playback_loop, NULL);
        return;
    }
    fl_dismiss_message();
    g_playing = false;
    release_everything();
}

void InfinitePlaybackToggle(void) {
    g_macro_infinite = !g_macro_infinite;
    if (g_macro_infinite) fl_message(.message="Infinite playback enabled", .timeout=1.0);
    else fl_message(.message="Infinite playback disabled", .timeout=1.0);
}

void RemoveDeadTimeToggle(void) {
    g_macro_remove_dead = !g_macro_remove_dead;
    if (g_macro_remove_dead) fl_message(.message="Remove dead time enabled", .timeout=1.0);
    else fl_message(.message="Remove dead time disabled", .timeout=1.0);
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


FLINT_FEATURE_INIT {
  fl_register_hotkey("Ctrl+Alt+R", RecordToggle);
  fl_add_to_cc(.display_name="SaveMacro",.on_enter=SaveMacro,.type="Macro",.icon="");
  fl_add_to_cc(.display_name="LoadMacro",.on_enter=LoadMacro,.type="Macro",.icon="");
  fl_register_hotkey("Ctrl+Alt+P", PlayMacro);
  fl_add_to_cc(.display_name="InfinitePlayback",.on_enter=InfinitePlaybackToggle,.type="Macro",.icon="");
  fl_add_to_cc(.display_name="RemoveDeadTime",.on_enter=RemoveDeadTimeToggle,.type="Macro",.icon="");

}
