#ifndef TIMER_H
#define TIMER_H

#include <stdio.h>
#include "core/types.h"
#include "config.h"

static inline WinTimer TimerStart() {
  WinTimer t;
  QueryPerformanceFrequency(&t.frequency);
  QueryPerformanceCounter(&t.start);
  return t;
}

static inline void TimerEnd(WinTimer t, const char* operation_name) {
  LARGE_INTEGER end;
  QueryPerformanceCounter(&end);

  double elapsed_us = (double)(end.QuadPart - t.start.QuadPart) * 1000000.0 / (double)t.frequency.QuadPart;

  if (DoTimeDebugs) {
    if (elapsed_us < 1000.0) {
      printf("[ TIMER] %-25s : %8.2f µs\n", operation_name, elapsed_us);
    } else if (elapsed_us < 1000000.0) {
      printf("[ TIMER] %-25s : %8.2f ms\n", operation_name, elapsed_us / 1000.0);
    } else {
      printf("[ TIMER] %-25s : %8.2f s\n", operation_name, elapsed_us / 1000000.0);
    }
  }
}

#endif
