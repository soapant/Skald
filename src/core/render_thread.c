#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "helpers/helpers_render_thread.h"
#include "helpers/helpers_monitor.h"
#include "platform/platform.h"
#include "deps/log.h"
#include "deps/stb_ds.h"

#define RT_IDLE_SLEEP_US 20000
#define RT_DEFAULT_FPS 1
#define RT_MAX_SLEEP_US 16000

typedef struct {
  RenderFrameFn fn;
  void *ud;
} RtCallback;

static RtCallback *callbacks;
static RtCallback *pending;
#define count arrlenu(callbacks)
#define pending_count arrlenu(pending)
static volatile bool running = false;
static volatile bool quit = false;
static __thread bool is_thread = false;
static int frame_rate = RT_DEFAULT_FPS;

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
  while (!quit) {
    RtCallback *snap = NULL;
    size_t n = 0;
    Platform_RenderThreadLock();
    n = count;
    if (n > 0) {
      RtCallback *dst = arraddnptr(snap, n);
      memcpy(dst, callbacks, sizeof(*dst) * n);
    }
    Platform_RenderThreadUnlock();
    rt_run_pending();
    for (size_t i = 0; i < n; i++) {
      if (snap[i].fn)
        snap[i].fn(snap[i].ud);
    }
    arrfree(snap);

    long us = (n == 0)
                  ? RT_IDLE_SLEEP_US
                  : (1000000L / (frame_rate > 0 ? frame_rate : RT_DEFAULT_FPS));
    if (us > RT_MAX_SLEEP_US)
      us = RT_MAX_SLEEP_US;
    Platform_RenderThreadLock();
    if (!quit && pending_count == 0)
      Platform_RenderThreadWaitTimed(us);
    Platform_RenderThreadUnlock();
  }
}

bool render_thread_ensure_started(int fps) {
  if (fps > frame_rate)
    frame_rate = fps;
  if (running)
    return true;
  quit = false;
  if (!Platform_RenderThreadStart()) {
    log_warn("render thread: backend start failed, widget frames disabled");
    return false;
  }
  running = true;
  return true;
}

void render_thread_stop(void) {
  if (!running)
    return;
  quit = true;
  Platform_RenderThreadWakeAll();
  Platform_RenderThreadJoin();
  running = false;
  Platform_RenderThreadLock();
  arrfree(callbacks);
  arrfree(pending);
  Platform_RenderThreadUnlock();
}

bool render_thread_post(RenderFrameFn fn, void *ud) {
  if (!fn || !running)
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
  if (!running)
    return;
  Platform_RenderThreadLock();
  Platform_RenderThreadWakeOne();
  Platform_RenderThreadUnlock();
}

bool render_thread_is_running(void) { return running; }

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

typedef struct {
  volatile int done;
  volatile int abandoned;
} RtSync;

static void rt_sync_mark(void *ud) {
  RtSync *s = (RtSync *)ud;
  if (!s)
    return;
  if (s->abandoned) {
    free(s);
    return;
  }
  s->done = 1;
}

void render_thread_sync(void) {
  if (!render_thread_is_running())
    return;
  if (render_thread_is_current_thread())
    return;
  RtSync *s = (RtSync *)malloc(sizeof(*s));
  if (!s)
    return;
  s->done = 0;
  s->abandoned = 0;
  if (!render_thread_post(rt_sync_mark, s)) {
    free(s);
    return;
  }
  long long start = Platform_GetWallClockMs();
  for (;;) {
    if (s->done) {
      free(s);
      return;
    }
    long long now = Platform_GetWallClockMs();
    if (now - start >= 5000) {
      log_warn("render thread sync timed out after 5s");
      s->abandoned = 1;
      return;
    }
    Platform_RenderThreadMiniSleep(1);
  }
}
