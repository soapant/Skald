#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "helpers/helpers_render_thread.h"
#include "helpers/helpers_monitor.h"
#include "platform/platform.h"
#include "deps/log.h"

#define RT_MAX_CALLBACKS 16
#define RT_MAX_PENDING 16
#define RT_IDLE_SLEEP_US 20000
#define RT_DEFAULT_FPS 1
#define RT_MAX_SLEEP_US 16000

typedef struct {
  RenderFrameFn fn;
  void *ud;
} RtCallback;

static RtCallback g_callbacks[RT_MAX_CALLBACKS];
static RtCallback g_pending[RT_MAX_PENDING];
static int g_count = 0;
static int g_pending_count = 0;
static volatile bool g_running = false;
static volatile bool g_quit = false;
static __thread bool g_is_thread = false;
static int g_fps = RT_DEFAULT_FPS;

static void rt_run_pending(void) {
  RtCallback pend[RT_MAX_PENDING];
  int pn = 0;
  RenderThread_LockBackend();
  pn = g_pending_count;
  if (pn > 0) {
    memcpy(pend, g_pending, sizeof(RtCallback) * (size_t)pn);
    g_pending_count = 0;
  }
  RenderThread_UnlockBackend();
  for (int i = 0; i < pn; i++) {
    if (pend[i].fn)
      pend[i].fn(pend[i].ud);
  }
}

void RenderThread_Main(void) {
  g_is_thread = true;
  while (!g_quit) {
    RtCallback snap[RT_MAX_CALLBACKS];
    int n = 0;
    RenderThread_LockBackend();
    n = g_count;
    if (n > 0)
      memcpy(snap, g_callbacks, sizeof(RtCallback) * (size_t)n);
    RenderThread_UnlockBackend();
    rt_run_pending();
    for (int i = 0; i < n; i++) {
      if (snap[i].fn)
        snap[i].fn(snap[i].ud);
    }

    long us = (n == 0) ? RT_IDLE_SLEEP_US
                       : (1000000L / (g_fps > 0 ? g_fps : RT_DEFAULT_FPS));
    if (us > RT_MAX_SLEEP_US)
      us = RT_MAX_SLEEP_US;
    RenderThread_LockBackend();
    if (!g_quit && g_pending_count == 0)
      RenderThread_WaitTimedBackend(us);
    RenderThread_UnlockBackend();
  }
}

bool RenderThread_EnsureStarted(int fps) {
  if (fps > g_fps)
    g_fps = fps;
  if (g_running)
    return true;
  g_quit = false;
  if (!RenderThread_StartBackend()) {
    log_warn("render thread: backend start failed, widget frames disabled");
    return false;
  }
  g_running = true;
  return true;
}

void RenderThread_Stop(void) {
  if (!g_running)
    return;
  g_quit = true;
  RenderThread_WakeAllBackend();
  RenderThread_JoinBackend();
  g_running = false;
  RenderThread_LockBackend();
  g_count = 0;
  g_pending_count = 0;
  RenderThread_UnlockBackend();
}

bool RenderThread_Post(RenderFrameFn fn, void *ud) {
  if (!fn || !g_running)
    return false;
  RenderThread_LockBackend();
  if (g_pending_count >= RT_MAX_PENDING) {
    RenderThread_UnlockBackend();
    return false;
  }
  g_pending[g_pending_count].fn = fn;
  g_pending[g_pending_count].ud = ud;
  g_pending_count++;
  RenderThread_WakeOneBackend();
  RenderThread_UnlockBackend();
  return true;
}

void RenderThread_Wake(void) {
  if (!g_running)
    return;
  RenderThread_LockBackend();
  RenderThread_WakeOneBackend();
  RenderThread_UnlockBackend();
}

bool RenderThread_IsRunning(void) { return g_running; }

void *RenderThread_Display(void) { return RenderThread_DisplayBackend(); }

int RenderThread_CallbackCount(void) {
  RenderThread_LockBackend();
  int n = g_count;
  RenderThread_UnlockBackend();
  return n;
}

bool RenderThread_IsCurrentThread(void) { return g_is_thread; }

bool RenderThread_AddFrameCallback(RenderFrameFn fn, void *ud) {
  if (!fn)
    return false;
  if (!RenderThread_EnsureStarted(0))
    return false;
  RenderThread_LockBackend();
  if (g_count >= RT_MAX_CALLBACKS) {
    RenderThread_UnlockBackend();
    log_warn("render thread: callback table full");
    return false;
  }
  g_callbacks[g_count].fn = fn;
  g_callbacks[g_count].ud = ud;
  g_count++;
  RenderThread_UnlockBackend();
  return true;
}

bool RenderThread_RemoveFrameCallback(RenderFrameFn fn, void *ud) {
  bool removed = false;
  RenderThread_LockBackend();
  for (int i = 0; i < g_count; i++) {
    if (g_callbacks[i].fn == fn && g_callbacks[i].ud == ud) {
      for (int j = i; j < g_count - 1; j++)
        g_callbacks[j] = g_callbacks[j + 1];
      g_count--;
      removed = true;
      break;
    }
  }
  RenderThread_UnlockBackend();
  return removed;
}

typedef struct {
  volatile int done;
} RtSync;

static void rt_sync_mark(void *ud) {
  RtSync *s = (RtSync *)ud;
  if (s)
    s->done = 1;
}

void RenderThread_Sync(void) {
  if (!RenderThread_IsRunning())
    return;
  if (RenderThread_IsCurrentThread())
    return;
  RtSync *s = (RtSync *)malloc(sizeof(*s));
  if (!s)
    return;
  s->done = 0;
  if (!RenderThread_Post(rt_sync_mark, s)) {
    free(s);
    return;
  }
  long long start = Platform_GetWallClockMs();
  for (;;) {
    if (s->done)
      return;
    long long now = Platform_GetWallClockMs();
    if (now - start >= 5000) {
      log_warn("render thread sync timed out after 5s");
      return;
    }
    RenderThread_MiniSleepBackend(1);
  }
}
