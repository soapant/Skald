#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <X11/Xlib.h>
#include "platform/render_thread.h"
#include "deps/log.h"

#include "core/timer.h"
#define RT_MAX_CALLBACKS 16
#define RT_MAX_PENDING 16
#define RT_IDLE_SLEEP_US 20000
#define RT_DEFAULT_FPS 1
#define RT_MAX_SLEEP_US 16000

typedef struct {
    Platform_RenderFrameFn fn;
    void *ud;
} RtCallback;

static pthread_t g_rt_thread;
static pthread_mutex_t g_rt_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_rt_cond = PTHREAD_COND_INITIALIZER;static RtCallback g_rt_callbacks[RT_MAX_CALLBACKS];
static RtCallback g_rt_pending[RT_MAX_PENDING];
static int g_rt_count = 0;
static int g_rt_pending_count = 0;
static volatile bool g_rt_running = false;
static volatile bool g_rt_quit = false;
static __thread bool g_rt_is_thread = false;
static int g_rt_fps = RT_DEFAULT_FPS;
static Display *g_rt_dpy = NULL;

static void rt_run_pending(void) {
        PROFILE_START(_pt);
RtCallback pend[RT_MAX_PENDING];
    int pn = 0;
    pthread_mutex_lock(&g_rt_lock);
    pn = g_rt_pending_count;
    if (pn > 0) {
        memcpy(pend, g_rt_pending, sizeof(RtCallback) * (size_t)pn);
        g_rt_pending_count = 0;
    }
    pthread_mutex_unlock(&g_rt_lock);
    for (int i = 0; i < pn; i++) {
        if (pend[i].fn) pend[i].fn(pend[i].ud);
    }
}

bool Platform_RenderThread_Post(Platform_RenderFrameFn fn, void *ud) {
        PROFILE_START(_pt);
if (!fn || !g_rt_running) { PROFILE_END_VAR(_pt, "Platform_RenderThread_Post"); return false; }
    pthread_mutex_lock(&g_rt_lock);
    if (g_rt_pending_count >= RT_MAX_PENDING) {
        pthread_mutex_unlock(&g_rt_lock);
        return false;
    }
    g_rt_pending[g_rt_pending_count].fn = fn;
    g_rt_pending[g_rt_pending_count].ud = ud;
    g_rt_pending_count++;
    pthread_cond_signal(&g_rt_cond);
    pthread_mutex_unlock(&g_rt_lock);
    { PROFILE_END_VAR(_pt, "Platform_RenderThread_Post"); return true; }
}

void Platform_RenderThread_Wake(void) {
        PROFILE_START(_pt);
if (!g_rt_running) { PROFILE_END_VAR(_pt, "Platform_RenderThread_Wake"); return ; }
    pthread_mutex_lock(&g_rt_lock);
    pthread_cond_signal(&g_rt_cond);
    pthread_mutex_unlock(&g_rt_lock);
}

void *Platform_RenderThread_Display(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_RenderThread_Display"); return (void *)g_rt_dpy; }
}

bool Platform_RenderThread_IsRunning(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_RenderThread_IsRunning"); return g_rt_running; }
}

int Platform_RenderThread_CallbackCount(void) {
        PROFILE_START(_pt);
pthread_mutex_lock(&g_rt_lock);
    int n = g_rt_count;
    pthread_mutex_unlock(&g_rt_lock);
    { PROFILE_END_VAR(_pt, "Platform_RenderThread_CallbackCount"); return n; }
}

bool Platform_RenderThread_IsCurrentThread(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "Platform_RenderThread_IsCurrentThread"); return g_rt_is_thread; }
}

static void *rt_thread_func(void *arg) {
        PROFILE_START(_pt);
(void)arg;
    g_rt_is_thread = true;
    while (!g_rt_quit) {
        RtCallback snap[RT_MAX_CALLBACKS];
        int n = 0;
        pthread_mutex_lock(&g_rt_lock);
        n = g_rt_count;
        if (n > 0) memcpy(snap, g_rt_callbacks, sizeof(RtCallback) * (size_t)n);
        pthread_mutex_unlock(&g_rt_lock);
        rt_run_pending();
        for (int i = 0; i < n; i++) {
            if (snap[i].fn) snap[i].fn(snap[i].ud);
        }

        long us = (n == 0) ? RT_IDLE_SLEEP_US : (1000000L / (g_rt_fps > 0 ? g_rt_fps : RT_DEFAULT_FPS));
        if (us > RT_MAX_SLEEP_US) us = RT_MAX_SLEEP_US;
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += (time_t)(us / 1000000L);
        ts.tv_nsec += (long)(us % 1000000L) * 1000L;
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000L;
        }
        pthread_mutex_lock(&g_rt_lock);
        if (!g_rt_quit && g_rt_pending_count == 0) {
            pthread_cond_timedwait(&g_rt_cond, &g_rt_lock, &ts);
        }
        pthread_mutex_unlock(&g_rt_lock);
    }
    { PROFILE_END_VAR(_pt, "rt_thread_func"); return NULL; }
}

typedef struct { volatile int done; } RtSync;

static void rt_sync_mark(void *ud) {
        PROFILE_START(_pt);
RtSync *s = (RtSync *)ud;
    if (s) s->done = 1;
}

void Platform_RenderThread_Sync(void) {
        PROFILE_START(_pt);
if (!g_rt_running) { PROFILE_END_VAR(_pt, "Platform_RenderThread_Sync"); return ; }
    if (Platform_RenderThread_IsCurrentThread()) { PROFILE_END_VAR(_pt, "Platform_RenderThread_Sync"); return ; }
    RtSync s;
    s.done = 0;
    if (!Platform_RenderThread_Post(rt_sync_mark, &s)) { PROFILE_END_VAR(_pt, "Platform_RenderThread_Sync"); return ; }
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 5;
    for (;;) {
        if (s.done) return;
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec > deadline.tv_sec) {
            log_warn("render thread sync timed out after 5s");
            return;
        }
        struct timespec nap = { 0, 200000 };
        nanosleep(&nap, NULL);
    }
}

bool Platform_RenderThread_EnsureStarted(int fps) {
        PROFILE_START(_pt);
if (fps > g_rt_fps) g_rt_fps = fps;
    if (g_rt_running) { PROFILE_END_VAR(_pt, "Platform_RenderThread_EnsureStarted"); return true; }
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        log_warn("render thread: XOpenDisplay failed, widget frames disabled");
        return false;
    }
    g_rt_dpy = dpy;
    g_rt_quit = false;
    if (pthread_create(&g_rt_thread, NULL, rt_thread_func, NULL) != 0) {
        XCloseDisplay(dpy);
        g_rt_dpy = NULL;
        log_warn("render thread: pthread_create failed");
        return false;
    }
    g_rt_running = true;
    { PROFILE_END_VAR(_pt, "Platform_RenderThread_EnsureStarted"); return true; }
}

void Platform_RenderThread_Stop(void) {
        PROFILE_START(_pt);
if (!g_rt_running) { PROFILE_END_VAR(_pt, "Platform_RenderThread_Stop"); return ; }
    g_rt_quit = true;
    pthread_mutex_lock(&g_rt_lock);
    pthread_cond_broadcast(&g_rt_cond);
    pthread_mutex_unlock(&g_rt_lock);
    pthread_join(g_rt_thread, NULL);
    g_rt_running = false;
    if (g_rt_dpy) {
        XCloseDisplay(g_rt_dpy);
        g_rt_dpy = NULL;
    }
    pthread_mutex_lock(&g_rt_lock);
    g_rt_count = 0;
    g_rt_pending_count = 0;
    pthread_mutex_unlock(&g_rt_lock);
}

bool Platform_RenderThread_AddFrameCallback(Platform_RenderFrameFn fn, void *ud) {
        PROFILE_START(_pt);
if (!fn) { PROFILE_END_VAR(_pt, "Platform_RenderThread_AddFrameCallback"); return false; }
    if (!Platform_RenderThread_EnsureStarted(0)) { PROFILE_END_VAR(_pt, "Platform_RenderThread_AddFrameCallback"); return false; }
    pthread_mutex_lock(&g_rt_lock);
    if (g_rt_count >= RT_MAX_CALLBACKS) {
        pthread_mutex_unlock(&g_rt_lock);
        log_warn("render thread: callback table full");
        return false;
    }
    g_rt_callbacks[g_rt_count].fn = fn;
    g_rt_callbacks[g_rt_count].ud = ud;
    g_rt_count++;
    pthread_mutex_unlock(&g_rt_lock);
    { PROFILE_END_VAR(_pt, "Platform_RenderThread_AddFrameCallback"); return true; }
}

bool Platform_RenderThread_RemoveFrameCallback(Platform_RenderFrameFn fn, void *ud) {
        PROFILE_START(_pt);
bool removed = false;
    pthread_mutex_lock(&g_rt_lock);
    for (int i = 0; i < g_rt_count; i++) {
        if (g_rt_callbacks[i].fn == fn && g_rt_callbacks[i].ud == ud) {
            for (int j = i; j < g_rt_count - 1; j++) g_rt_callbacks[j] = g_rt_callbacks[j + 1];
            g_rt_count--;
            removed = true;
            break;
        }
    }
    pthread_mutex_unlock(&g_rt_lock);
    { PROFILE_END_VAR(_pt, "Platform_RenderThread_RemoveFrameCallback"); return removed; }
}
