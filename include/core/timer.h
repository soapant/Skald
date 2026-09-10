#ifndef CORE_TIMER_H
#define CORE_TIMER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif
#include "core/types.h"
#include "config.h"

#define TIMER_LOG_CAP 65536

extern char g_timer_buffer[TIMER_LOG_CAP];
extern size_t g_timer_buffer_len;
extern int g_timer_atexit_registered;
extern int g_timer_file_initialized;

void TimerBufferFlush(void);
void TimerBufferAppend(const char* line);

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

void TimerEnd(WinTimer t, const char* operation_name);

// Lightweight profiling helpers to reduce boilerplate.
// Usage: PROFILE_SCOPE("my_op"); or manual PROFILE_FUNCTION()/PROFILE_END("name")
#define PROFILE_FUNCTION() WinTimer _profile_timer = TimerStart()
#define PROFILE_END(name) TimerEnd(_profile_timer, (name))
#define PROFILE_SCOPE(name) WinTimer _profile_timer_##__LINE__ = TimerStart(); \
  __attribute__((cleanup(_profile_timer_cleanup))) WinTimer* _profile_ptr_##__LINE__ = &_profile_timer_##__LINE__; \
  const char* _profile_name_##__LINE__ = (name)
static inline void _profile_timer_cleanup(WinTimer* t) {
  // Note: caller still needs name; this helper is for future RAII use if desired.
  (void)t;
}

#endif
