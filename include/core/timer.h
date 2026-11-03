#ifndef CORE_TIMER_H
#define CORE_TIMER_H

#include <stdint.h>

#include "core/types.h"
#include "platform/platform.h"
#include "core/types/misc.h"

#define TIMER_LOG_CAP 65536
#define TIMER_BUF_LINES 8192
#define TIMER_BUF_LINE 256

static inline int64_t TimerNowTicks(void) { return Platform_TimerNowTicks(); }

static inline int64_t TimerTickFrequency(void) {
  return Platform_TimerTickFrequency();
}

static inline WinTimer TimerStart(void) {
  WinTimer t;
  t.frequency = TimerTickFrequency();
  t.start = TimerNowTicks();
  return t;
}

void TimerEnd(WinTimer t, const char *operation_name, const char *file,
              int line);
void TimerFlush(void);
double TimerElapsedUs(WinTimer t);
#endif
