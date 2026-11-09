#include "platform/render_thread.h"

#include "platform/platform.h"

#include <stdbool.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

#include "deps/log.h"

typedef struct {
  volatile int done;
} RtSync;

static void rt_sync_mark(void *ud) {

  RtSync *s = (RtSync *)ud;
  if (s)
    s->done = 1;
}

void RenderThread_Sync(void) {
  if (!Platform_RenderThread_IsRunning())
    return;
  if (Platform_RenderThread_IsCurrentThread())
    return;
  RtSync *s = (RtSync *)malloc(sizeof(*s));
  if (!s)
    return;
  s->done = 0;
  if (!Platform_RenderThread_Post(rt_sync_mark, s)) {
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
#if defined(_WIN32)
    Sleep(1);
#else
    struct timespec nap = {0, 200000};
    nanosleep(&nap, NULL);
#endif
  }
}
