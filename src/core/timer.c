#include "core/timer.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char g_timer_buffer[TIMER_LOG_CAP];
size_t g_timer_buffer_len = 0;
int g_timer_atexit_registered = 0;
int g_timer_file_initialized = 0;

void TimerBufferFlush(void) {
  if (g_timer_buffer_len == 0) return;
  const char* path = "time_logs.txt";
  FILE* f = fopen(path, "a");
  if (f) {
    fwrite(g_timer_buffer, 1, g_timer_buffer_len, f);
    fclose(f);
  }
  g_timer_buffer_len = 0;
  g_timer_buffer[0] = '\0';
}

static void TimerAtexitFlush(void) {
  TimerBufferFlush();
}

void TimerBufferAppend(const char* line) {
  size_t len = strlen(line);
  if (len == 0) return;
  if (len >= TIMER_LOG_CAP) {
    FILE* f = fopen("time_logs.txt", "a");
    if (f) { fwrite(line, 1, len, f); fclose(f); }
    return;
  }
  if (len >= TIMER_LOG_CAP - g_timer_buffer_len) {
    TimerBufferFlush();
  }
  memcpy(g_timer_buffer + g_timer_buffer_len, line, len);
  g_timer_buffer_len += len;
  g_timer_buffer[g_timer_buffer_len] = '\0';
  if (!g_timer_atexit_registered) {
    atexit(TimerAtexitFlush);
    g_timer_atexit_registered = 1;
  }
}

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
    if (line[0] != '\0') TimerBufferAppend(line);
  }
}