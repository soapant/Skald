#include <stdbool.h>

#ifndef HELPERS_RENDER_THREAD_H
#define HELPERS_RENDER_THREAD_H

typedef void (*RenderFrameFn)(void *ud);

bool RenderThread_EnsureStarted(int fps);
void RenderThread_Stop(void);
bool RenderThread_IsRunning(void);
int RenderThread_CallbackCount(void);
bool RenderThread_AddFrameCallback(RenderFrameFn fn, void *ud);
bool RenderThread_RemoveFrameCallback(RenderFrameFn fn, void *ud);
bool RenderThread_Post(RenderFrameFn fn, void *ud);
void RenderThread_Wake(void);
void RenderThread_Sync(void);
bool RenderThread_IsCurrentThread(void);
void *RenderThread_Display(void);

void RenderThread_Main(void);
void RenderThread_MiniSleepBackend(int milliseconds);
void RenderThread_InitBackend(void);
void RenderThread_LockBackend(void);
void RenderThread_UnlockBackend(void);
void RenderThread_WakeOneBackend(void);
void RenderThread_WakeAllBackend(void);
void RenderThread_WaitTimedBackend(long us);
bool RenderThread_StartBackend(void);
void RenderThread_JoinBackend(void);
void *RenderThread_DisplayBackend(void);

#endif
