#include "core/flapi.h"

#if !defined(_WIN32)
#include <pthread.h>
#endif

#define WM_SAMPLE_SECONDS 3
#define WM_MAX_TITLE 80
#define WM_MAX_ROWS 25

static FlHashMap* g_wm_map = NULL;
static bool g_wm_running = false;
static bool g_wm_inited = false;
#if !defined(_WIN32)
static pthread_t g_wm_thread;
static pthread_mutex_t g_wm_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

static void wm_track(const char* title) {
    if (!title || !title[0]) return;
    char short_title[WM_MAX_TITLE + 1];
    strncpy(short_title, title, WM_MAX_TITLE);
    short_title[WM_MAX_TITLE] = '\0';
#if !defined(_WIN32)
    pthread_mutex_lock(&g_wm_lock);
#endif
    fl_hashmap_add(&g_wm_map, short_title, WM_SAMPLE_SECONDS);
#if !defined(_WIN32)
    pthread_mutex_unlock(&g_wm_lock);
#endif
}

#if !defined(_WIN32)
static void* wm_sampler(void* arg) {
    (void)arg;
    while (g_wm_running) {
        char* id = fl_get_active_window_app_id();
        if (id) {
            wm_track(id);
            free(id);
        }
        fl_sleep(WM_SAMPLE_SECONDS);
    }
    return NULL;
}
#endif

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
#if !defined(_WIN32)
    pthread_mutex_lock(&g_wm_lock);
#endif
    size_t total = fl_hashmap_len(g_wm_map);
    FlHashMap* rows = NULL;
    if (total > 0) {
        rows = malloc(total * sizeof(*rows));
        for (size_t i = 0; i < total; i++) {
            const FlHashMap* e = fl_hashmap_at(g_wm_map, i);
            if (e) rows[i] = *e;
        }
    }
#if !defined(_WIN32)
    pthread_mutex_unlock(&g_wm_lock);
#endif
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
           .render = window_monitor_render);
}

FLINT_FEATURE_EXPORT void FlintFeatureShutdown(void) {
    g_wm_running = false;
#if !defined(_WIN32)
    if (g_wm_inited) pthread_join(g_wm_thread, NULL);
#endif
    g_wm_inited = false;
}

FLINT_FEATURE_INIT {
    if (g_wm_inited) return;
    g_wm_inited = true;
    fl_hashmap_init(&g_wm_map);
    g_wm_running = true;
#if !defined(_WIN32)
    pthread_create(&g_wm_thread, NULL, wm_sampler, NULL);
#else
    char* id = fl_get_active_window_app_id();
    if (id) {
        wm_track(id);
        free(id);
    }
#endif
    fl_register_hotkey("Alt+Shift+W", TriggerWindowMonitor);
    fl_register_feature("Window Monitor", TriggerWindowMonitor);
}
