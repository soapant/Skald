#include <pthread.h>
#include <time.h>
#include <X11/Xlib.h>
#include <stdbool.h>
#include <errno.h>

#include "helpers/helpers_render_thread.h"
#include "platform/platform.h"
#include "deps/log.h"

static pthread_t g_rt_thread;
static pthread_mutex_t g_rt_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_rt_cond = PTHREAD_COND_INITIALIZER;
static Display *rt_dpy = NULL;

void Platform_RenderThreadInit(void) {}
void Platform_RenderThreadLock(void) { pthread_mutex_lock(&g_rt_lock); }
void Platform_RenderThreadUnlock(void) { pthread_mutex_unlock(&g_rt_lock); }
void Platform_RenderThreadWakeOne(void) { pthread_cond_signal(&g_rt_cond); }
void Platform_RenderThreadWakeAll(void) { pthread_cond_broadcast(&g_rt_cond); }
void Platform_RenderThreadWaitTimed(long us) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += (time_t)(us / 1000000L);
  ts.tv_nsec += (long)(us % 1000000L) * 1000L;
  if (ts.tv_nsec >= 1000000000L) {
    ts.tv_sec += 1;
    ts.tv_nsec -= 1000000000L;
  }
  pthread_cond_timedwait(&g_rt_cond, &g_rt_lock, &ts);
}

void Platform_RenderThreadMiniSleep(int milliseconds) {
  struct timespec ts;
  ts.tv_sec = (time_t)(milliseconds / 1000);
  ts.tv_nsec = ((long)milliseconds % 1000) * 1000000L;
  while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
  }
}

static void *rt_entry(void *arg) {
  (void)arg;
  render_thread_main();
  return NULL;
}

bool Platform_RenderThreadStart(void) {
  Display *dpy = XOpenDisplay(NULL);
  if (!dpy) {
    log_warn("render thread: XOpenDisplay failed, widget frames disabled");
    return false;
  }
  rt_dpy = dpy;
  if (pthread_create(&g_rt_thread, NULL, rt_entry, NULL) != 0) {
    XCloseDisplay(dpy);
    rt_dpy = NULL;
    log_warn("render thread: pthread_create failed");
    return false;
  }
  return true;
}

void Platform_RenderThreadJoin(void) {
  pthread_join(g_rt_thread, NULL);
  if (rt_dpy) {
    XCloseDisplay(rt_dpy);
    rt_dpy = NULL;
  }
}

void *Platform_RenderThreadDisplay(void) { return (void *)rt_dpy; }
