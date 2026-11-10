#include <windows.h>
#include <stdbool.h>

#include "helpers/helpers_render_thread.h"
#include "deps/log.h"

static HANDLE g_rt_thread = NULL;
static CRITICAL_SECTION g_rt_lock;
static CONDITION_VARIABLE g_rt_cond;
static volatile LONG g_rt_lock_ready = 0;

void RenderThread_InitBackend(void) {}

static void rt_ensure_lock(void) {
  if (g_rt_lock_ready == 2)
    return;
  if (InterlockedCompareExchange(&g_rt_lock_ready, 1, 0) == 0) {
    InitializeCriticalSection(&g_rt_lock);
    InitializeConditionVariable(&g_rt_cond);
    InterlockedExchange(&g_rt_lock_ready, 2);
    return;
  }
  while (InterlockedCompareExchange(&g_rt_lock_ready, 2, 2) != 2)
    Sleep(0);
}

void RenderThread_LockBackend(void) {
  rt_ensure_lock();
  EnterCriticalSection(&g_rt_lock);
}

void RenderThread_UnlockBackend(void) { LeaveCriticalSection(&g_rt_lock); }

void RenderThread_WakeOneBackend(void) { WakeConditionVariable(&g_rt_cond); }

void RenderThread_WakeAllBackend(void) { WakeAllConditionVariable(&g_rt_cond); }

void RenderThread_WaitTimedBackend(long us) {
  SleepConditionVariableCS(&g_rt_cond, &g_rt_lock, (DWORD)(us / 1000));
}

void RenderThread_MiniSleepBackend(int milliseconds) {
  Sleep((DWORD)milliseconds);
}

static DWORD WINAPI rt_entry(LPVOID arg) {
  (void)arg;
  RenderThread_Main();
  return 0;
}

bool RenderThread_StartBackend(void) {
  rt_ensure_lock();
  g_rt_thread = CreateThread(NULL, 0, rt_entry, NULL, 0, NULL);
  if (!g_rt_thread) {
    log_warn("render thread: CreateThread failed, widget frames disabled");
    return false;
  }
  return true;
}

void RenderThread_JoinBackend(void) {
  if (!g_rt_thread)
    return;
  WaitForSingleObject(g_rt_thread, INFINITE);
  CloseHandle(g_rt_thread);
  g_rt_thread = NULL;
}

void *RenderThread_DisplayBackend(void) { return NULL; }
