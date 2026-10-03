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

static char g_timer_buf[TIMER_BUF_LINES][TIMER_BUF_LINE];
static long g_timer_buf_count = 0;
static int g_timer_atexit = 0;

static void timer_flush_buf(void) {
  long n = g_timer_buf_count;
  if (n > TIMER_BUF_LINES) n = TIMER_BUF_LINES;
  for (long i = 0; i < n; i++) fputs(g_timer_buf[i], stdout);
  fflush(stdout);
  g_timer_buf_count = 0;
}

static void timer_print(double elapsed_us, const char *operation_name) {
  char line[TIMER_BUF_LINE];
  if (elapsed_us < 1000000.0) {
    snprintf(line, sizeof(line), "[ TIMER] %-38s : %8.3f ms\n", operation_name, elapsed_us / 1000.0);
  } else {
    snprintf(line, sizeof(line), "[ TIMER] %-38s : %8.2f s\n", operation_name, elapsed_us / 1000000.0);
  }
  if (g_timer_buf_count < TIMER_BUF_LINES) {
    snprintf(g_timer_buf[g_timer_buf_count], TIMER_BUF_LINE, "%s", line);
    g_timer_buf_count++;
  }
  if (!g_timer_atexit) { g_timer_atexit = 1; atexit(timer_flush_buf); }
}

void TimerFlush(void) {
  if (!g_timer_atexit) return;
  timer_flush_buf();
}

void TimerPrintRaw(double elapsed_us, const char* operation_name) {
  if (!DoTimeDebugs) return;
  timer_print(elapsed_us, operation_name);
}

void TimerEnd(WinTimer t, const char* operation_name) {
  double elapsed_us = timer_elapsed_us(t);
  if (DoTimeDebugs) {
    if (elapsed_us < 100.0) return;
    timer_print(elapsed_us, operation_name);
  }
}

