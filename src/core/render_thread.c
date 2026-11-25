#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "helpers/helpers_render_thread.h"
#include "helpers/helpers_monitor.h"
#include "platform/platform.h"
#include "deps/log.h"
#include "deps/stb_ds.h"

#define RT_IDLE_SLEEP_US 20000
#define RT_DEFAULT_FPS 1
#define RT_MAX_SLEEP_US 16000
#define RT_SYNC_TIMEOUT_MS 5000

typedef struct {
  RenderFrameFn fn;
  void *ud;
} RtCallback;

static RtCallback *callbacks;
static RtCallback *pending;
static __thread RtCallback *snap;
#define count arrlenu(callbacks)
#define pending_count arrlenu(pending)

static atomic_bool g_running = false;
static atomic_bool g_quit = false;
static atomic_int g_frame_rate = RT_DEFAULT_FPS;
static __thread bool is_thread = false;

static void rt_run_pending(void) {
  Platform_RenderThreadLock();
  RtCallback *pend = pending;
  pending = NULL;
  Platform_RenderThreadUnlock();
  if (!pend)
    return;
  for (size_t i = 0; i < arrlenu(pend); i++) {
    if (pend[i].fn)
      pend[i].fn(pend[i].ud);
  }
  arrfree(pend);
}

void render_thread_main(void) {
  is_thread = true;
  while (!atomic_load_explicit(&g_quit, memory_order_acquire)) {
    rt_run_pending();
    size_t n = 0;
    Platform_RenderThreadLock();
    n = count;
    arrsetlen(snap, n);
    if (n > 0)
      memcpy(snap, callbacks, sizeof(*snap) * n);
    Platform_RenderThreadUnlock();
    for (size_t i = 0; i < n; i++) {
      if (snap[i].fn)
        snap[i].fn(snap[i].ud);
    }

    int fps = atomic_load_explicit(&g_frame_rate, memory_order_relaxed);
    long us = (n == 0) ? RT_IDLE_SLEEP_US
                       : (1000000L / (fps > 0 ? fps : RT_DEFAULT_FPS));
    if (us > RT_MAX_SLEEP_US)
      us = RT_MAX_SLEEP_US;
    Platform_RenderThreadLock();
    if (!atomic_load_explicit(&g_quit, memory_order_acquire) &&
        pending_count == 0)
      Platform_RenderThreadWaitTimed(us);
    Platform_RenderThreadUnlock();
  }
  arrfree(snap);
  snap = NULL;
}

bool render_thread_ensure_started(int fps) {
  int rate = atomic_load_explicit(&g_frame_rate, memory_order_relaxed);
  while (fps > rate &&
         !atomic_compare_exchange_weak_explicit(&g_frame_rate, &rate, fps,
                                                memory_order_relaxed,
                                                memory_order_relaxed)) {
  }
  if (atomic_load_explicit(&g_running, memory_order_acquire))
    return true;
  atomic_store_explicit(&g_quit, false, memory_order_release);
  if (!Platform_RenderThreadStart()) {
    log_warn("render thread: backend start failed, widget frames disabled");
    return false;
  }
  atomic_store_explicit(&g_running, true, memory_order_release);
  return true;
}

void render_thread_stop(void) {
  if (!atomic_load_explicit(&g_running, memory_order_acquire))
    return;
  atomic_store_explicit(&g_quit, true, memory_order_release);
  Platform_RenderThreadWakeAll();
  Platform_RenderThreadJoin();
  atomic_store_explicit(&g_running, false, memory_order_release);

  Platform_RenderThreadLock();
  RtCallback *undelivered = pending;
  pending = NULL;
  Platform_RenderThreadUnlock();
  for (size_t i = 0; i < arrlenu(undelivered); i++) {
    if (undelivered[i].fn)
      undelivered[i].fn(undelivered[i].ud);
  }
  arrfree(undelivered);

  Platform_RenderThreadLock();
  arrfree(callbacks);
  arrfree(pending);
  Platform_RenderThreadUnlock();
}

bool render_thread_post(RenderFrameFn fn, void *ud) {
  if (!fn || !atomic_load_explicit(&g_running, memory_order_acquire) ||
      atomic_load_explicit(&g_quit, memory_order_acquire))
    return false;
  Platform_RenderThreadLock();
  RtCallback *e = arraddnptr(pending, 1);
  e->fn = fn;
  e->ud = ud;
  Platform_RenderThreadWakeOne();
  Platform_RenderThreadUnlock();
  return true;
}

void render_thread_wake(void) {
  if (!atomic_load_explicit(&g_running, memory_order_acquire))
    return;
  Platform_RenderThreadLock();
  Platform_RenderThreadWakeOne();
  Platform_RenderThreadUnlock();
}

bool render_thread_is_running(void) {
  return atomic_load_explicit(&g_running, memory_order_acquire);
}

void *render_thread_display(void) { return Platform_RenderThreadDisplay(); }

bool render_thread_is_current_thread(void) { return is_thread; }

bool render_thread_add_frame_callback(RenderFrameFn fn, void *ud) {
  if (!fn)
    return false;
  if (!render_thread_ensure_started(0))
    return false;
  Platform_RenderThreadLock();
  RtCallback *e = arraddnptr(callbacks, 1);
  e->fn = fn;
  e->ud = ud;
  Platform_RenderThreadUnlock();
  return true;
}

bool render_thread_remove_frame_callback(RenderFrameFn fn, const void *ud) {
  bool removed = false;
  Platform_RenderThreadLock();
  for (size_t i = 0; i < count; i++) {
    if (callbacks[i].fn == fn && callbacks[i].ud == ud) {
      arrdel(callbacks, i);
      removed = true;
      break;
    }
  }
  Platform_RenderThreadUnlock();
  return removed;
}

typedef enum {
  RT_SYNC_PENDING = 0,
  RT_SYNC_DONE,
  RT_SYNC_ABANDONED
} RtSyncState;

typedef struct {
  atomic_int state;
} RtSync;

static void rt_sync_mark(void *ud) {
  RtSync *s = (RtSync *)ud;
  if (!s)
    return;
  int expected = RT_SYNC_PENDING;
  if (atomic_compare_exchange_strong_explicit(&s->state, &expected, RT_SYNC_DONE,
                                              memory_order_acq_rel,
                                              memory_order_acquire))
    return;
  if (expected == RT_SYNC_ABANDONED)
    free(s);
}

void render_thread_sync(void) {
  if (!render_thread_is_running())
    return;
  if (render_thread_is_current_thread())
    return;
  RtSync *s = (RtSync *)malloc(sizeof(*s));
  if (!s)
    return;
  atomic_store_explicit(&s->state, RT_SYNC_PENDING, memory_order_relaxed);
  if (!render_thread_post(rt_sync_mark, s)) {
    free(s);
    return;
  }
  long long start = Platform_GetWallClockMs();
  for (;;) {
    int state = atomic_load_explicit(&s->state, memory_order_acquire);
    if (state == RT_SYNC_DONE) {
      free(s);
      return;
    }
    if (state == RT_SYNC_ABANDONED)
      return;
    if (Platform_GetWallClockMs() - start >= RT_SYNC_TIMEOUT_MS) {
      log_warn("render thread sync timed out after 5s");
      int expected = RT_SYNC_PENDING;
      atomic_compare_exchange_strong_explicit(
          &s->state, &expected, RT_SYNC_ABANDONED, memory_order_acq_rel,
          memory_order_acquire);
      if (expected == RT_SYNC_DONE)
        free(s);
      return;
    }
    Platform_RenderThreadMiniSleep(1);
  }
}
