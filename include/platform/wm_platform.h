#ifndef WM_PLATFORM_H
#define WM_PLATFORM_H

#include "core/window_manager.h"
#include "core/types.h"

bool Platform_WatcherStart(WindowManager* wm);
void Platform_WatcherStop(WindowManager* wm);

bool Platform_WM_InitDisplay(WindowManager* wm);
int Platform_WM_Run(WindowManager* wm);
void Platform_WM_Quit(WindowManager* wm);
void Platform_WM_ProcessEvents(WindowManager* wm);
void Platform_WM_OnPopEmpty(WindowManager* wm);
void Platform_WM_GetMonitorGeometry(int *x, int *y, int *w, int *h);
char* Platform_WM_ShowPrompt(WindowManager* wm, const PromptSpec* spec);
char* Platform_WM_ShowSimplePrompt(WindowManager* wm, const SimplePromptSpec* spec);

#endif
