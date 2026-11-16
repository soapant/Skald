#include <windows.h>
#include <stdbool.h>

#include "helpers/helpers_render_thread.h"
#include "platform/platform.h"
#include "deps/log.h"

static HANDLE rt_thread = NULL;
static CRITICAL_SECTION rt_lock;
static CONDITION_VARIABLE rt_cond;
static volatile LONG rt_lock_ready = 0;

void Platform_RenderThreadInit(void) {}

static void rt_ensure_lock(void) {
  if (rt_lock_ready == 2)
    return;
  if (InterlockedCompareExchange(&rt_lock_ready, 1, 0) == 0) {
    InitializeCriticalSection(&rt_lock);
    InitializeConditionVariable(&rt_cond);
    InterlockedExchange(&rt_lock_ready, 2);
    return;
  }
  while (InterlockedCompareExchange(&rt_lock_ready, 2, 2) != 2)
    Sleep(0);
}

void Platform_RenderThreadLock(void) {
  rt_ensure_lock();
  EnterCriticalSection(&rt_lock);
}

void Platform_RenderThreadUnlock(void) { LeaveCriticalSection(&rt_lock); }

void Platform_RenderThreadWakeOne(void) { WakeConditionVariable(&rt_cond); }

void Platform_RenderThreadWakeAll(void) { WakeAllConditionVariable(&rt_cond); }

void Platform_RenderThreadWaitTimed(long us) {
  SleepConditionVariableCS(&rt_cond, &rt_lock, (DWORD)(us / 1000));
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
  rt_thread = CreateThread(NULL, 0, rt_entry, NULL, 0, NULL);
  if (!rt_thread) {
    log_warn("render thread: CreateThread failed, widget frames disabled");
    return false;
  }
  return true;
}

void Platform_RenderThreadJoin(void) {
  if (!rt_thread)
    return;
  WaitForSingleObject(rt_thread, INFINITE);
  CloseHandle(rt_thread);
  rt_thread = NULL;
}

void *Platform_RenderThreadDisplay(void) { return NULL; }
