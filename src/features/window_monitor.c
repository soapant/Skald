#include "core/flapi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define WM_SAMPLE_SECONDS 1
#define WM_MAX_TITLE 80
#define WM_MAX_ROWS 25

static FlHashMap* g_wm_map = NULL;
static bool g_wm_running = false;
static bool g_wm_inited = false;
static char g_wm_last[WM_MAX_TITLE + 1] = {0};
static FlThread g_wm_thread = NULL;
static FlMutex g_wm_lock = NULL;

static void wm_track(const char* title) {
    if (!title || !title[0]) return;
    char short_title[WM_MAX_TITLE + 1];
    strncpy(short_title, title, WM_MAX_TITLE);
    short_title[WM_MAX_TITLE] = '\0';
    fl_mutex_lock(g_wm_lock);
    fl_hashmap_add(&g_wm_map, short_title, WM_SAMPLE_SECONDS);
    fl_mutex_unlock(g_wm_lock);
}

static void* wm_sampler(void* arg) {
    (void)arg;
    while (g_wm_running) {
        char* id = fl_get_active_window_app_id();
        if (id) {
            strncpy(g_wm_last, id, WM_MAX_TITLE);
            g_wm_last[WM_MAX_TITLE] = '\0';
            wm_track(id);
            free(id);
        } else if (g_wm_last[0]) {
            wm_track(g_wm_last);
        }
        fl_sleep(WM_SAMPLE_SECONDS);
    }
    return NULL;
}

static void wm_format_time(long total, char* out, size_t cap) {
    long h = total / 3600;
    long m = (total % 3600) / 60;
    long s = total % 60;
    if (h > 0) snprintf(out, cap, "%ldh %ldm", h, m);
    else if (m > 0) snprintf(out, cap, "%ldm %lds", m, s);
    else snprintf(out, cap, "%lds", s);
}

static int wm_cmp_desc(const void* a, const void* b) {
    const FlHashMap* ea = (const FlHashMap*)a;
    const FlHashMap* eb = (const FlHashMap*)b;
    if (ea->value < eb->value) return 1;
    if (ea->value > eb->value) return -1;
    return 0;
}

void window_monitor_render(PromptState* state) {
    fl_draw_text(.text = "Window Monitor", .fontsize = 22);
    fl_mutex_lock(g_wm_lock);
    size_t total = fl_hashmap_len(g_wm_map);
    FlHashMap* rows = NULL;
    if (total > 0) {
        rows = malloc(total * sizeof(*rows));
        for (size_t i = 0; i < total; i++) {
            const FlHashMap* e = fl_hashmap_at(g_wm_map, i);
            if (e) rows[i] = *e;
        }
    }
    fl_mutex_unlock(g_wm_lock);
    if (total > 0) qsort(rows, total, sizeof(rows[0]), wm_cmp_desc);
    size_t n = total > WM_MAX_ROWS ? WM_MAX_ROWS : total;
    int y = 60;
    for (size_t i = 0; i < n; i++) {
        char line[256];
        char spent[32];
        wm_format_time(rows[i].value, spent, sizeof(spent));
        snprintf(line, sizeof(line), "%s  -  %s", rows[i].key ? rows[i].key : "", spent);
        fl_draw_text(.y = y, .text = line);
        y += 28;
    }
    if (n == 0) {
        fl_draw_text(.y = 60, .text = "No windows tracked yet", .fontsize = 16);
    }
    free(rows);
}

void TriggerWindowMonitor(void) {
    PROMPT(.title = "Window Monitor",
           .tick = true,
           .render = window_monitor_render);
}

FLINT_FEATURE_EXPORT void FlintFeatureShutdown(void) {
    g_wm_running = false;
    if (g_wm_inited) fl_thread_join(g_wm_thread);
    fl_mutex_destroy(g_wm_lock);
    g_wm_lock = NULL;
    g_wm_inited = false;
}

FLINT_FEATURE_INIT {
    if (g_wm_inited) return;
    g_wm_inited = true;
    fl_hashmap_init(&g_wm_map);
    g_wm_lock = fl_mutex_create();
    g_wm_running = true;
    g_wm_thread = fl_thread_create(wm_sampler, NULL);
    fl_register_hotkey("Alt+Shift+W", TriggerWindowMonitor);
    fl_register_feature("Window Monitor", TriggerWindowMonitor);
}
