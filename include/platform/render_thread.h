#ifndef PLATFORM_RENDER_THREAD_H
#define PLATFORM_RENDER_THREAD_H

#include <stdbool.h>

typedef void (*Platform_RenderFrameFn)(void *ud);

bool Platform_RenderThread_EnsureStarted(int fps);
void Platform_RenderThread_Stop(void);
bool Platform_RenderThread_IsRunning(void);
int Platform_RenderThread_CallbackCount(void);
bool Platform_RenderThread_AddFrameCallback(Platform_RenderFrameFn fn, void *ud);
bool Platform_RenderThread_RemoveFrameCallback(Platform_RenderFrameFn fn, void *ud);
bool Platform_RenderThread_Post(Platform_RenderFrameFn fn, void *ud);
void Platform_RenderThread_Wake(void);
void Platform_RenderThread_Sync(void);
bool Platform_RenderThread_IsCurrentThread(void);

void *Platform_RenderThread_Display(void);

#endif
