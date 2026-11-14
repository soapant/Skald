#include <windows.h>
#include <stdbool.h>

#include "helpers/helpers_render_thread.h"
#include "platform/platform.h"
#include "deps/log.h"

static HANDLE g_rt_thread = NULL;
static CRITICAL_SECTION g_rt_lock;
static CONDITION_VARIABLE g_rt_cond;
static volatile LONG rt_lock_ready = 0;

void Platform_RenderThreadInit(void) {}

static void rt_ensure_lock(void) {
  if (rt_lock_ready == 2)
    return;
  if (InterlockedCompareExchange(&rt_lock_ready, 1, 0) == 0) {
    InitializeCriticalSection(&g_rt_lock);
    InitializeConditionVariable(&g_rt_cond);
    InterlockedExchange(&rt_lock_ready, 2);
    return;
  }
  while (InterlockedCompareExchange(&rt_lock_ready, 2, 2) != 2)
    Sleep(0);
}

void Platform_RenderThreadLock(void) {
  rt_ensure_lock();
  EnterCriticalSection(&g_rt_lock);
}

void Platform_RenderThreadUnlock(void) { LeaveCriticalSection(&g_rt_lock); }

void Platform_RenderThreadWakeOne(void) { WakeConditionVariable(&g_rt_cond); }

void Platform_RenderThreadWakeAll(void) {
  WakeAllConditionVariable(&g_rt_cond);
}

void Platform_RenderThreadWaitTimed(long us) {
  SleepConditionVariableCS(&g_rt_cond, &g_rt_lock, (DWORD)(us / 1000));
}

void Platform_RenderThreadMiniSleep(int milliseconds) {
  Sleep((DWORD)milliseconds);
}

static DWORD WINAPI rt_entry(LPVOID arg) {
  (void)arg;
  render_thread_main();
  return 0;
}

bool Platform_RenderThreadStart(void) {
  rt_ensure_lock();
  g_rt_thread = CreateThread(NULL, 0, rt_entry, NULL, 0, NULL);
  if (!g_rt_thread) {
    log_warn("render thread: CreateThread failed, widget frames disabled");
    return false;
  }
  return true;
}

void Platform_RenderThreadJoin(void) {
  if (!g_rt_thread)
    return;
  WaitForSingleObject(g_rt_thread, INFINITE);
  CloseHandle(g_rt_thread);
  g_rt_thread = NULL;
}

void *Platform_RenderThreadDisplay(void) { return NULL; }
