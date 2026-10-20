#include "core/timer.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>

static double timer_elapsed_us(WinTimer t) {
#if defined(_WIN32)
  LARGE_INTEGER end;
  QueryPerformanceCounter(&end);
  return (double)(end.QuadPart - t.start.QuadPart) * 1000000.0 / (double)t.frequency.QuadPart;
#else
  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &end);
  return (double)(end.tv_sec - t.start.tv_sec) * 1000000.0 + (double)(end.tv_nsec - t.start.tv_nsec) / 1000.0;
#endif
}

double TimerElapsedUs(WinTimer t) {
  return timer_elapsed_us(t);
}

static void timer_print(double elapsed_us, const char *operation_name,
                        const char *file, int line) {
  char line_buf[TIMER_BUF_LINE];
  if (elapsed_us < 1000000.0) {
    snprintf(line_buf, sizeof(line_buf), "[ TIMER] %s:%d %-38s : %8.3f ms\n",
             file, line, operation_name, elapsed_us / 1000.0);
  } else {
    snprintf(line_buf, sizeof(line_buf), "[ TIMER] %s:%d %-38s : %8.2f s\n",
             file, line, operation_name, elapsed_us / 1000000.0);
  }
  printf("%s", line_buf);
}

void TimerPrintRaw(double elapsed_us, const char* operation_name,
                   const char *file, int line) {
  if (!DoTimeDebugs) return;
  timer_print(elapsed_us, operation_name, file, line);
}

void TimerEnd(WinTimer t, const char* operation_name,
              const char *file, int line) {
  double elapsed_us = timer_elapsed_us(t);
  if (DoTimeDebugs) {
    if (elapsed_us < 1000.0) return;
    timer_print(elapsed_us, operation_name, file, line);
  }
}
