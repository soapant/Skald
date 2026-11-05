#include "core/timer.h"

#include <stdio.h>

static double timer_elapsed_us(WinTimer t) {
  int64_t end = TimerNowTicks();
  if (t.frequency <= 0)
    return 0.0;
  return (double)(end - t.start) * 1000000.0 / (double)t.frequency;
}

double TimerElapsedUs(WinTimer t) { return timer_elapsed_us(t); }

