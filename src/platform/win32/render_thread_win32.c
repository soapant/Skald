#include <windows.h>
#include "platform/render_thread.h"
#include "deps/log.h"

#define RT_MAX_CALLBACKS 16
#define RT_MAX_PENDING 16
#define RT_IDLE_SLEEP_MS 20
#define RT_DEFAULT_FPS 1

typedef struct {
    Platform_RenderFrameFn fn;
    void *ud;
} RtCallback;

static HANDLE g_rt_thread = NULL;
static CRITICAL_SECTION g_rt_lock;
static CONDITION_VARIABLE g_rt_cond;
static BOOL g_rt_lock_ready = FALSE;
static RtCallback g_rt_callbacks[RT_MAX_CALLBACKS];
static RtCallback g_rt_pending[RT_MAX_PENDING];
static int g_rt_count = 0;
static int g_rt_pending_count = 0;
static volatile LONG g_rt_quit = 0;
static int g_rt_fps = RT_DEFAULT_FPS;
static __thread bool g_rt_is_thread = false;

typedef struct { volatile LONG done; } RtSync;

static void rt_sync_mark(void *ud) {
    RtSync *s = (RtSync *)ud;
    if (s) InterlockedExchange(&s->done, 1);
}

void Platform_RenderThread_Sync(void) {
    if (!g_rt_thread) return;
    RtSync s;
    s.done = 0;
    if (!Platform_RenderThread_Post(rt_sync_mark, &s)) return;
    DWORD waited = 0;
    while (!s.done && waited < 5000) {
        Sleep(1);
        waited++;
    }
    if (!s.done) log_warn("render thread sync timed out after 5s");
}

bool Platform_RenderThread_EnsureStarted(int fps) {
    if (fps > 0) g_rt_fps = fps;
    if (g_rt_thread) return true;
    if (!g_rt_lock_ready) {
        InitializeCriticalSection(&g_rt_lock);
        InitializeConditionVariable(&g_rt_cond);
        g_rt_lock_ready = TRUE;
    }
    g_rt_quit = 0;
    g_rt_thread = CreateThread(NULL, 0, rt_thread_func, NULL, 0, NULL);
    if (!g_rt_thread) {
        log_warn("render thread: CreateThread failed, widget frames disabled");
        return false;
    }
    log_info("render thread started fps=%d", g_rt_fps);
    return true;
}

void Platform_RenderThread_Stop(void) {
    if (!g_rt_thread) return;
    InterlockedExchange(&g_rt_quit, 1);
    WakeConditionVariable(&g_rt_cond);
    WaitForSingleObject(g_rt_thread, INFINITE);
    CloseHandle(g_rt_thread);
    g_rt_thread = NULL;
    if (g_rt_lock_ready) {
        EnterCriticalSection(&g_rt_lock);
        g_rt_count = 0;
        g_rt_pending_count = 0;
        LeaveCriticalSection(&g_rt_lock);
    }
    log_info("render thread stopped");
}

bool Platform_RenderThread_AddFrameCallback(Platform_RenderFrameFn fn, void *ud) {
    if (!fn) return false;
    if (!Platform_RenderThread_EnsureStarted(0)) return false;
    EnterCriticalSection(&g_rt_lock);
    if (g_rt_count >= RT_MAX_CALLBACKS) {
        LeaveCriticalSection(&g_rt_lock);
        log_warn("render thread: callback table full");
        return false;
    }
    g_rt_callbacks[g_rt_count].fn = fn;
    g_rt_callbacks[g_rt_count].ud = ud;
    g_rt_count++;
    LeaveCriticalSection(&g_rt_lock);
    return true;
}

bool Platform_RenderThread_RemoveFrameCallback(Platform_RenderFrameFn fn, void *ud) {
    BOOL removed = FALSE;
    EnterCriticalSection(&g_rt_lock);
    for (int i = 0; i < g_rt_count; i++) {
        if (g_rt_callbacks[i].fn == fn && g_rt_callbacks[i].ud == ud) {
            for (int j = i; j < g_rt_count - 1; j++) g_rt_callbacks[j] = g_rt_callbacks[j + 1];
            g_rt_count--;
            removed = TRUE;
            break;
        }
    }
    LeaveCriticalSection(&g_rt_lock);
    return removed;
}
