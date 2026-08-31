#ifndef CORE_TIMER_H
#define CORE_TIMER_H

#define TIMER_LOG_CAP 4096

static char g_timer_buffer[TIMER_LOG_CAP];
static size_t g_timer_buffer_len = 0;
static int g_timer_atexit_registered = 0;

static void TimerBufferFlush(void);

static void TimerAtexitFlush(void) {
  TimerBufferFlush();
}

static void TimerBufferFlush(void) {
  if (g_timer_buffer_len > 0) {
    fwrite(g_timer_buffer, 1, g_timer_buffer_len, stdout);
    g_timer_buffer_len = 0;
    g_timer_buffer[0] = '\0';
  }
}

static void TimerBufferAppend(const char* line) {
  size_t len = strlen(line);
  if (len == 0) return;
  if (len >= TIMER_LOG_CAP - g_timer_buffer_len) {
    TimerBufferFlush();
    if (len >= TIMER_LOG_CAP) return;
  }
  memcpy(g_timer_buffer + g_timer_buffer_len, line, len);
  g_timer_buffer_len += len;
  g_timer_buffer[g_timer_buffer_len] = '\0';
  if (!g_timer_atexit_registered) {
    atexit(TimerAtexitFlush);
    g_timer_atexit_registered = 1;
  }
}

static inline WinTimer TimerStart() {
  WinTimer t;
#if defined(_WIN32)
  QueryPerformanceFrequency(&t.frequency);
  QueryPerformanceCounter(&t.start);
#else
  clock_gettime(CLOCK_MONOTONIC, &t.start);
#endif
  return t;
}

static inline void TimerEnd(WinTimer t, const char* operation_name) {
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
    char line[128];
    if (elapsed_us < 1000.0) {
      snprintf(line, sizeof(line), "[ TIMER] %-25s : %8.2f µs\n", operation_name, elapsed_us);
    } else if (elapsed_us < 1000000.0) {
      snprintf(line, sizeof(line), "[ TIMER] %-25s : %8.2f ms\n", operation_name, elapsed_us / 1000.0);
    } else {
      snprintf(line, sizeof(line), "[ TIMER] %-25s : %8.2f s\n", operation_name, elapsed_us / 1000000.0);
    }
    TimerBufferAppend(line);

    if (strstr(operation_name, "TOTAL") != NULL) {
      TimerBufferFlush();
    }
  }
}

#endif
