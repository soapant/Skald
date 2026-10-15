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

static inline WinTimer TimerStart(void) {
  WinTimer t;
#if defined(_WIN32)
  QueryPerformanceFrequency(&t.frequency);
  QueryPerformanceCounter(&t.start);
#else
  clock_gettime(CLOCK_MONOTONIC, &t.start);
#endif
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
