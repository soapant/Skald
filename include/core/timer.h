#ifndef CORE_TIMER_H
#define CORE_TIMER_H

#include <stdint.h>

#include "core/types.h"
#include "platform/platform.h"

#define TIMER_LOG_CAP 65536
#define TIMER_BUF_LINES 8192
#define TIMER_BUF_LINE 256

static inline int64_t TimerNowTicks(void) { return Platform_TimerNowTicks(); }

static inline int64_t TimerTickFrequency(void) {
  return Platform_TimerTickFrequency();
}

void TimerFlush(void);
#endif
