#ifndef CORE_TIMER_H
#define CORE_TIMER_H

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif
#include "core/types.h"

#define TIMER_LOG_CAP 65536
#define TIMER_BUF_LINES 8192
#define TIMER_BUF_LINE 256

static inline int64_t TimerNowTicks(void) {
#if defined(_WIN32)
  LARGE_INTEGER c;
  QueryPerformanceCounter(&c);
  return (int64_t)c.QuadPart;
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
#endif
}

static inline int64_t TimerTickFrequency(void) {
#if defined(_WIN32)
  LARGE_INTEGER f;
  QueryPerformanceFrequency(&f);
  return (int64_t)f.QuadPart;
#else
  return 1000000000LL;
#endif
}

static inline WinTimer TimerStart(void) {
  WinTimer t;
  t.frequency = TimerTickFrequency();
  t.start = TimerNowTicks();
  return t;
}

void TimerEnd(WinTimer t, const char* operation_name, const char* file, int line);
void TimerFlush(void);
double TimerElapsedUs(WinTimer t);
void TimerPrintRaw(double elapsed_us, const char* operation_name, const char* file, int line);

#define PROFILE_FUNCTION() WinTimer _profile_timer = TimerStart()
#define PROFILE_START(var) WinTimer var = TimerStart()
#define PROFILE_END_VAR(var, name) TimerEnd(var, name, __FILE__, __LINE__)
#define PROFILE_SCOPE(name) WinTimer _profile_timer_##__LINE__ = TimerStart(); \
  __attribute__((cleanup(_profile_timer_cleanup))) WinTimer* _profile_ptr_##__LINE__ = &_profile_timer_##__LINE__; \
  const char* _profile_name_##__LINE__ = (name)
static inline void _profile_timer_cleanup(WinTimer* t) {
  (void)t;
}

#endif
