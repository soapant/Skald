#include <stdbool.h>

#ifndef HELPERS_RENDER_THREAD_H
#define HELPERS_RENDER_THREAD_H

typedef void (*RenderFrameFn)(void *ud);

bool RenderThread_EnsureStarted(int fps);
void RenderThread_Stop(void);
bool RenderThread_IsRunning(void);
bool RenderThread_AddFrameCallback(RenderFrameFn fn, void *ud);
bool RenderThread_RemoveFrameCallback(RenderFrameFn fn, const void *ud);
bool RenderThread_Post(RenderFrameFn fn, void *ud);
void RenderThread_Wake(void);
void RenderThread_Sync(void);
bool RenderThread_IsCurrentThread(void);
void *RenderThread_Display(void);

void RenderThread_Main(void);

#endif
