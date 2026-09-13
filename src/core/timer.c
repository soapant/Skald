#include "core/timer.h"
#include "deps/log.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void TimerEnd(WinTimer t, const char* operation_name) {
  double elapsed_us;
#if defined(_WIN32)
  LARGE_INTEGER end;
  QueryPerformanceCounter(&end);
  elapsed_us = (double)(end.QuadPart - t.start.QuadPart) * 1000000.0 / (double)t.frequency.QuadPart;
#else
  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &end);
  elapsed_us = (double)(end.tv_sec - t.start.tv_sec) * 1000000.0 + (double)(end.tv_nsec - t.start.tv_nsec) / 1000.0;
#endif

  if (DoTimeDebugs) {
    char line[256];
    if (elapsed_us < 1000.0) {
      line[0] = '\0';
    } else if (elapsed_us < 1000000.0) {
      snprintf(line, sizeof(line), "[ TIMER] %-25s : %8.2f ms\n", operation_name, elapsed_us / 1000.0);
    } else {
      snprintf(line, sizeof(line), "[ TIMER] %-25s : %8.2f s\n", operation_name, elapsed_us / 1000000.0);
    }
    printf("%s", line);
  }
}
